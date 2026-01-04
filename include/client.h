#include <sstream>
#include "common.h"

#pragma once

enum STATE {
    HOME,
    CONVERSATION
};

void SendRequest(int sockfd, const std::string& request);
Response RecieveResponse(int sockfd);