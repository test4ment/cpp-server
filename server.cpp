#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <cstring>
#include <cstdio>
#include <stdlib.h>
#include <iostream>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <vector>
#include <string>
#include <sys/epoll.h>
#include <sstream>
#include <chrono>
#include <ctime>
#include <unistd.h>

// using namespace std;

#define SERVER_PORT "25500"
#define BUFFER_SIZE 512
#define SERVER_BACKLOG 10
#define EVENTS_BUF 20

void halt_for_err(){
    if(errno != 0){
        std::cerr << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }
}

int main()
{
    struct addrinfo hints;
    struct addrinfo *servinfo_tcp;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    int status = getaddrinfo(NULL, SERVER_PORT, &hints, &servinfo_tcp);
    if(status != 0){
        std::cout << gai_strerror(status) << std::endl;
        exit(EXIT_FAILURE);
    }

    hints.ai_socktype = SOCK_DGRAM;
    struct addrinfo *servinfo_udp;
    status = getaddrinfo(NULL, SERVER_PORT, &hints, &servinfo_udp);
    if(status != 0){
        std::cout << gai_strerror(status) << std::endl;
        exit(EXIT_FAILURE);
    }

    int listen_socket_tcp = socket(servinfo_tcp->ai_family, servinfo_tcp->ai_socktype, servinfo_tcp->ai_protocol);
    halt_for_err();
    int listen_socket_udp = socket(servinfo_udp->ai_family, servinfo_udp->ai_socktype, servinfo_udp->ai_protocol);
    halt_for_err();

    bind(listen_socket_tcp, servinfo_tcp->ai_addr, servinfo_tcp->ai_addrlen);
    bind(listen_socket_udp, servinfo_udp->ai_addr, servinfo_udp->ai_addrlen);
    freeaddrinfo(servinfo_tcp);
    listen(listen_socket_tcp, SERVER_BACKLOG);
    halt_for_err();

    int epoll_fd = epoll_create1(0);
    halt_for_err();

    struct epoll_event ev, events[EVENTS_BUF];
    ev.events = EPOLLIN;
    ev.data.fd = listen_socket_tcp;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_socket_tcp, &ev) == -1) {
        perror("epoll_ctl: listen_sock_tcp");
        exit(EXIT_FAILURE);
    }

    ev.data.fd = listen_socket_udp;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_socket_udp, &ev) == -1) {
        perror("epoll_ctl: listen_sock_udp");
        exit(EXIT_FAILURE);
    }

    int nfds;
    char buf[BUFFER_SIZE];
    int total_conn = 0;
    int curr_conn = 0;
    bool shutdown;
    struct sockaddr_in sender_addr;
    sender_addr.sin_family = AF_INET;
    socklen_t sender_addr_l;
    int written;
    std::ostringstream stream;
    stream.rdbuf()->pubsetbuf(buf, sizeof(buf));

    std::cout << "Server listening at " << SERVER_PORT << "..." << std::endl;

    for (;;) {
        if(shutdown) break;

        nfds = epoll_wait(epoll_fd, events, EVENTS_BUF, -1);
        if (nfds == -1) { perror("epoll_wait"); exit(EXIT_FAILURE); }

        for (int i = 0; i < nfds; ++i) {
            int event_fd = events[i].data.fd;
            if (event_fd == listen_socket_tcp) {
                int conn_sock = accept(listen_socket_tcp, NULL, NULL);
                std::cout << "Accepted new client" << std::endl;
                if (conn_sock == -1) { perror("accept"); exit(EXIT_FAILURE); }
                total_conn++; curr_conn++;
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = conn_sock;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_sock,
                            &ev) == -1) { perror("epoll_ctl: conn_sock"); exit(EXIT_FAILURE); }
                continue;
            } 
            int received;
            if(event_fd == listen_socket_udp)
                received = recvfrom(event_fd, &buf, BUFFER_SIZE, 0, (struct sockaddr *)&sender_addr, &sender_addr_l);
            else 
                received = recv(event_fd, &buf, BUFFER_SIZE, 0);
            
            if(received < 0){
                std::cerr << "Receive err: " << strerror(errno) << " in " << event_fd << std::endl;
                continue;
            }
            if(received == 0){
                std::cout << event_fd << " disconnected" << std::endl;
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, event_fd, &ev);
                curr_conn--;
                continue;
            } 
            std::cout << "Received " << received << " bytes from " << event_fd << ": ";
            for(int j=0; j<received; ++j) std::cout << buf[j];
            std::string inp = std::string(buf, received);
            std::cout << std::endl;

            stream.seekp(0);
            if(inp == std::string("/time")){
                std::time_t t = std::time(nullptr);
                written = std::strftime(buf, BUFFER_SIZE, "%F %T", std::localtime(&t));
            } else if (inp == std::string("/stats")) {
                stream << "Currently connected: " << curr_conn << 
                std::endl << "Total connections: " << total_conn << std::endl;
                written = stream.tellp();
            } else if (inp == std::string("/shutdown")) {
                shutdown = true;
                stream << "Shutting down..." << std::endl;
                written = stream.tellp();
            } else {
                written = received;
            }
            int sent;
            if(event_fd == listen_socket_udp) 
                sent = sendto(event_fd, &buf, written, 0, (struct sockaddr *) &sender_addr, sender_addr_l);
            else 
                sent = send(event_fd, &buf, written, 0);
            if(sent <= 0){
                std::cerr << "Send err: " << strerror(errno) << " in " << event_fd << std::endl;
            }
        }
    }
    
    close(listen_socket_tcp);
    close(epoll_fd);

    std::cout << "Server closed" << std::endl;
}
