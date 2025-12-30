#pragma once

void SendRequest(int sockfd, const std::string& request);
std::string RecieveResponse(int sockfd);