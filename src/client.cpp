#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstdlib>

#include <dotenv.h>

int main() {
    // Load variables from the .env file
    dotenv::init();

    const char* port = "7011";
    const char* server_ip = std::getenv("HOST_ADDR");
    char server_addr[INET_ADDRSTRLEN];

    int status;
    struct addrinfo hints, *res, *p;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(server_ip, port, &hints, &res)) != 0) {
        throw std::runtime_error("Error getting server address info: " + std::string(gai_strerror(status)));
    }

    int sockfd = -1;

    for (p = res; p != nullptr; p = p->ai_next) {
        // attempt to connect given this res address
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            continue; // try next connection
        }

        // attempt to connect with socket
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0) {
            // set server_addr string then break
            auto* addr = (sockaddr_in*)p->ai_addr;
            inet_ntop(AF_INET, &addr->sin_addr, server_addr, sizeof(server_addr));
            break; // successful connect
        }

        close(sockfd);
        sockfd = -1;
    }

    freeaddrinfo(res);

    if (sockfd == -1) {
        throw std::runtime_error("Failed to connect with socket." + std::string(strerror(errno)));
    }

    std::cout << "Connected to server at " << server_ip << " on port " << port << "\n" << std::endl;

    

    // now send and recieve
    std::string msg;
    std::cout << "Enter message to send to server: ";
    std::getline(std::cin, msg);

    // get length header details
    uint32_t length = htonl(msg.length());
    char length_buf[4];
    memcpy(length_buf, &length, 4);

    // send header
    size_t total_sent = 0;
    while (total_sent < 4) {
        ssize_t sent = send(sockfd, length_buf + total_sent, 4 - total_sent, 0);
        if (sent == -1) {
            throw std::runtime_error("Error sending request header: " + std::string(strerror(errno)));
        }
        total_sent += sent;
    }

    // now send message
    total_sent = 0;
    while (total_sent < msg.length()) {
        ssize_t sent = send(sockfd, msg.c_str() + total_sent, msg.length() - total_sent, 0);
        if (sent == -1) {
            throw std::runtime_error("Error sending request body: " + std::string(strerror(errno)));
        }
        total_sent += sent;
    }

    close(sockfd);
    return 0;
}

void PrintAccounts() {
    std::cout << "Available Accounts:" << std::endl;
    
}