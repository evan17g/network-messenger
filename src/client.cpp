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

#include <sqlite3.h>
#include <dotenv.h>
#include "client.h"

sqlite3* db;

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
    SendRequest(sockfd, "GET_ACCOUNTS|0|0");
    std::string response = RecieveResponse(sockfd);
    std::cout << response << std::endl;

    int user_id = 0;
    std::cout << "Please type the Account ID that you wish to use: ";
    std::cin >> user_id;
    std::cout << std::endl;

    std::string request = "GET_CONVERSATIONS|" + std::to_string(user_id) + "|0";
    SendRequest(sockfd, request);
    response = RecieveResponse(sockfd);
    std::cout << response << std::endl;

    // close connections
    SendRequest(sockfd, "QUIT|0|0");
    close(sockfd);
    sqlite3_close(db);
    return 0;
}

void SendRequest(int sockfd, const std::string& request) {
    // get length header details
    uint32_t length = htonl(request.length());
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
    while (total_sent < request.length()) {
        ssize_t sent = send(sockfd, request.c_str() + total_sent, request.length() - total_sent, 0);
        if (sent == -1) {
            throw std::runtime_error("Error sending request body: " + std::string(strerror(errno)));
        }
        total_sent += sent;
    }
}

std::string RecieveResponse(int sockfd) {
    // read response header from client
    size_t response_length;
    size_t total_read = 0;
    char response_length_buf[4];
    while (total_read < 4) {
        ssize_t read = recv(sockfd, response_length_buf + total_read, 4 - total_read, 0);
        if (read == -1) {
            throw std::runtime_error("Error receiving response header: " + std::string(strerror(errno)));
        } else if (read == 0) {
            throw std::runtime_error("Connection closed while receiving response");
        }
        total_read += read;
    }
    memcpy(&response_length, response_length_buf, 4);
    response_length = ntohl(response_length);

    // now read message from client
    std::string res(response_length, '\0');
    total_read = 0;
    while (total_read < response_length) {
        ssize_t read = recv(sockfd, &res[0] + total_read, response_length - total_read, 0);
        if (read == -1) {
            throw std::runtime_error("Error receiving request: " + std::string(strerror(errno)));
        } else if (read == 0) {
            throw std::runtime_error("Connection closed while recieving request");
        }
        total_read += read;
    }

    return res;
}