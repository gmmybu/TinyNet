#include "Socket.h"
using namespace TinyNet;

#pragma comment(lib, "TinyNet.lib")

int total = 0;

class EchoServerHandler : public SocketHandler
{
public:
    void OnStart(uint32_t name, bool status)
    {
        std::string text("hello   theManager.Transfer(name, writer.GetPacket());   theManager.Transfer(name, writer.GetPacket());   theManager.Transfer(name, writer.GetPacket());   theManager.Transfer(name, writer.GetPacket());");
        PacketWriter writer(0, 0);
        writer<<text;
        writer<<1;

        theManager.Transfer(name, writer.GetPacket());
    }

    void OnReceive(uint32_t name, PacketPtr& packet)
    {
        PacketReader reader(packet);

        const char* text = reader.ReadString();

        int id;
        reader>>id;

        PacketWriter writer(0, 0);
        writer.Write(text);
        writer<<(id + 1);

        total++;
        if (total % 100000 == 0) { printf("%d\n", GetTickCount()); }

        theManager.Transfer(name, writer.GetPacket());
    }

    void OnClose(uint32_t name)
    {
        printf("%d Disconnected\n", name);
    }
};

SocketHandlerPtr socketHanlder = SocketHandlerPtr(new EchoServerHandler);

class EchoServerAcceptHandler : public ServerHandler
{
public:
    SocketHandlerPtr OnAccept(uint32_t name)
    {
        return socketHanlder;
    }

    void OnClose(uint32_t name)
    {
        printf("%d ServerClosed", name);
    }
};

uint32_t g_Accept;

int main()
{
    theManager.Start();
    
    ServerHandlerPtr acceptHandler = ServerHandlerPtr(new EchoServerAcceptHandler);
    g_Accept = theManager.Listen("127.0.0.1", 1234, acceptHandler);

    getchar();

    theManager.Close();

    return 0;
}