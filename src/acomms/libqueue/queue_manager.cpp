// copyright 2009 t. schneider tes@mit.edu 
//
// this file is part of the Queue Library (libqueue),
// the goby-acomms message queue manager. goby-acomms is a collection of 
// libraries for acoustic underwater networking
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This software is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this software.  If not, see <http://www.gnu.org/licenses/>.

#include <map>
#include <deque>

#include <boost/foreach.hpp>

#include "goby/acomms/xml/xml_parser.h"
#include "goby/util/logger.h"
#include "goby/util/time.h"
#include "goby/util/binary.h"

#include "queue_constants.h"
#include "queue_manager.h"
#include "queue_xml_callbacks.h"

int goby::acomms::QueueManager::modem_id_ = 0;

goby::acomms::QueueManager::QueueManager(std::ostream* log /* =0 */)
    : log_(log),
      packet_ack_(0)
{}
    
goby::acomms::QueueManager::QueueManager(const std::string& file, const std::string schema, std::ostream* log /* =0 */)
    : log_(log),
      packet_ack_(0)

{ add_xml_queue_file(file, schema); }
    
goby::acomms::QueueManager::QueueManager(const std::set<std::string>& files,
                                         const std::string schema, std::ostream* log /* =0 */)
    : log_(log),
      packet_ack_(0)
{
    BOOST_FOREACH(const std::string& s, files)
        add_xml_queue_file(s, schema);
}

goby::acomms::QueueManager::QueueManager(const QueueConfig& cfg, std::ostream* log /* =0 */)
    : log_(log),
      packet_ack_(0)
{ add_queue(cfg); }

goby::acomms::QueueManager::QueueManager(const std::set<QueueConfig>& cfgs, std::ostream* log /* =0 */)
    : log_(log),
      packet_ack_(0)
{
    BOOST_FOREACH(const QueueConfig& c, cfgs)
        add_queue(c);    
}

void goby::acomms::QueueManager::add_queue(const QueueConfig& cfg)
{
    QueueKey k(cfg.type(), cfg.id());
    
    Queue q(cfg, log_, modem_id_);
    
    if(queues_.count(k))
    {
        std::stringstream ss;
        ss << "Queue: duplicate key specified for key: " << k;
        throw queue_exception(ss.str());
    }
    else if((q.cfg().id() > MAX_ID || q.cfg().id() < MIN_ID) && q.cfg().type() == queue_dccl)
    {
        std::stringstream ss;
        ss << "Queue: key (" << k << ") is out of bounds for use with libqueue. Use a id from " << MIN_ID << " to " << MAX_ID;
        throw queue_exception(ss.str());
    }
    else
        queues_.insert(std::pair<QueueKey, Queue>(k, q));

    if(log_) *log_<< group("q_out") << "added new queue: \n" << q << std::endl;
    
}

void goby::acomms::QueueManager::add_xml_queue_file(const std::string& xml_file,
                                                    const std::string xml_schema)
{
    std::vector<QueueConfig> cfgs;
    
    // Register handlers for XML parsing
    QueueContentHandler content(cfgs);
    QueueErrorHandler error;
    // instantiate a parser for the xml message files
    XMLParser parser(content, error);
    // parse(file, [schema])
    if(xml_schema != "")
        xml_schema_ = xml_schema;
        
    parser.parse(xml_file, xml_schema_);

    BOOST_FOREACH(const QueueConfig& c, cfgs)
        add_queue(c);
}

void goby::acomms::QueueManager::do_work()
{
    typedef std::pair<const QueueKey, Queue> P;
    for(std::map<QueueKey, Queue>::iterator it = queues_.begin(), n = queues_.end(); it != n; ++it)
    {
        std::vector<protobuf::ModemDataTransmission> expired_msgs = it->second.expire();
        if(callback_expire)
        {
            protobuf::ModemDataExpire expire;
            protobuf::ModemDataTransmission* data_msg = expire.mutable_orig_msg();
            BOOST_FOREACH(*data_msg, expired_msgs)
                callback_expire(it->first, expire);
        }
    }
    
}

void goby::acomms::QueueManager::push_message(QueueKey key, const protobuf::ModemDataTransmission& data_msg)
{
    // message is to us, auto-loopback
    if(data_msg.base().dest() == modem_id_)
    {
        if(log_) *log_<< group("q_out") << "outgoing message is for us: using loopback, not physical interface" << std::endl;
        
        handle_modem_receive(data_msg);
    }
    // we have a queue with this key, so push message for sending
    else if(queues_.count(key))
    {
        queues_[key].push_message(data_msg);
        qsize(&queues_[key]);
    }
    else
    {
        std::stringstream ss;
        ss << "no queue for key: " << key;
        throw queue_exception(ss.str());
    }    
}

void goby::acomms::QueueManager::push_message(unsigned id, const protobuf::ModemDataTransmission& data_msg, QueueType type /* = dccl_queue */)
{ push_message(QueueKey(type, id), data_msg); }

void goby::acomms::QueueManager::set_on_demand(QueueKey key)
{
    if(queues_.count(key))
        queues_[key].set_on_demand(true);
    else
    {
        std::stringstream ss;
        ss << "no queue for key: " << key;
        throw queue_exception(ss.str());
    }
}

void goby::acomms::QueueManager::set_on_demand(unsigned id, QueueType type /* = dccl_queue */)
{ set_on_demand(QueueKey(type, id)); }


std::string goby::acomms::QueueManager::summary() const
{
    std::string s;
    typedef std::pair<const QueueKey, Queue> P;
    BOOST_FOREACH(const P& p, queues_)
        s += p.second.summary();

    return s;
}

    
std::ostream& goby::acomms::operator<< (std::ostream& out, const QueueManager& d)
{
    out << d.summary();
    return out;
}

// finds and publishes outgoing data for the modem driver
// first query every Queue for its priority data using
// priority_values(priority, last_send_time)
// priority_values returns false if that object has no data to give
// (either no data at all, or in blackout interval) 
// thus, from all the priority values that return true, pick the one with the lowest
// priority value, or given a tie, pick the one with the oldest last_send_time
void goby::acomms::QueueManager::handle_modem_data_request(const protobuf::ModemDataRequest& request_msg, protobuf::ModemDataTransmission* data_msg)
{
    if(request_msg.frame() == 0)
        clear_packet();
    else // discipline remaining frames to the first frame ack value
        data_msg->set_ack_requested(packet_ack_);
    
    // first (0th) user-frame
    Queue* winning_queue = find_next_sender(request_msg, *data_msg, true);
    
    // no data at all for this frame ... :(
    if(!winning_queue)
    {
        data_msg->set_ack_requested(packet_ack_);
        if(log_) *log_<< group("q_out") << "no data found. sending blank to firmware" 
                      << ": " << *data_msg << std::endl; 
    }
    else
    {
        stitch_recursive(request_msg, data_msg, winning_queue);
    }
}

bool goby::acomms::QueueManager::stitch_recursive(const protobuf::ModemDataRequest& request_msg, protobuf::ModemDataTransmission* complete_data_msg, Queue* winning_queue)
{
    const unsigned CCL_ID_BYTES = head_ccl_id_size / BITS_IN_BYTE;
    
    // new user frame (e.g. 32B)
    const protobuf::ModemDataTransmission& next_data_msg = winning_queue->give_data(request_msg);

    // discipline the destination of the packet if initially unset
    if(complete_data_msg->base().dest() == QUERY_DESTINATION_ID)
        complete_data_msg->mutable_base()->set_dest(next_data_msg.base().dest());
    
    // message should never be empty
    if(next_data_msg.data().empty()) throw (queue_exception("empty message!"));

    if(log_) *log_<< group("q_out") << "sending data to firmware from: "
                  << winning_queue->cfg().name() 
                  << ": " << next_data_msg << std::endl;
    
    // if an ack been set, do not unset these
    if (packet_ack_ == false) packet_ack_ = next_data_msg.ack_requested();    
    
    // insert ack if desired
    if(next_data_msg.ack_requested())
        waiting_for_ack_.insert(std::pair<unsigned, Queue*>(complete_data_msg->frame(), winning_queue));
    else
    {
        winning_queue->pop_message(complete_data_msg->frame());
        qsize(winning_queue); // notify change in queue size
    }    

    // e.g. 32B
    std::string new_data = next_data_msg.data();
    
    // insert the size of the next field (e.g. 33B) right after the header
    std::string frame_size(USER_FRAME_NEXT_SIZE_BYTES,
                           (next_data_msg.data().size()-DCCL_NUM_HEADER_BYTES));
    new_data.insert(DCCL_NUM_HEADER_BYTES, frame_size);
    
    // append without the CCL ID (old size + 32B)
    *(complete_data_msg->mutable_data()) += new_data.substr(CCL_ID_BYTES);
    
    bool is_last_user_frame = true;
    // if true, we may be able to add more user-frames to this message
    if((request_msg.max_bytes() - complete_data_msg->data().size()) > DCCL_NUM_HEADER_BYTES
       && winning_queue->cfg().type() != queue_ccl)
    {
        winning_queue = find_next_sender(request_msg, *complete_data_msg, false);
        if(winning_queue) is_last_user_frame = false;
    }

    if(!is_last_user_frame)
    {
        replace_header(false, complete_data_msg, next_data_msg, new_data);
        return stitch_recursive(request_msg, complete_data_msg, winning_queue);
    }
    else
    {
        replace_header(true, complete_data_msg, next_data_msg, new_data);
        // add the CCL ID back on to the message (e.g. 33B)
        complete_data_msg->mutable_data()->insert(0, std::string(1, DCCL_CCL_HEADER));
        // remove the size of the next field from the last user-frame (e.g. 32B).
        complete_data_msg->mutable_data()->erase(complete_data_msg->data().size()-new_data.size()+DCCL_NUM_HEADER_BYTES, USER_FRAME_NEXT_SIZE_BYTES);
        // set the ack to conform to the entire message
        complete_data_msg->set_ack_requested(packet_ack_);
        
        return true;
    }
}

void goby::acomms::QueueManager::replace_header(bool is_last_user_frame, protobuf::ModemDataTransmission* data_msg, const protobuf::ModemDataTransmission& next_data_msg, const std::string& new_data)
{
    // decode the header so that we can modify the flags
    DCCLHeaderDecoder head_decoder(new_data);

    // don't put the multimessage flag on the last user-frame
    head_decoder[head_multimessage_flag] =
        (!is_last_user_frame) ? true : false;
    // put the broadcast flag on if needed 
    head_decoder[head_broadcast_flag] =
        (next_data_msg.base().dest() == BROADCAST_ID) ? true : false;

    // re-encode the header
    DCCLHeaderEncoder head_encoder(head_decoder.get());

    // replace the header without the CCL ID
    const unsigned CCL_ID_BYTES = head_ccl_id_size / BITS_IN_BYTE;
    data_msg->mutable_data()->replace(data_msg->data().size()-new_data.size()+CCL_ID_BYTES,
                                      head_encoder.str().size()-CCL_ID_BYTES,
                                      head_encoder.str().substr(CCL_ID_BYTES));
}


void goby::acomms::QueueManager::clear_packet()
{
    typedef std::pair<unsigned, Queue*> P;
    BOOST_FOREACH(const P& p, waiting_for_ack_)
        p.second->clear_ack_queue();
    
    waiting_for_ack_.clear();
    
    packet_ack_ = 0;
}



goby::acomms::Queue* goby::acomms::QueueManager::find_next_sender(const protobuf::ModemDataRequest& request_msg, const protobuf::ModemDataTransmission& data_msg, bool first_user_frame)
{   
// competition between variable about who gets to send
    double winning_priority;
    boost::posix_time::ptime winning_last_send_time;

    Queue* winning_queue = 0;
    
    if(log_) *log_<< group("priority") << "starting priority contest"
                  << "... have " << data_msg 
                  << "requesting: " << request_msg << std::endl;
    
    for(std::map<QueueKey, Queue>::iterator it = queues_.begin(), n = queues_.end(); it != n; ++it)
    {
        Queue& oq = it->second;
        
        // encode on demand
        if(oq.on_demand() &&
           (!oq.size() || oq.newest_msg_time() + ON_DEMAND_SKEW < util::goby_time())
            )
        {
            if(callback_ondemand)
            {
                protobuf::ModemDataTransmission data_msg;
                callback_ondemand(it->first, request_msg, &data_msg);
                push_message(it->first, data_msg);
            }
        }
        
        double priority;
        boost::posix_time::ptime last_send_time;
        if(oq.priority_values(priority, last_send_time, request_msg, data_msg))
        {
            // no winner, better winner, or equal & older winner
            // AND not CCL when not the first user-frame
            if((!winning_queue || priority > winning_priority ||
                (priority == winning_priority && last_send_time < winning_last_send_time))
               && !(oq.cfg().type() == queue_ccl && !first_user_frame))
            {
                winning_priority = priority;
                winning_last_send_time = last_send_time;
                winning_queue = &oq;
            }            if(log_) *log_<< group("priority") << "\t" << oq.cfg().name()
                          << " has priority value"
                          << ": " << priority << std::endl;
        }
    }

    if(log_) *log_<< group("priority") << "\t"
                  << "all other queues have no messages" << std::endl;

    if(winning_queue)
    {
        if(log_) *log_<< group("priority") << winning_queue->cfg().name()
                      << " has highest priority." << std::endl;
    }
    
    return winning_queue;
}    


void goby::acomms::QueueManager::handle_modem_ack(const protobuf::ModemDataAck& orig_ack_msg)
{
    protobuf::ModemDataAck ack_msg(orig_ack_msg);
    
    if(ack_msg.base().dest() != modem_id_)
    {
        if(log_) *log_<< group("q_in") << warn
                      << "ignoring ack for modem_id = " << ack_msg.base().dest() << std::endl;
        return;
    }
    else if(!waiting_for_ack_.count(ack_msg.frame()))
    {
        if(log_) *log_<< group("q_in")
                      << "got ack but we were not expecting one" << std::endl;
        return;
    }
    else
    {
        
        // got an ack, let's pop this!
        if(log_) *log_<< group("q_in") << "received ack for this id" << std::endl;
        
        std::multimap<unsigned, Queue *>::iterator it = waiting_for_ack_.find(ack_msg.frame());
        while(it != waiting_for_ack_.end())
        {
            Queue* oq = it->second;

            protobuf::ModemDataTransmission* removed_msg = ack_msg.mutable_orig_msg();
            if(!oq->pop_message_ack(ack_msg.frame(), removed_msg))
            {
                if(log_) *log_<< group("q_in") << warn
                              << "failed to pop message from "
                              << oq->cfg().name() << std::endl;
            }
            else
            {
                qsize(oq);
                if(callback_ack)
                    callback_ack(QueueKey(oq->cfg().type(), oq->cfg().id()), ack_msg);
            }
            
            waiting_for_ack_.erase(it);
            
            it = waiting_for_ack_.find(ack_msg.frame());
        }
    }
    
    return;    
}


// parses and publishes incoming data
// by matching the variableID field with the variable specified
// in a "receive = " line of the configuration file
void goby::acomms::QueueManager::handle_modem_receive(const protobuf::ModemDataTransmission& message)
{
    if(log_) *log_<< group("q_in") << "received message"
                  << ": " << message << std::endl;

    std::string data = message.data();
    if(data.size() < DCCL_NUM_HEADER_BYTES)
        return;
    
    DCCLHeaderDecoder head_decoder(data);
    int ccl_id = head_decoder[head_ccl_id];

    // check for queue_dccl type
    if(ccl_id == DCCL_CCL_HEADER)
    {
        protobuf::ModemDataTransmission mod_message = message;
        unstitch_recursive(&data, &mod_message);
    }
    // check for ccl type
    else
    {
        QueueKey key(queue_ccl, ccl_id);
        std::map<QueueKey, Queue>::iterator it = queues_.find(key);
        
        if (it != queues_.end())
        {
            if(callback_receive_ccl) callback_receive_ccl(key, message);
        }
        else
        {
            if(log_) *log_<< group("q_in") << warn << "incoming data string is not for us (not DCCL or known CCL)." << std::endl;
        }
    }
}

bool goby::acomms::QueueManager::unstitch_recursive(std::string* data, protobuf::ModemDataTransmission* data_msg)
{
    unsigned original_dest = data_msg->base().dest();
    DCCLHeaderDecoder head_decoder(*data);
    bool multimessage_flag = head_decoder[head_multimessage_flag];
    bool broadcast_flag = head_decoder[head_broadcast_flag];
    unsigned dccl_id = head_decoder[head_dccl_id];
    
    // test multimessage bit
    if(multimessage_flag)
    {
        // extract frame_size
        // TODO(tes): Make this work properly for strings larger than one byte
        unsigned frame_size = data->substr(DCCL_NUM_HEADER_BYTES, USER_FRAME_NEXT_SIZE_BYTES)[0];
        
        // erase the frame size byte
        data->erase(DCCL_NUM_HEADER_BYTES, USER_FRAME_NEXT_SIZE_BYTES);
        
        // extract the data for this user-frame
        data_msg->set_data(data->substr(0, (frame_size + DCCL_NUM_HEADER_BYTES)));
        
        data->erase(USER_FRAME_NEXT_SIZE_BYTES,
                    (frame_size + DCCL_NUM_HEADER_BYTES-USER_FRAME_NEXT_SIZE_BYTES));
    }
    else
    {
        data_msg->set_data(*data);
    }
    
    // reset these flags
    head_decoder[head_multimessage_flag] = false;
    head_decoder[head_broadcast_flag] = false;
    
    DCCLHeaderEncoder head_encoder(head_decoder.get());
    data_msg->mutable_data()->replace(0, DCCL_NUM_HEADER_BYTES, head_encoder.str());
    // overwrite destination as BROADCAST if broadcast bit is set
    data_msg->mutable_base()->set_dest(broadcast_flag ? BROADCAST_ID : original_dest);
    publish_incoming_piece(*data_msg, dccl_id); 
    
    // put the destination back
    data_msg->mutable_base()->set_dest(original_dest);

    // unstitch until the multimessage flag is no longer set
    return multimessage_flag ? unstitch_recursive(data, data_msg) : true;
}

bool goby::acomms::QueueManager::publish_incoming_piece(const protobuf::ModemDataTransmission& message, const unsigned incoming_var_id)
{
    if(message.base().dest() != BROADCAST_ID && message.base().dest() != modem_id_)
    {
        if(log_) *log_<< group("q_in") << warn << "ignoring message for modem_id = "
                      << message.base().dest() << std::endl;
        return false;
    }

    QueueKey dccl_key(queue_dccl, incoming_var_id);
    std::map<QueueKey, Queue>::iterator it_dccl = queues_.find(dccl_key);
    
    if(it_dccl == queues_.end())
    {
        if(log_) *log_<< group("q_in") << warn << "no mapping for this variable ID: "
                      << incoming_var_id << std::endl;
        return false;
    }

    if(callback_receive) callback_receive(dccl_key, message);    

    return true;
}

void goby::acomms::QueueManager::add_flex_groups(util::FlexOstream& tout)
{
    tout.add_group("push", util::Colors::lt_cyan, "stack push - outgoing messages (goby_queue)");
    tout.add_group("pop",  util::Colors::lt_green, "stack pop - outgoing messages (goby_queue)");
    tout.add_group("priority",  util::Colors::yellow, "priority contest (goby_queue)");
    tout.add_group("q_out",  util::Colors::cyan, "outgoing queuing messages (goby_queue)");
    tout.add_group("q_in",  util::Colors::green, "incoming queuing messages (goby_queue)");
}

