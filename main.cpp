#include <iostream>
#include <fstream>
#include <sstream>
#include <conio.h>
#include <map>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
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


std::string get_option(std::string menu, std::pair<std::string, std::vector<std::string>> option) {
    int selected = 0;

    while (1) {
        system("cls");
        std::cout << menu;
        std::cout << option.first << ":\n";
        for (int i = 0; i < option.second.size(); i++) {
            std::cout << option.second[i];
            if (i == selected) std::cout << "  <<<\n";
            else std::cout << '\n';
        }

        int ch = getch();
        if (ch == 72 || ch == 80) {
            if (ch == 72) selected = ((--selected)%option.second.size()); // Up
            else selected = ((++selected)%option.second.size()) ; // Down
        }
        else if (ch == 13) { // Enter
            if (*(option.second[selected].end() - 1) == '*') {
                std::string value;
                std::cout << "Enter value: ";
                std::cin >> value;

                return value;
            }
            return option.second[selected];
        }
    }
}

static void modify_menu(std::ifstream &menufmt, std::string &MENU, std::map<std::string, std::string>&selectedoptions) {
    std::string temp;
    while (std::getline(menufmt, temp)) {
        for (int i = 0; i < temp.length(); i++) {
            if (temp[i] == '$') {
                std::pair<std::string, std::vector<std::string>> curopt;
                int opts;
                std::stringstream option(std::string(temp.begin() + i + 3, temp.end()));
                option >> opts;
                option.ignore(3);
                option >> curopt.first;

                while (opts--) {
                    std::string subopt;
                    std::getline(menufmt, subopt);
                    
                    curopt.second.push_back(subopt);
                }
                std::string chosen = get_option(MENU, curopt);

                size_t firstNonSpace = chosen.find_first_not_of(" ");
                size_t lastNonSpace = chosen.find_last_not_of(" ");

                if (firstNonSpace != std::string::npos && lastNonSpace != std::string::npos) {
                    chosen = chosen.substr(firstNonSpace, lastNonSpace - firstNonSpace + 1);
                }

                selectedoptions[curopt.first] = chosen;
                MENU += curopt.first + " : " + selectedoptions[curopt.first] + '\n';
                system("cls");
                break;
            }
            MENU += temp[i];
        }
        MENU += '\n';
    }
}

int main()
{
    std::ifstream menufmt("./menufmt.txt");
    std::string MENU;
    std::map<std::string, std::string> selectedoptions;
    
    modify_menu(menufmt, std::ref(MENU), std::ref(selectedoptions));

    menufmt.close();
    menufmt.clear();
    if (selectedoptions["START"] == "Threaded Server")    
        menufmt.open("./serverselect.txt");
    else if (selectedoptions["START"] == "IO Server") 
        menufmt.open("./serverselect.txt");
    else 
        menufmt.open("./" + selectedoptions["START"] + "select.txt");

    if (menufmt.is_open()) modify_menu(menufmt, std::ref(MENU), std::ref(selectedoptions));
    else throw("Error Encountered");

    // Initialize WinSock.

    WSADATA wsadata;

    if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0) {
        cout << "Error in WSAStartup()" << endl;
        return 1;
    }

    // Server requires only port (defaulted to DEFAULT_PORT) 
    // client requires both server_ip (defaulted to localhost) and port

    string port;
    if (selectedoptions["START"] == "Threaded Server") {

        ThreadedServer server = ThreadedServer();

        // Initializes server and starts listening for incoming connections.

        if (server.create_tcp_socket(selectedoptions["PORT"].c_str())) {
            server.listen_and_accept(std::stoi(selectedoptions["CLIENTS"]));
        }
    }
    else if (selectedoptions["START"] == "IO Server") {
        IOServer server = IOServer();

        if (server.create_tcp_socket(selectedoptions["PORT"].c_str())) {
            server.listen_and_accept(std::stoi(selectedoptions["CLIENTS"]));
        }
    }
    else {
        ThreadedClient client = ThreadedClient();

        // Initializes client and connects to given IP and port.

        if (client.create_tcp_socket(selectedoptions["IP_ADDRESS"].c_str(), selectedoptions["PORT"].c_str())) {
            client.start_chat();
        }
    }

    cout << "Cleanup started." << endl;
    WSACleanup();

    return 0;
}