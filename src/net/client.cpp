#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>

#include "net/client.hpp"

namespace WMNet {

    namespace {
        bool send_all(SOCKET socket, byte* ptr, size_t bytes, int flags) {
            do {
                int sent = send(socket, ptr, bytes, flags);

                if (sent <= 0) {
                    return false;
                }
                ptr += sent;
                bytes -= sent;

            } while (bytes);

            return true;
        }

        bool recv_all(SOCKET socket, byte* ptr, size_t bytes, int flags) {
            do {
                int sent = recv(socket, ptr, bytes, flags);

                if (sent <= 0) {
                    return false;
                }
                ptr += sent;
                bytes -= sent;

            } while (bytes);

            return true;
        }
    }

    Client::Client() {
        
        WSADATA wsaData;
        int startup_res = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (startup_res != 0) {
            // WSAStartup failed, networking is unavailable
            return;
        }
    }

	int Client::create_tcp_socket(const char* ip, const char* port) {
        struct addrinfo* result = NULL, * ptr = NULL;
        addrinfo hints;

        ZeroMemory(&hints, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = 0;


        if (getaddrinfo(ip, port, &hints, &result)) {
            return 0;
        }
        client_socket = INVALID_SOCKET;

        ptr = result;

        while (ptr != NULL) {
            client_socket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

            if (client_socket == INVALID_SOCKET) {
                ptr = ptr->ai_next;
                if (ptr == NULL) {
                    return 0;
                }
                continue;
            }


            if (connect(client_socket, ptr->ai_addr, (int)ptr->ai_addrlen)) {
                closesocket(client_socket);
                client_socket = INVALID_SOCKET;

                ptr = ptr->ai_next;
                if (ptr == NULL) {
                    return 0;
                }
                continue;
            }

            break;
        }

        // memory management

        freeaddrinfo(result);
        connected.store(true);
        on_connect_to_server();

        return 1;
	}

    void Client::register_handler(msg_id_t id, MessageHandler handler) {
        message_handlers[id].push_back(handler);
    }

    void Client::send_loop() {
        while (connected.load()) {
            std::unique_lock<std::mutex> ulock(write_mutex);
            if (outgoing_messages.empty()) write_status.wait(ulock, [this] {return !this->outgoing_messages.empty() || !this->connected.load(); });

            if (!connected.load()) break;

            std::vector<byte> write;

            if (!outgoing_messages.try_pop(write)) {
                continue;
            }

            ulock.unlock();

            size_t rem = write.size();
            byte* ptr = write.data();

            send_all(client_socket, ptr, rem, 0);

        }
    }

    void Client::message_listener() {

        while (connected.load()) {
            std::unique_lock<std::mutex> lock(queue_mutex);

            if (incoming_messages.empty()) queue_status.wait(lock, [this] {return !this->incoming_messages.empty() || !this->connected.load(); });

            if (!connected.load()) break;

            ServerMessage incm;
            if (incoming_messages.try_pop(incm)) {
                lock.unlock();
                auto it = message_handlers.find(incm.header.id);
                if (it != message_handlers.end()) {
                    for (auto& handler : it->second) handler(incm);
                }
                else {
                    //unknown message
                }
            };
        }

    }

    bool Client::is_connected() {
        return connected.load();
    }

    void Client::read_loop() {

        while (connected.load()) {
            NetHeader header;

            byte* ptr = reinterpret_cast<byte*>(&header);

            if (!recv_all(client_socket, ptr, sizeof(NetHeader), 0)) {
                // handle error
                return;
            };

            header.id = ntohs(header.id);
            header.size = ntohs(header.size);
            header.type = ntohs(header.type);

            std::vector<byte> data(header.size);

            if (header.size) {
                ptr = data.data();

                if (!recv_all(client_socket, ptr, header.size, 0)) {
                    // handle error
                    return;
                }
            }

            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                incoming_messages.push({ header, data });
                queue_status.notify_one();
            }
        }

    }

    int Client::start_client(const char* ip, const char* port) {
        if (create_tcp_socket(ip, port)) {

            connected.store(true);
            m_message_thread = std::thread([this]() {
                this->message_listener();
                });

            m_read_thread = std::thread([this]() {
                this->read_loop();
                });

            m_write_thread = std::thread([this]() {
                this->send_loop();
                });

        }
        else {
            // Issue

            return 0;
        }

        return 1;
    }

    void Client::stop_client() {
        connected.store(false);
        queue_status.notify_all();
        write_status.notify_all();

        closesocket(client_socket);

        if (m_message_thread.joinable()) m_message_thread.join();
        if (m_read_thread.joinable()) m_read_thread.join();
        if (m_write_thread.joinable()) m_write_thread.join();
    }

}