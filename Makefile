
# Makefile

default: server.cpp client.cpp
	mkdir -p bin/
	g++ -Os -Wall -std=c++1z -o ./bin/server server.cpp -lpthread -lrt
	g++ -Os -Wall -std=c++1z -o ./bin/client client.cpp -lpthread -lrt
