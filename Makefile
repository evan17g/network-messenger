CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude
CC = gcc
CFLAGS = -Wall -Wextra

client:
	$(CXX) $(CXXFLAGS) src/client.cpp out/sqlite3.o -o out/client
	./out/client

server:
	$(CXX) $(CXXFLAGS) src/server.cpp out/sqlite3.o -o out/server
	./out/server

src/sqlite3.o: src/sqlite3.c
	$(CC) $(CFLAGS) -c src/sqlite3.c -o out/sqlite3.o

clean:
	rm -f client server