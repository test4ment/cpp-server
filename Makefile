CXX = g++

CXXFLAGS = -std=c++17 -Wall

.PHONY: all
all: server client

server: server.cpp
	$(CXX) $(CXXFLAGS) server.cpp -o server

client: local_client.cpp
	$(CXX) $(CXXFLAGS) local_client.cpp -o client

.PHONY: clean
clean:
	rm -f server client *.o
