#include <sstream>
#include "common.h"

#pragma once

std::string GetAccounts(sqlite3* db);

void handle_client(int client_fd);
Request RecieveRequest(int client_fd);
void SendResponse(int client_fd, const std::string& response);

void SetupSignalHandling();
void sigint_handler(int sig);
