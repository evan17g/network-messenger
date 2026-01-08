#include <sstream>
#include "common.h"

#pragma once

void GetAccounts(std::string& response, sqlite3* db);
void AddAccount(std::string& response, const int user_id, std::string user_name, sqlite3* db);
void GetConversations(std::string& response, const int user_id, sqlite3* db);
void GetConversation(std::string& response, const int user_id, const int conversation_id, sqlite3* db);
void AddConversation(std::string& response, const int user_id, const int other_user_id, sqlite3* db);
void SendMessage(std::string& response, const int user_id, const int conversation_id, const std::string& message, sqlite3* db);
void SendBroadcast(int client_fd, int conversation_id);

void handle_client(int client_fd);
Request RecieveRequest(int client_fd);
void SendResponse(int client_fd, const std::string& response);

void SetupSignalHandling();
void sigint_handler(int sig);
