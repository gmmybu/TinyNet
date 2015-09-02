#include "Socket.h"
using namespace TinyNet;

#pragma comment(lib, "TinyNet.lib")

class EchoClientHandler : public SocketHandler
{
public:
    void OnStart(uint32_t name, bool status)
    {
        printf("%d Connected %s\n", name, status ? "success" : "fail");
    }

    void OnReceive(uint32_t name, PacketPtr& packet)
    {
        PacketReader reader(packet);

        std::string text;
        reader>>text;

        int id;
        reader>>id;

      //  printf("%d OnReceive, %s %d\n", name, text.c_str(), id);

        PacketWriter writer(0, 0);
        writer<<text;
        writer<<(id + 1);

        theManager.Transfer(name, writer.GetPacket());
    }

    void OnClose(uint32_t name)
    {
        printf("%d OnClosed\n", name);
    }
};

int main()
{
    theManager.Start();

    SocketHandlerPtr handler = SocketHandlerPtr(new EchoClientHandler);
    
    for(int i = 0; i < 10; i++) {
        theManager.Create("127.0.0.1", 1234, handler);
    }

    getchar();

    theManager.Close();

    return 0;
}