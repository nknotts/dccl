// copyright 2011 t. schneider tes@mit.edu
// 
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

#ifndef SUBSCRIPTION20110412H
#define SUBSCRIPTION20110412H

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/dynamic_message.h>



namespace goby
{
    namespace pb
    {
        // forms a non-template base for the Subscription class, allowing us
        // use a common pointer type.
        class SubscriptionBase
        {
          public:
            virtual void post(const void* data, int size) = 0;
            virtual const google::protobuf::Message& newest() const = 0;
            virtual const std::string& type_name() const = 0;
            virtual bool has_valid_handler() const = 0;
        };

        // forms the concept of a subscription to a given Google Protocol Buffers
        // type ProtoBufMessage
        // An instantiation of this is created for each call to ApplicationBase::subscribe()
        template<typename ProtoBufMessage>
            class Subscription : public SubscriptionBase
        {
          public:
            typedef boost::function<void (const ProtoBufMessage&)> HandlerType;

          Subscription(HandlerType& handler,
                       const std::string& type_name)
              : handler_(handler),
                type_name_(type_name)
                { }
            
            // handle an incoming message (serialized using the google::protobuf
            // library calls)
            void post(const void* data, int size)
            {
                static ProtoBufMessage msg;
                msg.ParseFromArray(data, size);
                newest_msg_ = msg;
                if(handler_) handler_(newest_msg_);

            }

            // getters
            const google::protobuf::Message& newest() const { return newest_msg_; }
            const std::string& type_name() const { return type_name_; }
            bool has_valid_handler() const { return handler_; }
            
            
          private:
            HandlerType handler_;
            ProtoBufMessage newest_msg_;
            const std::string type_name_;
        };
    }
}



#endif