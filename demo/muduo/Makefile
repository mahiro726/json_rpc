CXX = g++
CFLAG = -std=c++11 -pthread -I ../../build/release-install-cpp11/include/
LFLAG = -L../../build/release-install-cpp11/lib -lmuduo_net -lmuduo_base -pthread

.PHONY: all clean

all: server client

server: my_server.cpp
	$(CXX) $(CFLAG) -o $@ $^ $(LFLAG)

client: my_client.cpp
	$(CXX) $(CFLAG) -o $@ $^ $(LFLAG)

clean:
	rm -f server client