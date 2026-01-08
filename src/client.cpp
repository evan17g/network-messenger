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
#include <mutex>
#include <condition_variable>
#include <exception>
#include <thread>

#include <sqlite3.h>
#include <dotenv.h>
#include "client.h"

sqlite3* db;

std::mutex response_mutex;
std::mutex state_mutex;
std::condition_variable cv;
bool response_handled;

STATE state = HOME;
int user_id = 0;
int conversation_id = 0;

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

    // set up thread to recieve
    std::thread t(RecieveFromServer, sockfd);

    std::cout << "Connected to server at " << server_ip << " on port " << port << "\n" << std::endl;


    // client flow logic
    try {
        bool running = true;
        while (running) {
            if (state == CONVERSATION) { 
                std::string message;
                std::getline(std::cin, message);

                if (message == ".home") {
                    std::unique_lock<std::mutex> state_lock(state_mutex);
                    state = HOME;
                } else {
                    Request r(SEND_MESSAGE, user_id, conversation_id, message);
                    std::unique_lock<std::mutex> response_lock(response_mutex);
                    response_handled = false;
                    SendRequest(sockfd, r.to_string());
                    cv.wait(response_lock, []{return response_handled;});
                }
            } else { // state == HOME
                int option;
                std::cout << "-------------- Home Page ----------------" << std::endl;
                std::cout << " 1. Get all current accounts" << std::endl;
                std::cout << " 2. Create a new account" << std::endl;
                std::cout << " 3. Get all conversations for an account" << std::endl;
                std::cout << " 4. Create a new conversation for an account" << std::endl;
                std::cout << " 5. Send messages" << std::endl;
                std::cout << " 6. Quit" << std::endl;

                std::cout << std::endl << "Please select an option from the menu: ";
                std::cin >> option;

                switch (option) {
                case 1: {
                    Request r(GET_ACCOUNTS, 0, 0, "");
                    std::unique_lock<std::mutex> response_lock(response_mutex);
                    response_handled = false;
                    SendRequest(sockfd, r.to_string());
                    cv.wait(response_lock, []{return response_handled;});
                    break;
                }

                case 2: {
                    int new_user_id;
                    std::string new_user_name;
                    std::cout << "Enter the user_id you wish to claim (must not already be taken): ";
                    std::cin >> new_user_id;

                    std::cout << "Enter your username: ";
                    std::cin >> new_user_name;

                    Request r(ADD_ACCOUNT, new_user_id, 0, new_user_name);
                    std::unique_lock<std::mutex> response_lock(response_mutex);
                    response_handled = false;
                    SendRequest(sockfd, r.to_string());
                    cv.wait(response_lock, []{return response_handled;});
                    break;
                }

                case 3: {
                    int user_id = 0;
                    std::cout << "Please type the Account ID that you wish to use: ";
                    std::cin >> user_id;
                    std::cout << std::endl;

                    Request r(GET_CONVERSATIONS, user_id, 0, "");
                    std::unique_lock<std::mutex> response_lock(response_mutex);
                    response_handled = false;
                    SendRequest(sockfd, r.to_string());
                    cv.wait(response_lock, []{return response_handled;});
                    break;
                }

                case 4: {
                    int user_id = 0;
                    std::cout << "Please type the Account ID that you wish to use: ";
                    std::cin >> user_id;

                    int other_user_id = 0;
                    std::cout << "Please type the Account ID that you wish to message: ";
                    std::cin >> other_user_id;
                    std::cout << std::endl;

                    Request r(ADD_CONVERSATION, user_id, other_user_id, "");
                    std::unique_lock<std::mutex> response_lock(response_mutex);
                    response_handled = false;
                    SendRequest(sockfd, r.to_string());
                    cv.wait(response_lock, []{return response_handled;});
                    break;
                }

                case 5: {
                    std::cout << "Select the account that you wish to use: ";
                    std::cin >> user_id;

                    std::cout << "Select the conversation that you wish to join: ";
                    std::cin >> conversation_id;
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                    
                    Request r(GET_CONVERSATION, user_id, conversation_id, "");
                    std::unique_lock<std::mutex> response_lock(response_mutex);
                    response_handled = false;
                    SendRequest(sockfd, r.to_string());
                    cv.wait(response_lock, []{return response_handled;});
                    std::unique_lock<std::mutex> state_lock(state_mutex);
                    state = CONVERSATION;
                    break;
                }

                case 6: {
                    running = false;
                    std::cout << "Quitting application." << std::endl;
                    break;
                }

                default:
                    std::cout << "Invalid selection. Please try again." << std::endl;
                    break;
                }
            }
        }
    } catch (const std::runtime_error& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    // close connections
    SendRequest(sockfd, "QUIT|0|0|");
    t.join();
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

Response RecieveResponse(int sockfd) {
    // read response header from client
    size_t response_length;
    size_t total_read = 0;
    char response_length_buf[4];
    while (total_read < 4) {
        ssize_t read = recv(sockfd, response_length_buf + total_read, 4 - total_read, 0);
        if (read == -1) {
            throw std::runtime_error("Error receiving response header: " + std::string(strerror(errno)));
        } else if (read == 0) {
            std::cout << "Message: " << response_length_buf << std::endl;
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
            std::cout << "Message: " << res << std::endl;
            throw std::runtime_error("Connection closed while recieving request");
        }
        total_read += read;
    }

    return Response::parseResponse(res);
}

void RecieveFromServer(int sockfd) {
    while (true) {
        // read response header from server
        size_t response_length;
        size_t total_read = 0;
        char response_length_buf[4];
        while (total_read < 4) {
            ssize_t read = recv(sockfd, response_length_buf + total_read, 4 - total_read, 0);
            if (read == -1) {
                throw std::runtime_error("Error receiving output header: " + std::string(strerror(errno)));
            } else if (read == 0) {
                throw std::runtime_error("Connection closed while receiving output from server");
            }
            total_read += read;
        }
        memcpy(&response_length, response_length_buf, 4);
        response_length = ntohl(response_length);

        // now read response from server
        std::string res(response_length, '\0');
        total_read = 0;
        while (total_read < response_length) {
            ssize_t read = recv(sockfd, &res[0] + total_read, response_length - total_read, 0);
            if (read == -1) {
                throw std::runtime_error("Error receiving output: " + std::string(strerror(errno)));
            } else if (read == 0) {
                throw std::runtime_error("Connection closed while recieving output from server");
            }
            total_read += read;
        }

        Response response = Response::parseResponse(res);

        if (response.type == RESPONSE) {
            if (response.success) {
                std::cout << response.data << std::endl;
            } else {
                std::cout << response.message << std::endl;
            }
            std::unique_lock<std::mutex> lock(response_mutex);
            response_handled = true;
            cv.notify_one();
        } else if (response.type == BROADCAST) {
            std::unique_lock<std::mutex> state_lock(state_mutex);
            if (state == CONVERSATION && conversation_id == response.conversation_id) {
                // need to update screen
                Request r(UPDATE_CONVERSATION, user_id, conversation_id, "");
                SendRequest(sockfd, r.to_string());
                Response response = RecieveResponse(sockfd);
                if (response.success) {
                    std::cout << response.data << std::endl;
                } else {
                    std::cout << response.message << std::endl;
                }
            }
        }
    }
}