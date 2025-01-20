//#include <iostream>
//#include <WinSock2.h>
//#include <WS2tcpip.h>
//#include <thread>
//
//#pragma comment (lib, "ws2_32.lib")
//#define DEFAULT_PORT "56001"
//#define DEFAULT_BUFLEN 512
//
//using namespace std;
//
//int init_res;
//
//int client() {
//    struct addrinfo* result = NULL, *ptr = NULL, hints;
//
//    ZeroMemory(&hints, sizeof(hints));
//    hints.ai_family = AF_INET;
//    hints.ai_socktype = SOCK_STREAM;
//    hints.ai_protocol = IPPROTO_TCP;
//
//    //return value is 0 if failed, uses `hints` to configure and get address info, `results` will be replaced with a linked list
//    //of addrinfo objects with ip addresses of servers with that name.
//    init_res = getaddrinfo("localhost", DEFAULT_PORT, &hints, &result);
//
//    if (init_res != 0) {
//        printf("getaddrinfo failed: %d\n", init_res);
//        WSACleanup();
//        return 1;
//    }
//
//    SOCKET connect_socket = INVALID_SOCKET;
//
//    ptr = result;
//
//    while (ptr != NULL) {
//        connect_socket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
//
//        init_res = connect(connect_socket, ptr->ai_addr, (int)ptr->ai_addrlen);
//
//        if (init_res == SOCKET_ERROR) {
//            closesocket(connect_socket);
//            connect_socket = INVALID_SOCKET;
//            continue;
//        }
//
//        if (connect_socket == INVALID_SOCKET) {
//            printf("Unable to connect to server");
//            ptr = ptr->ai_next;
//            if (ptr == NULL) {
//                WSACleanup();
//                return 1;
//            }
//            continue;
//        }
//
//        printf("Successfully established connection with server: ");
//        break;
//    }
//
//    freeaddrinfo(result);
//
//    const char* sendbuf = "this is a test";
//    char recvbuf[DEFAULT_BUFLEN];
//    
//    int iResult;
//
//    init_res = send(connect_socket, sendbuf, (int)strlen(sendbuf), 0);
//    if (init_res == SOCKET_ERROR) {
//        printf("send failed: %d\n", WSAGetLastError());
//        closesocket(connect_socket);
//        WSACleanup();
//        return 1;
//    }
//
//    printf("Bytes sent: %d\n", init_res);
//
//    init_res = shutdown(connect_socket, SD_SEND);
//    if (init_res == SOCKET_ERROR) {
//        printf("shutdown failed: %d\n", WSAGetLastError());
//        closesocket(connect_socket);
//        WSACleanup();
//        return 1;
//    }
//    string message;
//    do {
//        init_res = recv(connect_socket, recvbuf, DEFAULT_BUFLEN, 0);
//        if (init_res > 0) {
//            for (int i = 0; i < init_res; i++) {
//                message.push_back(recvbuf[i]);
//            }
//            printf("Bytes received: %d\n", init_res);
//        }
//        else if (init_res == 0)
//            printf("Connection closed\n");
//        else
//            printf("recv failed: %d\n", WSAGetLastError());
//    } while (init_res > 0);
//
//    cout << "Message recieved: " << message << endl;
//
//    closesocket(connect_socket);
//    WSACleanup();
//
//    printf("Successfully closed socket");
//    return 0;
//
//}
//
//int server()
//{
//    struct addrinfo* result = NULL, * ptr = NULL, hints;
//
//    ZeroMemory(&hints, sizeof(hints));
//    hints.ai_family = AF_INET;
//    hints.ai_socktype = SOCK_STREAM;
//    hints.ai_protocol = IPPROTO_TCP;
//    hints.ai_flags = AI_PASSIVE;
//
//    init_res = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
//
//    if (init_res != 0) {
//        printf("getaddrinfo failed: %d\n", init_res);
//        WSACleanup();
//        return 1;
//    }
//
//    SOCKET listen_socket = INVALID_SOCKET;
//
//    ptr = result;
//
//    while (ptr != NULL) {
//        listen_socket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
//
//        init_res = bind(listen_socket, ptr->ai_addr, (int)ptr->ai_addrlen);
//
//        if (init_res == SOCKET_ERROR) {
//            printf("bind failed with error: %d\n", WSAGetLastError());
//            closesocket(listen_socket);
//            listen_socket = INVALID_SOCKET;
//            continue;
//        }
//
//        if (listen_socket == INVALID_SOCKET) {
//            printf("Unable to connect to client");
//            ptr = ptr->ai_next;
//            if (ptr == NULL) {
//                WSACleanup();
//                return 1;
//            }
//            continue;
//        }
//
//        cout << "Listening on port: " << DEFAULT_PORT << endl;
//        break;
//    }
//
//    freeaddrinfo(result);
//
//    if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR) {
//        printf("Listen failed with error: %ld\n", WSAGetLastError());
//        closesocket(listen_socket);
//        WSACleanup();
//        return 1;
//    }
//
//    SOCKET client_socket;
//
//    client_socket = INVALID_SOCKET;
//
//    client_socket = accept(listen_socket, NULL, NULL);
//    if (client_socket == INVALID_SOCKET) {
//        printf("accept failed: %d\n", WSAGetLastError());
//        closesocket(listen_socket);
//        WSACleanup();
//        return 1;
//    }
//
//    char recvbuf[DEFAULT_BUFLEN];
//    string message;
//    int sendres;
//    do {
//
//        init_res = recv(client_socket, recvbuf, DEFAULT_BUFLEN, 0);
//        if (init_res > 0) {
//            printf("Bytes received: %d\n", init_res);
//            for (int i = 0; i < init_res; i++) {
//                message.push_back(recvbuf[i]);
//            }
//            sendres = send(client_socket, recvbuf, init_res, 0);
//            if (sendres == SOCKET_ERROR) {
//                printf("send failed: %d\n", WSAGetLastError());
//                closesocket(client_socket);
//                WSACleanup();
//                return 1;
//            }
//            printf("Bytes sent: %d\n", sendres);
//        }
//        else if (init_res == 0)
//            printf("Connection closing...\n");
//        else {
//            printf("recv failed: %d\n", WSAGetLastError());
//            closesocket(client_socket);
//            WSACleanup();
//            return 1;
//        }
//
//    } while (init_res > 0);
//
//    cout << "message recieved: " << message << endl;
//
//    init_res = shutdown(client_socket, SD_SEND);
//    if (init_res == SOCKET_ERROR) {
//        printf("shutdown failed: %d\n", WSAGetLastError());
//        closesocket(client_socket);
//        WSACleanup();
//        return 1;
//    }
//
//    closesocket(client_socket);
//    WSACleanup();
//
//    return 0;
//}
//
//int main()
//{
//    WSADATA wsadata;
//
//    // Initialize Winsock
//    init_res = WSAStartup(MAKEWORD(2, 2), &wsadata);
//    if (init_res != 0) {
//        printf("WSAStartup failed: %d\n", init_res);
//        return 1;
//    }
//    int choice;
//    cout << "Start server(0)/client(1)?: ";
//    cin >> choice;
//
//    if (!choice) {
//        return server();
//    }
//    else {
//        return client();
//    }
//
//}