
# Makefile

default: test_publisher.cpp test_subscriber.cpp
	mkdir -p bin/
	g++ -Os -Wall -std=c++1z -o ./bin/test_publisher test_publisher.cpp -lpthread -lrt
	g++ -Os -Wall -std=c++1z -o ./bin/test_subscriber test_subscriber.cpp -lpthread -lrt

old: server.cpp client.cpp
	mkdir -p bin/
	g++ -Os -Wall -std=c++1z -o ./bin/server server.cpp -lpthread -lrt
	g++ -Os -Wall -std=c++1z -o ./bin/client client.cpp -lpthread -lrt

