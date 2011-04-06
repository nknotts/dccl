// copyright 2011 t. schneider tes@mit.edu
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


#include "goby/acomms/dccl.h"
#include "test.pb.h"
#include "goby/util/string.h"
#include "goby/util/time.h"

using goby::acomms::operator<<;
using goby::acomms::operator+;

class CustomCodec : public goby::acomms::DCCLFixedFieldCodec
{
private:
    unsigned _size()
        {
            return (part() == HEAD) ? 0 : A_SIZE + B_SIZE;                
        }
    
    Bitset _encode(const boost::any& field_value)
        {
            if(part() == HEAD)
            {
                return Bitset();
            }
            else
            {
                const CustomMsg& msg = boost::any_cast<const CustomMsg&>(field_value);
                
                Bitset a(A_SIZE, static_cast<unsigned long>(msg.a()));
                Bitset b(B_SIZE, static_cast<unsigned long>(msg.b()));
                
                std::cout << "a: " << a << std::endl;
                std::cout << "b: " << b  << std::endl;
                
                return a + b;
            }
        }
    
    
    boost::any _decode(Bitset* bits)
        {
            if(part() == HEAD)
            {
                return boost::any();
            }
            else
            {
                Bitset a = *bits;
                a.resize(A_SIZE);
                Bitset b = *bits;
                b >>= A_SIZE;
                b.resize(B_SIZE);
                
                CustomMsg msg;
                msg.set_a(a.to_ulong());
                msg.set_b(b.to_ulong());
                return msg;
            }
        }
    
    
    void _validate()
        {
        }

    enum { A_SIZE = 32 };
    enum { B_SIZE = 1 };
};    


int main()
{
    goby::acomms::DCCLCodec::set_log(&std::cerr);    

    goby::acomms::FieldCodecManager::add<CustomMsg, CustomCodec>("custom_codec");


    goby::acomms::protobuf::DCCLConfig cfg;
    cfg.set_crypto_passphrase("my_passphrase!");
    goby::acomms::DCCLCodec::set_cfg(cfg);

    CustomMsg msg_in1;

    msg_in1.set_a(10);
    msg_in1.set_b(true);

    goby::acomms::DCCLCodec::info(msg_in1.GetDescriptor(), &std::cout);    
    std::cout << "Message in:\n" << msg_in1.DebugString() << std::endl;
    assert(goby::acomms::DCCLCodec::validate(msg_in1.GetDescriptor()));
    std::cout << "Try encode..." << std::endl;
    std::string bytes1 = goby::acomms::DCCLCodec::encode(msg_in1);
    std::cout << "... got bytes (hex): " << goby::acomms::hex_encode(bytes1) << std::endl;
    std::cout << "Try decode..." << std::endl;
    boost::shared_ptr<google::protobuf::Message> msg_out1 = goby::acomms::DCCLCodec::decode(bytes1);
    std::cout << "... got Message out:\n" << msg_out1->DebugString() << std::endl;
    assert(msg_in1.SerializeAsString() == msg_out1->SerializeAsString());


    CustomMsg2 msg_in2;

    msg_in2.mutable_msg()->set_a(10);
    msg_in2.mutable_msg()->set_b(true);

    goby::acomms::DCCLCodec::info(msg_in2.GetDescriptor(), &std::cout);    
    std::cout << "Message in:\n" << msg_in2.DebugString() << std::endl;
    assert(goby::acomms::DCCLCodec::validate(msg_in2.GetDescriptor()));
    std::cout << "Try encode..." << std::endl;
    std::string bytes2 = goby::acomms::DCCLCodec::encode(msg_in2);
    std::cout << "... got bytes (hex): " << goby::acomms::hex_encode(bytes2) << std::endl;
    std::cout << "Try decode..." << std::endl;
    boost::shared_ptr<google::protobuf::Message> msg_out2 = goby::acomms::DCCLCodec::decode(bytes2);
    std::cout << "... got Message out:\n" << msg_out2->DebugString() << std::endl;
    assert(msg_in2.SerializeAsString() == msg_out2->SerializeAsString());

    std::cout << "all tests passed" << std::endl;
}

