#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <thread>
#include "client.hpp"
#include "server.hpp"

// This pragma makes sure visual studio knows that winsock is a required library.
// May not work on other IDEs

#pragma comment (lib, "ws2_32.lib")
#define DEFAULT_PORT "56001"

using namespace std;

/*
    For this to work on devices across the internet, i.e outside the LAN, you need to enable port forwarding
    in your router settings, and connect to the router IP on the specified port.
    However, this will work perfectly fine across computers inside the LAN, or with localhost (working computer)
*/

int main()
{

    // Get choice to start server/client from user;

    int choice;
    cout << "Start server(0)/client(1)?: ";
    cin >> choice;

    // Initialize WinSock.

    WSADATA wsadata;

    if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0) {
        cout << "Error in WSAStartup()" << endl;
        return 1;
    }

    // Server requires only port (defaulted to DEFAULT_PORT) 
    // client requires both server_ip (defaulted to localhost) and port

    string port;
    if (!choice) {

        int conns;

        ThreadedServer server = ThreadedServer();
        cout << "Enter PORT <leave 0 for default port (56001)>: ";
        cin >> port;
        cout << "Enter max accepted clients <greater than 0>: ";
        cin >> conns;

        // Initializes server and starts listening for incoming connections.

        if (port == "0") port = DEFAULT_PORT;
        if (server.create_tcp_socket(port.c_str())) {
            server.listen_and_accept(conns);
        }
    }
    else {
        ThreadedClient client = ThreadedClient();
        string ip;
        cout << "Enter IP <localhost/127.0.0.1/leave 0 for working computer>: ";
        cin >> ip;
        if (ip == "0") ip = "localhost";
        cout << "Enter PORT <leave 0 for default port (56001)>: ";
        cin >> port;
        if (port == "0") port = DEFAULT_PORT;

        // Initializes client and connects to given IP and port.

        if (client.create_tcp_socket(ip.c_str(), port.c_str())) {
            client.start_chat();
        }
    }

    cout << "Cleanup started." << endl;
    WSACleanup();

    return 0;
}