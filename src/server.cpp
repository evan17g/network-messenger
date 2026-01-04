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
#include <sstream>
#include <thread>
#include <vector>
#include <atomic>
#include <csignal>

#include <sqlite3.h>
#include "server.h"

volatile sig_atomic_t shutdown_requested = 0;
int listen_sockfd = -1;

int main() {
    // setup signal handling
    SetupSignalHandling();

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

    listen_sockfd = -1;

    for (p = res; p != nullptr; p = p->ai_next) {
        // attempt to create socket for this address
        if ((listen_sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            continue; // try next connection
        }
        
        // set socket to be reused if needed
        int yes = 1;
        if (setsockopt(listen_sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
            close(listen_sockfd); // close socket and try next if fail to set options
            continue;
        }

        // attempt to bind socket to port
        if (bind(listen_sockfd, p->ai_addr, p->ai_addrlen) == 0) {
            break; // successful bind
        }

        close(listen_sockfd);
        listen_sockfd = -1;
    }

    freeaddrinfo(res);

    if (listen_sockfd == -1) {
        throw std::runtime_error("Failed to bind to socket.");
    }

    if (listen(listen_sockfd, SOMAXCONN) == -1) {
        throw std::runtime_error("Failed to listen on port: " + std::string(strerror(errno)));
    }

    std::cout << "Server Listening on Port " << port << std::endl;
    std::vector<std::thread> threads;

    while (!shutdown_requested) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int fd = accept(listen_sockfd, (struct sockaddr*) &client_addr, &client_addr_len);
        if (fd == -1) {
            if (errno == EINTR && shutdown_requested) {
                break;
            }
            std::cerr << "Error accepting new client connection." << std::endl;
            continue;
        }

        std::thread t(handle_client, fd);
        threads.push_back(std::move(t));
    }

    for (int i=0; i<threads.size(); ++i) {
        std::thread& t = threads[i];
        if (t.joinable()) {
            t.join();
        }
        
    }
    close(listen_sockfd);
    return 0;
}

void handle_client(int client_fd) {
    sqlite3* db;
    try {
        // Store the IP address and port in peer_ip and peer_port
        struct sockaddr_in peer_addr;
        socklen_t peer_addr_len = sizeof(peer_addr);

        if (getpeername(client_fd, (sockaddr *)&peer_addr, &peer_addr_len) == -1) {
            throw std::runtime_error("Error getting client address and port: " + std::string(strerror(errno)));
        }

        std::cout << "New client connection at " << inet_ntoa(peer_addr.sin_addr) << " on port " << peer_addr.sin_port << std::endl;

        // setup new thread db connection
        if (sqlite3_open("database.db", &db) != SQLITE_OK) {
            throw std::runtime_error("Failed to open database: " + std::string(sqlite3_errmsg(db)));
        }
        // Enable foreign keys
        sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);

        bool running = true;

        while (running) {
            if (shutdown_requested) {
                break;
            }

            // now can use fd to send and recieve data
            Request req = RecieveRequest(client_fd);

            std::string response;
            switch (req.type) {
                case QUIT:
                    running = false;
                    break;
                case GET_ACCOUNTS:
                    GetAccounts(response, db);
                    SendResponse(client_fd, response);
                    break;
                case GET_CONVERSATIONS:
                    GetConversations(response, req.user_id, db);
                    SendResponse(client_fd, response);
                    break;
                case GET_CONVERSATION:
                    GetConversation(response, req.user_id, req.conversation_id, db);
                    SendResponse(client_fd, response);
                    break;
                default:
                    running = false;
                    break;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
    }
    sqlite3_close(db);
    close(client_fd);
}

void GetAccounts(std::string& response, sqlite3* db) {
    std::stringstream ss;
    std::string success = "true";
    std::string message = "";
    std::string data = "";

    ss << "----- Current Accounts -----" << std::endl;

    sqlite3_stmt* stmt;
    const char* sql = "SELECT * FROM Users;";
    bool dbError = false;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        dbError = true;
    }

    bool atLeastOneRow = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int user_id = sqlite3_column_int(stmt, 0);
        const unsigned char* user_name = sqlite3_column_text(stmt, 1);

        int blank_space = 5;
        int start = user_id;
        while (start / 10 > 0) {
            start = start / 10;
            blank_space--;
        }
        std::string space(blank_space, ' ');
        
        ss << "ID: " << user_id << space << " Username: " << user_name << std::endl;
        atLeastOneRow = true;
    }

    sqlite3_finalize(stmt);
    data = ss.str();

    if (dbError) {
        success = "false";
        message = "Database Error: " + std::string(sqlite3_errmsg(db));
        data = "";
    } else if (!atLeastOneRow) {
        success = "false";
        message = "There are no accounts to retrieve!";
        data = "";
    }

    response = success + "|" + message + "|" + data;
}

void GetConversations(std::string& response, const int& user_id, sqlite3* db) {
    std::stringstream ss;
    std::string success = "true";
    std::string message = "";
    std::string data = "";
    
    ss << "----- Conversations -----" << std::endl;

    bool dbError = false;
    sqlite3_stmt* stmt;
    const char* sql = R"sql(
    SELECT 
        c.conversation_id, 
        CASE WHEN c.user_id_1 = ? THEN u2.user_name
        ELSE u1.user_name
        END AS other_user_name,
        c.last_updated       
    FROM Conversations c 
    JOIN Users u1 ON c.user_id_1 = u1.user_id
    JOIN Users u2 ON c.user_id_2 = u2.user_id
    WHERE c.user_id_1 = ? OR c.user_id_2 = ? 
    ORDER BY c.last_updated DESC;)sql";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        dbError = true;
    }

    if (sqlite3_bind_int(stmt, 1, user_id) != SQLITE_OK) {
        dbError = true;
    }
    if (sqlite3_bind_int(stmt, 2, user_id) != SQLITE_OK) {
        dbError = true;
    }
    if (sqlite3_bind_int(stmt, 3, user_id) != SQLITE_OK) {
        dbError = true;
    }

    bool atLeastOneRow = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int conversation_id = sqlite3_column_int(stmt, 0);
        const unsigned char* other_user_name = sqlite3_column_text(stmt, 1);
        const unsigned char* last_updated = sqlite3_column_text(stmt, 2);

        // int blank_space = 5;
        // int start = user_id;
        // while (start / 10 > 0) {
        //     start = start / 10;
        //     blank_space--;
        // }
        // std::string space(blank_space, ' ');
        
        ss << "CID: " << conversation_id << " With: " << other_user_name << " Updated: " << last_updated << std::endl; 
        atLeastOneRow = true;
    }

    sqlite3_finalize(stmt);
    data = ss.str();

    if (dbError) {
        success = "false";
        message = "Database Error: " + std::string(sqlite3_errmsg(db));
        data = "";
    } else if (!atLeastOneRow) {
        success = "false";
        message = "There are no conversations for the current account.";
        data = "";
    }

    response = success + "|" + message + "|" + data;
}

void GetConversation(std::string& response, const int& user_id, const int& conversation_id, sqlite3* db) {
    std::stringstream ss;
    std::string success = "true";
    std::string message = "";
    std::string data = "";

    bool dbError = false;
    int other_user_id;
    std::string other_user_name;
    sqlite3_stmt* stmt1;
    const char* sql1 = R"sql(
    SELECT
        CASE 
            WHEN c.user_id_1 = ? THEN c.user_id_2
            ELSE c.user_id_1
        END AS other_user_id,

        CASE
            WHEN c.user_id_1 = ? THEN u2.user_name
            ELSE u1.user_name
        END AS other_user_name

    FROM Conversations c
    JOIN Users u1 ON c.user_id_1 = u1.user_id
    JOIN Users u2 ON c.user_id_2 = u2.user_id
    WHERE c.conversation_id = ?;
    )sql";

    if (sqlite3_prepare_v2(db, sql1, -1, &stmt1, nullptr) != SQLITE_OK) {
        dbError = true;
    }

    if (sqlite3_bind_int(stmt1, 1, user_id) != SQLITE_OK) {
        dbError = true;
    }

    if (sqlite3_bind_int(stmt1, 2, user_id) != SQLITE_OK) {
        dbError = true;
    }

    if (sqlite3_bind_int(stmt1, 3, conversation_id) != SQLITE_OK) {
        dbError = true;
    }

    if (sqlite3_step(stmt1) == SQLITE_ROW) {
        other_user_id = sqlite3_column_int(stmt1, 0);
        other_user_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt1, 1));
    } else {
        dbError = true;
    }

    sqlite3_finalize(stmt1);

    std::string user_name;
    sqlite3_stmt* stmt2;
    const char* sql2 = R"sql(SELECT user_name FROM Users WHERE user_id = ?;)sql";

    if (sqlite3_prepare_v2(db, sql2, -1, &stmt2, nullptr) != SQLITE_OK) {
        dbError = true;
    }

    if (sqlite3_bind_int(stmt2, 1, user_id) != SQLITE_OK) {
        dbError = true;
    }

    if (sqlite3_step(stmt2) == SQLITE_ROW) {
        user_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt2, 0));
    } else {
        dbError = true;
    }

    sqlite3_finalize(stmt2);

    ss << "----- Messaging with " << other_user_name << " -----" << std::endl;

    sqlite3_stmt* stmt3;
    const char* sql3 = R"sql(SELECT * FROM Messages WHERE conversation_id = ? ORDER BY created_at)sql";

    if (sqlite3_prepare_v2(db, sql3, -1, &stmt3, nullptr) != SQLITE_OK) {
        dbError = true;
    }

    if (sqlite3_bind_int(stmt3, 1, conversation_id) != SQLITE_OK) {
        dbError = true;
    }

    bool atLeastOneRow = false;
    while (sqlite3_step(stmt3) == SQLITE_ROW) {
        int sender_id = sqlite3_column_int(stmt3, 1);
        const unsigned char* message = sqlite3_column_text(stmt3, 3);

        if (sender_id == user_id) {
            ss << user_name << ": " << message << std::endl;
        } else {
            ss << other_user_name << ": " << message << std::endl;
        }

        atLeastOneRow = true;
    }

    sqlite3_finalize(stmt3);
    data = ss.str();

    if (dbError) {
        success = "false";
        message = "Database Error: " + std::string(sqlite3_errmsg(db));
        data = "";
    } else if (!atLeastOneRow) {
        success = "false";
        message = "There are no messages in the current conversation.";
        data = "";
    }

    response = success + "|" + message + "|" + data;
}

void SetupSignalHandling() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);

    // Set up SIGINT handler
    sa.sa_handler = sigint_handler;
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Failed to set SIGINT handler");
        exit(1);
    }
}

void sigint_handler(int sig) {
    shutdown_requested = 1;
    close(listen_sockfd);
}

Request RecieveRequest(int client_fd) {
    // read request header from client
    size_t request_length;
    size_t total_read = 0;
    char request_length_buf[4];
    while (total_read < 4) {
        ssize_t read = recv(client_fd, request_length_buf + total_read, 4 - total_read, 0);
        if (read == -1) {
            throw std::runtime_error("Error receiving request header: " + std::string(strerror(errno)));
        } else if (read == 0) {
            throw std::runtime_error("Connection closed while receiving request");
        }
        total_read += read;
    }
    memcpy(&request_length, request_length_buf, 4);
    request_length = ntohl(request_length);

    // now read message from client
    std::string req(request_length, '\0');
    total_read = 0;
    while (total_read < request_length) {
        ssize_t read = recv(client_fd, &req[0] + total_read, request_length - total_read, 0);
        if (read == -1) {
            throw std::runtime_error("Error receiving request: " + std::string(strerror(errno)));
        } else if (read == 0) {
            throw std::runtime_error("Connection closed while recieving request");
        }
        total_read += read;
    }

    Request request = Request::parseRequest(req);
    return request;
}

void SendResponse(int client_fd, const std::string& response) {
    // get length header details
    uint32_t length = htonl(response.length());
    char length_buf[4];
    memcpy(length_buf, &length, 4);

    // send header
    size_t total_sent = 0;
    while (total_sent < 4) {
        ssize_t sent = send(client_fd, length_buf + total_sent, 4 - total_sent, 0);
        if (sent == -1) {
            throw std::runtime_error("Error sending response header: " + std::string(strerror(errno)));
        }
        total_sent += sent;
    }

    // now send message
    total_sent = 0;
    while (total_sent < response.length()) {
        ssize_t sent = send(client_fd, response.c_str() + total_sent, response.length() - total_sent, 0);
        if (sent == -1) {
            throw std::runtime_error("Error sending response body: " + std::string(strerror(errno)));
        }
        total_sent += sent;
    }
}