#include <cstring>
#include <netdb.h>
#include <stdlib.h>
#include <cstdio>
#include <iostream>
#include <unistd.h>

#define SERVER_PORT "25500"
#define LOCALHOST "127.0.0.1"
#define BUFFER_SIZE 512

int main(int argc, const char * argv[]){
    if (argc <= 1){
        std::cout << "Provide command" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::cout << "Use [t]cp or [u]dp? (default: tcp)" << std::endl;
    char inp = std::cin.get();

    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_flags = AI_PASSIVE;

    if(inp == 'u' || inp == 'U'){
        hints.ai_socktype = SOCK_DGRAM;
    } else {
        hints.ai_socktype = SOCK_STREAM;
    }

    char buf[BUFFER_SIZE];

    int j = 0;
    while(argv[1][j] != 0) j++;

    struct addrinfo *servinfo;
    
    int status = getaddrinfo(LOCALHOST, SERVER_PORT, &hints, &servinfo);
    if(status != 0){
        std::cout << gai_strerror(status) << std::endl;
        exit(EXIT_FAILURE);
    }

    int sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

    if ((status = connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen)) != 0){
        std::cout << "connect err " << status << std::endl;
        exit(EXIT_FAILURE);
    }

    int sent = send(sockfd, argv[1], j, 0);
    while(sent < j) sent += send(sockfd, &argv[1][sent], j - sent, 0);

    int received = recv(sockfd, &buf, BUFFER_SIZE, 0);

    close(sockfd);

    for(int i=0; i<received; ++i)
        std::cout << buf[i];    

    std::cout << std::endl;
}
