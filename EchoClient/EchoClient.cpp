#include "Socket.h"
using namespace TinyNet;

#pragma comment(lib, "TinyNet.lib")

SocketManager& g_Manager = SocketManager::Instance();
bool g_Closed = false;

class EchoClientHandler : public SocketHandler
{
public:
    void OnConnect(uint32_t name, bool status)
    {
        printf("%d OnConnected %s\n", name, status ? "success" : "fail");
    }

    void OnReceive(uint32_t name, PacketPtr& packet)
    {
        PacketReader reader(packet);

        std::string text;
        reader>>text;

        printf("%d OnReceive, %s\n", name, text.c_str());
    }

    void OnClose(uint32_t name)
    {
        g_Closed = true;
        printf("%d OnClosed\n", name);
    }
};

int main()
{
    g_Manager.Start();

    SocketHandlerPtr handler = SocketHandlerPtr(new EchoClientHandler);
    uint32_t socket = g_Manager.Connect("127.0.0.1", 1234, handler);

    char line[255];
    while (!g_Closed) {
        scanf_s("%s", line, 250);

        std::string text = line;

        PacketWriter writer(0,0);
        writer<<text;

        g_Manager.SendPacket(socket, writer.GetPacket());
    }
    g_Manager.Close();
    
    getchar();
    return 0;
}