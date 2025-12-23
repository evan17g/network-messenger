CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude

client:
	$(CXX) $(CXXFLAGS) src/client.cpp -o out/client
	./out/client

server:
	$(CXX) $(CXXFLAGS) src/server.cpp -o out/server
	./out/server

clean:
	rm -f client server