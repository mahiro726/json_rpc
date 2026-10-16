CXX = g++
CFLAG = -std=c++11 -pthread -I ../build/release-install-cpp11/include/
LFLAG = -L../build/release-install-cpp11/lib -ljsoncpp -lmuduo_net -lmuduo_base -pthread

all : client server
server : test_server.cpp
	$(CXX) $(CFLAG) -std=c++11 -o $@ $^ $(LFLAG)
client : test_client.cpp
	$(CXX) $(CFLAG) -std=c++11 -o $@ $^ $(LFLAG)

.PHONY:clean
clean:
	rm -f server client