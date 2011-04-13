// copyright 2010 t. schneider tes@mit.edu
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
// along with this software.  If not, see <http://www.gnu.org/licenses/>


#ifndef CoreConstants20100813H
#define CoreConstants20100813H

#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <google/protobuf/text_format.h>
#include "goby/util/time.h"

namespace goby
{
    namespace core
    {
        enum MarshallingScheme
        {
            MARSHALLING_UNKNOWN = 0,
            MARSHALLING_PROTOBUF = 1,
            MARSHALLING_CCL = 2,
            MARSHALLING_MOOS = 3,
            MARSHALLING_DCCL = 4,
            MARSHALLING_LCM = 5,
            MARSHALLING_MAX = 5
        };

        const int BITS_IN_UINT32 = 32;
        const int BITS_IN_BYTE = 8;


        
        //const unsigned MAX_MSG_BUFFER_SIZE = 1 << 19;
        //const unsigned MAX_NUM_MSG = 10;
        
        // see message_queue_util.h
        //const std::string QUEUE_PREFIX = "goby_";        
        /* const std::string CONNECT_LISTEN_QUEUE_PREFIX = QUEUE_PREFIX + "connect_listen_"; */
        /* const std::string CONNECT_RESPONSE_QUEUE_PREFIX = QUEUE_PREFIX + "connect_response_"; */
        /* const std::string TO_SERVER_QUEUE_PREFIX = QUEUE_PREFIX + "to_gobyd_from_"; */
        /* const std::string FROM_SERVER_QUEUE_PREFIX = QUEUE_PREFIX + "from_gobyd_to_"; */

        //inline std::string make_ipc_name(const std::string& self_name,
        //                            const std::string& type_name)
        //{ return "ipc://" + QUEUE_PREFIX + type_name + "_" + self_name; }

        
        
    }

    

}



#endif
