#ifndef _COMMON_H_
#define _COMMON_H_

#include <string>
#include <vector>
#include <chrono>

enum RequestType {
    QUIT,
    GET_ACCOUNTS,
    ADD_ACCOUNT,
    GET_CONVERSATIONS,
    GET_CONVERSATION,
    SEND_MESSAGE
};

RequestType parseRequestType(const std::string& s) {
    if (s == "QUIT") return QUIT;
    if (s == "GET_ACCOUNTS") return GET_ACCOUNTS;
    if (s == "ADD_ACCOUNT") return ADD_ACCOUNT;
    if (s == "GET_CONVERSATIONS") return GET_CONVERSATIONS;
    if (s == "GET_CONVERSATION") return GET_CONVERSATION;
    if (s == "SEND_MESSAGE") return SEND_MESSAGE;
    throw std::invalid_argument("Unknown request type: " + s);
}

struct Request {
    RequestType type;
    int user_id;
    int conversation_id;
    std::string user_name;

    Request(RequestType t, int uid, int cid, std::string un) : 
            type(t), user_id(uid), conversation_id(cid), user_name(un) {}

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

        if (parts.size() != 4) {
            return Request(QUIT, 0, 0, ""); // Return a default QUIT request if parsing fails
        }

        RequestType type = parseRequestType(parts[0]);

        if (type < 0 || type > 4) {
            return Request(QUIT, 0, 0, ""); // Return a default QUIT request if parsing fails
        }
        
        int user_id = std::stoi(parts[1]);
        int conversation_id = std::stoi(parts[2]);
        std::string user_name = parts[3];
        
        return Request(type, user_id, conversation_id, user_name);
    }
};

struct Response {
    bool success;
    std::string data;
    std::string message;

    Response(bool s, std::string m, std::string d) :
            success(s), message(m), data(d) {}

    static Response parseResponse(const std::string& buffer) {
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
            return Response(false, "", "Failed to parse response.");
        }

        std::istringstream is(parts[0]);
        bool success;
        is >> std::boolalpha >> success;
        if (is.fail()) {
            return Response(false, "", "Failed to parse response.");
        }
        std::string message = parts[1];
        std::string data = parts[2];
        
        return Response(success, message, data);
    }
};

#endif