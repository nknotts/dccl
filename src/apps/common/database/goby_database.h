// copyright 2010-2011 t. schneider tes@mit.edu
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

#ifndef GOBYDATABASE20100914H
#define GOBYDATABASE20100914H

#include <map>
#include <boost/unordered_map.hpp>

#include "goby/common/zeromq_application_base.h"
#include "goby/pb/protobuf_node.h"
#include "goby/pb/dbo/dbo_manager.h"


#include "goby/pb/protobuf/database_request.pb.h"
#include "database_config.pb.h"
#include "goby/pb/protobuf_node.h"
#include "goby/common/pubsub_node_wrapper.h"

namespace goby
{
    namespace common
    {
        class Database : public ZeroMQApplicationBase
        {
          public:
            Database();
          private:
            void loop();
            
            void init_sql();
            std::string format_filename(const std::string& in);
            void handle_database_request(const protobuf::DatabaseRequest& proto_request);
            
          private:
            static protobuf::DatabaseConfig cfg_;

            ZeroMQService zeromq_service_;
            pb::StaticProtobufNode req_rep_protobuf_node_;
            PubSubNodeWrapperBase pubsub_node_;
            
            DBOManager* dbo_manager_;
            int last_unique_id_;

            enum { DATABASE_SERVER_SOCKET_ID = 103996 };
            enum { MAX_LOOP_FREQ = 1 };

            ProtobufDBOPlugin protobuf_plugin_;
        };
    }
}



#endif