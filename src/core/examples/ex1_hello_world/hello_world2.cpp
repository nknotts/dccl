#include "goby/core/core.h"
#include "hello_world.pb.h"

using goby::core::operator<<;

class HelloWorld2 : public goby::core::ApplicationBase
{
public:
    HelloWorld2()
        {
            // subscribe for all messages of type HelloWorldMsg
            subscribe<HelloWorldMsg>(&HelloWorld2::receive_msg, this);
        }

private:
    void receive_msg(const HelloWorldMsg& msg)
        {
            // print to the log the newest received "HelloWorldMsg"
            glogger() << "received: " << msg << std::endl;
        }
};

int main(int argc, char* argv[])
{   
    return goby::run<HelloWorld2>(argc, argv);
}
