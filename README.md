This is my attempt at making a simple message application. There are two .cpp files that you can choose to run: client.cpp and server.cpp. The server file connects to the underlying sqlite3 database whereas the client file connects to the server to make requests.
The goal is that multiple clients can set up accounts, and send messages to each other via the server.
