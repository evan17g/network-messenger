#include <sstream>
#include "common.h"

#pragma once

void GetAccounts(std::string& response, sqlite3* db);
void GetConversations(std::string& response, const int& user_id, sqlite3* db);
void GetConversation(std::string& response, const int& user_id, const int& conversation_id, sqlite3* db);

void handle_client(int client_fd);
Request RecieveRequest(int client_fd);
void SendResponse(int client_fd, const std::string& response);

void SetupSignalHandling();
void sigint_handler(int sig);
