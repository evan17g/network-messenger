#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main() {
    const char* port = "7011";

    int status;
    struct addrinfo hints, *res, *p;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; 

    if ((status = getaddrinfo(NULL, port, &hints, &res)) != 0) {
        throw std::runtime_error("Error getting server address info: " + std::string(gai_strerror(status)));
    }

    int sockfd = -1;

    for (p = res; p != nullptr; p = p->ai_next) {
        // attempt to create socket for this address
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            continue; // try next connection
        }
        
        // set socket to be reused if needed
        int yes = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
            close(sockfd); // close socket and try next if fail to set options
            continue;
        }

        // attempt to bind socket to port
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == 0) {
            break; // successful bind
        }

        close(sockfd);
        sockfd = -1;
    }

    freeaddrinfo(res);

    if (sockfd == -1) {
        throw std::runtime_error("Failed to bind to socket.");
    }

    if (listen(sockfd, SOMAXCONN) == -1) {
        throw std::runtime_error("Failed to listen on port: " + std::string(strerror(errno)));
    }

    std::cout << "Server Listening on Port " << port << std::endl;

    int fd;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    if ((fd = accept(sockfd, (struct sockaddr*) &client_addr, &client_addr_len)) == -1) {
        throw std::runtime_error("Error accepting client connection: " + std::string(strerror(errno)));
    }

    // now can use fd to send and recieve data
    // read message header from client
    size_t msg_length;
    size_t total_read = 0;
    char msg_length_buf[4];
    while (total_read < 4) {
        ssize_t read = recv(fd, msg_length_buf + total_read, 4 - total_read, 0);
        if (read == -1) {
            throw std::runtime_error("Error receiving message header: " + std::string(strerror(errno)));
        } else if (read == 0) {
            throw std::runtime_error("Connection closed while receiving message");
        }
        total_read += read;
    }
    memcpy(&msg_length, msg_length_buf, 4);
    msg_length = ntohl(msg_length);

    // now read message from client
    std::string msg(msg_length, '\0');
    total_read = 0;
    while (total_read < msg_length) {
        ssize_t read = recv(fd, &msg[0] + total_read, msg_length - total_read, 0);
        if (read == -1) {
            throw std::runtime_error("Error receiving message: " + std::string(strerror(errno)));
        } else if (read == 0) {
            throw std::runtime_error("Connection closed while recieving message");
        }
        total_read += read;
    }

    std::cout << "Message from client: " << msg.c_str() << std::endl;

    close(sockfd);
    return 0;
}