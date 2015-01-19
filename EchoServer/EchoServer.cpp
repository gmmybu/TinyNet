#include "Socket.h"
using namespace TinyNet;

#pragma comment(lib, "TinyNet.lib")

SocketManager& g_Manager = SocketManager::Instance();

class EchoServerHandler : public SocketHandler
{
public:
    void OnConnect(uint32_t name, bool status)
    {
        std::string text("hello");
        PacketWriter writer(0, 0);
        writer<<text;

        g_Manager.SendPacket(name, writer.GetPacket());

        printf("%d OnConnect %s\n", name, status ? "success" : "fail");
    }

    void OnReceive(uint32_t name, PacketPtr& packet)
    {
        PacketReader reader(packet);
        
        std::string text;
        reader>>text;

        printf("%d OnReceive %s\n", name, text.c_str());

        if (text == "close") {
            g_Manager.CloseSocket(name);
            return;
        }

        for (size_t i = 0; i < text.size() / 2; i++) {
            size_t j = text.size() - i - 1;
            std::swap(text[i], text[j]);
        }

        PacketWriter writer(0, 0);
        writer<<text;

        g_Manager.SendPacket(name, writer.GetPacket());
    }

    void OnClose(uint32_t name)
    {
        printf("%d Disconnected\n", name);
    }
};

class EchoServerAcceptHandler : public SocketAcceptHandler
{
public:
    SocketHandlerPtr GetHandler(uint32_t name)
    {
        return SocketHandlerPtr(new EchoServerHandler);
    }

    void OnClose(uint32_t name)
    {
        printf("%d ServerClosed", name);
    }
};

uint32_t g_Accept;

int main()
{
    g_Manager.Start();
    
    SocketAcceptHandlerPtr acceptHandler = SocketAcceptHandlerPtr(new EchoServerAcceptHandler);
    g_Accept = g_Manager.Listen("127.0.0.1", 1234, acceptHandler);

    getchar();

    g_Manager.Close();

    return 0;
}