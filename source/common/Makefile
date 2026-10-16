CXX = g++
CFLAG = -std=c++11 -pthread -I ../build/release-install-cpp11/include/
LFLAG = -L../build/release-install-cpp11/lib -ljsoncpp -lmuduo_net -lmuduo_base -pthread
HEADERS = $(wildcard *.hpp ../server/*.hpp)
ROUTER_TEST = ../../build/tests/rpc_router_tests

all : client server
server : test_server.cpp $(HEADERS)
	$(CXX) $(CFLAG) -std=c++11 -o $@ $< $(LFLAG)
client : test_client.cpp $(HEADERS)
	$(CXX) $(CFLAG) -std=c++11 -o $@ $< $(LFLAG)

$(ROUTER_TEST): ../server/test_rpc_router.cpp $(HEADERS)
	mkdir -p ../../build/tests
	$(CXX) $(CFLAG) -o $@ $< -ljsoncpp -pthread

test-rpc-router: $(ROUTER_TEST)
	$(ROUTER_TEST)

.PHONY: clean test-rpc-router
clean:
	rm -f server client
