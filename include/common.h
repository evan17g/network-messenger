#ifndef _COMMON_H_
#define _COMMON_H_

#include <string>
#include <vector>
#include <chrono>

enum RequestType {
    QUIT,
    GET_ACCOUNTS,
    GET_CONVERSATIONS,
    GET_CONVERSATION,
    SEND_MESSAGE
};

RequestType parseRequestType(const std::string& s) {
    if (s == "QUIT") return QUIT;
    if (s == "GET_ACCOUNTS") return GET_ACCOUNTS;
    if (s == "GET_CONVERSATIONS") return GET_CONVERSATIONS;
    if (s == "GET_CONVERSATION") return GET_CONVERSATION;
    if (s == "SEND_MESSAGE") return SEND_MESSAGE;
    throw std::invalid_argument("Unknown request type: " + s);
}

struct Request {
    RequestType type;
    int user_id;
    int conversation_id;

    Request(RequestType t, int uid, int cid) : 
            type(t), user_id(uid), conversation_id(cid) {}

    static Request parseRequest(const std::string& buffer) {
        std::vector<std::string> parts;
        size_t pos = 0;
        std::string str = buffer;
        const std::string delimiter = "|";
        
        while ((pos = str.find(delimiter)) != std::string::npos) {
            parts.push_back(str.substr(0, pos));
            str.erase(0, pos + delimiter.length());
        }
        parts.push_back(str);

        if (parts.size() != 3) {
            return Request(QUIT, 0, 0); // Return a default QUIT request if parsing fails
        }

        RequestType type = parseRequestType(parts[0]);

        if (type < 0 || type > 4) {
            return Request(QUIT, 0, 0); // Return a default QUIT request if parsing fails
        }
        
        int user_id = std::stoi(parts[1]);
        int conversation_id = std::stoi(parts[2]);
        
        return Request(type, user_id, conversation_id);
    }
};

struct Response {
    bool success;
    std::string data;
    std::string message;

    Response(bool s = false, std::string d = "", std::string m = "") :
            success(s), data(d), message(m) {}
};

#endif