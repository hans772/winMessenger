#include "net/server.hpp"
#include "net/message.hpp"
#include "net/serialize.hpp"
#include "net/iocp.hpp"

#include <WS2tcpip.h>
#include <thread>
#include <chrono>

namespace WMNet {

	int Server::create_tcp_socket(const char* port) {

        struct addrinfo* result = NULL, * ptr = NULL;
        addrinfo hints;

        ZeroMemory(&hints, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = AI_PASSIVE;

        if (getaddrinfo(NULL, port, &hints, &result)) {

            return 0;
        }

        listen_socket = INVALID_SOCKET;

        ptr = result;

        while (ptr != NULL) {


            listen_socket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

            if (listen_socket == INVALID_SOCKET) {
                ptr = ptr->ai_next;
                if (ptr == NULL) {
                    throw std::exception("No available sockets found");
                    return 0;
                }
                continue;
            }

            if (bind(listen_socket, ptr->ai_addr, (int)ptr->ai_addrlen)) {
                closesocket(listen_socket);
                listen_socket = INVALID_SOCKET;

                ptr = ptr->ai_next;
                if (ptr == NULL) {
                    throw std::exception("No available sockets found");
                    return 0;
                }
                continue;
            }
            break;
        }

        freeaddrinfo(result);

        return 1;
	}

    Server::Server() : ioc_port(INVALID_HANDLE_VALUE), listen_socket(INVALID_SOCKET) {
        WSADATA wsaData;
        int startup_res = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (startup_res != 0) {
            // WSAStartup failed, networking is unavailable
            return;
        }

    }

    void Server::accept_connections(int max_connections) {

        if (listen(listen_socket, SOMAXCONN)) {
            closesocket(listen_socket);
            return;
        }

        ioc_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        int thread_count = sysinfo.dwNumberOfProcessors * 2;
        for (int i = 0; i < thread_count; ++i)
            CreateThread(nullptr, 0, Server::worker_thread, this, 0, nullptr);


        int connected = 0;

        while (connected < max_connections && active.load()) {
            SOCKET new_client = INVALID_SOCKET;
            new_client = accept(listen_socket, NULL, NULL);

            if (new_client == INVALID_SOCKET) {
                
                // accept failed;

            }
            else {
                if (!active.load()) break;

                std::unique_lock<std::shared_mutex> lock(conn_mtx);

                auto n_conn = connections[connected] = std::make_shared<Connection>(connected, new_client,
                    [this](std::shared_ptr<Connection> c, NetHeader h, std::vector<byte>& d) {
                        this->push_message(c, h, d);
                    });

                push_event(n_conn, NetEvent::ConnectionAccept);

                HANDLE assoc = CreateIoCompletionPort((HANDLE)new_client, ioc_port, reinterpret_cast<ULONG_PTR>(n_conn.get()), 0);
                if (!assoc) return;
                n_conn->send(Message<Empty>{.id = ReservedMessages::ConnectionAccepted});
                n_conn->begin_read();

                connected++;
            }
        }

        closesocket(listen_socket);
    }

    int Server::close_server() {

        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        int thread_count = sysinfo.dwNumberOfProcessors * 2;
        active.store(false);

        for (int i = 0; i < thread_count; ++i) {
            PostQueuedCompletionStatus(ioc_port, 0, 0, nullptr);
        }

        WSACleanup();
        return 1;
    }

    void Server::push_message(std::shared_ptr<Connection> conn, NetHeader header, std::vector<byte>& data) {
        std::lock_guard<std::mutex> lock(queue_mutex);
        incoming_queue.push({conn, header, data});
        queue_status.notify_one();
    }

    void Server::register_handler(msg_id_t id, MessageHandler handler) {
        message_handlers[id].push_back(handler);
    }

    void Server::push_event(std::shared_ptr<Connection> conn, NetEvent event) {
        std::lock_guard<std::mutex> lock(event_mutex);
        event_queue.push({ conn, event });
        event_status.notify_one();
    }

    void Server::listen_connections() {
        while (active.load()) {
            std::unique_lock<std::mutex> ulock(queue_mutex);

            if(incoming_queue.empty()) queue_status.wait(ulock, [this] {return !this->incoming_queue.empty() || !this->active.load(); });
            if (!active.load()) break;

            ConnectionMessage incm;
            if (incoming_queue.try_pop(incm)) {
                ulock.unlock();
                auto it = message_handlers.find(incm.header.id);
                if (it != message_handlers.end()) {
                    std::lock_guard<std::mutex> lock(app_mutex);
                    for( auto& handler : it->second) handler(incm);
                }
                else {
                    //handle unknown message id
                }
            };
        }
    }

    void Server::listen_events() {
        while (active.load()) {
            std::unique_lock<std::mutex> ulock(event_mutex);
            if(event_queue.empty()) event_status.wait(ulock, [this] {
                return !event_queue.empty() || !active.load();
                });

            if (!active.load()) break;

            std::pair<std::shared_ptr<Connection>, NetEvent> ev;
            if (event_queue.try_pop(ev)) {
                ulock.unlock();
                std::lock_guard<std::mutex> lock(app_mutex);
                switch (ev.second) {
                case NetEvent::ConnectionAccept:  connection_accept(ev.first); break;
                case NetEvent::ConnectionDropped: connection_drop(ev.first); break;
                case NetEvent::SafeToDestroy:     connection_destroy(ev.first);    break;
                }
            }
        }
    }

    void Server::ping(ConnectionMessage inc_m) {
        std::cout << "Ping!" << std::endl;
    }

    void Server::connection_accept(std::shared_ptr<Connection> conn) {
        on_connection_accepted(conn);
    }
    void Server::connection_drop(std::shared_ptr<Connection> conn) {

        std::unique_lock<std::shared_mutex> c_lock(conn_mtx);

        auto node = connections.extract(conn->conn_id);

        if (!node.empty()) {

            disconnections.insert(std::move(node));
            conn->is_disconnecting = true;
            closesocket(conn->socket);
        }

        on_connection_dropped(conn);

    }
    void Server::connection_destroy(std::shared_ptr<Connection> conn) {
        disconnections.erase(conn->conn_id);

        on_connection_destroyed(conn);

    }

    int Server::start_server(const char* port) {
        if (create_tcp_socket(port)) {

            active.store(true);

            m_acceptor_thread = std::thread([this]() {
                this->accept_connections(100);
                });

            m_message_thread = std::thread([this]() {
                this->listen_connections();
                });

            m_event_thread = std::thread([this]() {
                this->listen_events();
                });

        }
        else { 
            // Issue

            return 0;
        }

        return 1;
    }

    void Server::stop_server() {
        active.store(false);
        closesocket(listen_socket);
        queue_status.notify_all();
        event_status.notify_all();

        if (m_acceptor_thread.joinable()) m_acceptor_thread.join();
        if (m_message_thread.joinable()) m_message_thread.join();
        if (m_event_thread.joinable()) m_event_thread.join();
        
    }

    DWORD WINAPI Server::worker_thread(LPVOID lpParam) {

        Server* server = reinterpret_cast<Server*>(lpParam);

        DWORD bytes_transferred;
        ULONG_PTR key;
        IOCP_CONTEXT* context;

        while (true) {

            BOOL ok = GetQueuedCompletionStatus(server->ioc_port, &bytes_transferred, &key, (LPOVERLAPPED*)&context, INFINITE);
            
            if (context == nullptr) {
                //server disconnect
                break;
            }
            
            Connection* c_ptr = reinterpret_cast<Connection*>(key);
            c_ptr->active_contexts--;

            if (!c_ptr->active_contexts && c_ptr->is_disconnecting) {
                server->push_event(c_ptr->shared_from_this(), NetEvent::SafeToDestroy);

                context->in_service.store(false);
                continue;
            }
            
            // if ok is false it means something went wrong
            if (!ok) {
                DWORD err = WSAGetLastError();
                if (err == SOCKET_ERROR || err == 64) {
                    server->push_event(c_ptr->shared_from_this(), NetEvent::ConnectionDropped);
                }
                continue;
            }

            // if bytes transferred is 0, client has disconnected
            if (bytes_transferred == 0) {
              
                server->push_event(c_ptr->shared_from_this(), NetEvent::ConnectionDropped);

                continue;
            }

            switch (context->operation) {
            case IO_Oper::READ:

                context->wsa_buf.len -= bytes_transferred;
                context->wsa_buf.buf += bytes_transferred;
                context->completed_bytes += bytes_transferred;

                if (context->wsa_buf.len) {
                    context->flags = 0;
                    int res = WSARecv(c_ptr->socket, &context->wsa_buf, 1, nullptr, &context->flags, &context->overlapped, nullptr);
                    c_ptr->active_contexts++;

                    if (res == SOCKET_ERROR) {
                        int err = WSAGetLastError();
                        if (err != WSA_IO_PENDING) {
                            server->push_event(c_ptr->shared_from_this(), NetEvent::ConnectionDropped);

                            context->in_service.store(false);
                        }
                    }

                    continue;
                }

                c_ptr->on_read_completed();
                break;

            case IO_Oper::WRITE:
                context->wsa_buf.len -= bytes_transferred;
                context->wsa_buf.buf += bytes_transferred;
                context->completed_bytes += bytes_transferred;

                if (context->wsa_buf.len) {
                    int res = WSASend(c_ptr->socket, &context->wsa_buf, 1, nullptr, 0, &context->overlapped, nullptr);
                    c_ptr->active_contexts++;

                    if (res == SOCKET_ERROR) {
                        int err = WSAGetLastError();
                        if (err != WSA_IO_PENDING) {

                            server->push_event(c_ptr->shared_from_this(), NetEvent::ConnectionDropped);

                            context->in_service.store(false);
                        }
                    }
                    
                    continue;
                }

                c_ptr->on_write_completed();
                break;
            }
        }
        return 0;
    }

}