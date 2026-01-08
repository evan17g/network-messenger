#ifndef _COMMON_H_
#define _COMMON_H_

#include <string>
#include <vector>
#include <chrono>

// defining a custom exception to make sure requests are valid
class InvalidRequestException : public std::runtime_error {
public:
    // Use the base class constructor
    explicit InvalidRequestException(const std::string& message)
        : std::runtime_error(message) {}
};

enum RequestType {
    QUIT,
    GET_ACCOUNTS,
    ADD_ACCOUNT,
    GET_CONVERSATIONS,
    GET_CONVERSATION,
    ADD_CONVERSATION,
    SEND_MESSAGE,
    UPDATE_CONVERSATION
};

enum ResponseType {
    BROADCAST,
    RESPONSE
};

RequestType parseRequestType(const std::string& s) {
    if (s == "QUIT") return QUIT;
    if (s == "GET_ACCOUNTS") return GET_ACCOUNTS;
    if (s == "ADD_ACCOUNT") return ADD_ACCOUNT;
    if (s == "GET_CONVERSATIONS") return GET_CONVERSATIONS;
    if (s == "GET_CONVERSATION") return GET_CONVERSATION;
    if (s == "ADD_CONVERSATION") return ADD_CONVERSATION;
    if (s == "SEND_MESSAGE") return SEND_MESSAGE;
    if (s == "UPDATE_CONVERSATION") return UPDATE_CONVERSATION;
    throw std::invalid_argument("Unknown request type: " + s);
}

std::string stringifyType(const RequestType& r) {
    if (r == QUIT) return "QUIT";
    if (r == GET_ACCOUNTS) return "GET_ACCOUNTS";
    if (r == ADD_ACCOUNT) return "ADD_ACCOUNT";
    if (r == GET_CONVERSATIONS) return "GET_CONVERSATIONS";
    if (r == GET_CONVERSATION) return "GET_CONVERSATION";
    if (r == ADD_CONVERSATION) return "ADD_CONVERSATION";
    if (r == SEND_MESSAGE) return "SEND_MESSAGE";
    if (r == UPDATE_CONVERSATION) return "UPDATE_CONVERSATION";
    throw std::invalid_argument("Unknown request type: " + r);
}

std::string stringifyResponseType(const ResponseType& r) {
    if (r == BROADCAST) return "BROADCAST";
    if (r == RESPONSE) return "RESPONSE";
    throw std::invalid_argument("Unknown response type: " + r);
}

std::string boolToString(bool b) {
    if (b) {
        return "true";
    } else {
        return "false";
    }
}

struct Request {
    RequestType type;
    int user_id;
    int conversation_id;
    std::string user_name;

    Request(RequestType t, int uid, int cid, std::string un) : 
            type(t), user_id(uid), conversation_id(cid), user_name(un) {}

    std::string to_string() {
        return stringifyType(type) + "|" + std::to_string(user_id) + "|" + std::to_string(conversation_id) + "|" + user_name;
    }

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
            throw InvalidRequestException("Request sent was invalid!");
        }

        RequestType type = parseRequestType(parts[0]);

        if (type < 0 || type > 7) {
            throw InvalidRequestException("Request sent was invalid!");
        }
        
        int user_id = std::stoi(parts[1]);
        int conversation_id = std::stoi(parts[2]);
        std::string user_name = parts[3];
        
        return Request(type, user_id, conversation_id, user_name);
    }
};

struct Response {
    ResponseType type;
    bool success;
    std::string data;
    std::string message;
    int conversation_id;

    Response(ResponseType t, bool s, std::string m, std::string d, int cid) :
            type(t), success(s), message(m), data(d), conversation_id(cid) {}
    
    std::string to_string() {
        return stringifyResponseType(type) + "|" + boolToString(success) + "|" + message + "|" + data + "|" + std::to_string(conversation_id);
    }

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

        if (parts.size() != 5) {
            return Response(RESPONSE, false, "", "Failed to parse response.", 0);
        }

        ResponseType type;
        if (parts[0] == "RESPONSE") {
            type = RESPONSE;
        } else if (parts[0] == "BROADCAST") {
            type = BROADCAST;
        } else {
            return Response(RESPONSE, false, "", "Failed to parse response.", 0);
        }

        std::istringstream is(parts[1]);
        bool success;
        is >> std::boolalpha >> success;
        if (is.fail()) {
            return Response(RESPONSE, false, "", "Failed to parse response.", 0);
        }
        std::string message = parts[2];
        std::string data = parts[3];
        int conversation_id = std::stoi(parts[4]);
        
        return Response(type, success, message, data, conversation_id);
    }
};

#endif