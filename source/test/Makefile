CXX = g++
BUILD_DIR = ../../build
INSTALL_DIR = $(BUILD_DIR)/release-install-cpp11
CFLAG = -std=c++11 -pthread -I $(INSTALL_DIR)/include
LFLAG = -L$(INSTALL_DIR)/lib -ljsoncpp -lmuduo_net -lmuduo_base -pthread
HEADERS = $(wildcard ../common/*.hpp ../server/*.hpp ../client/*.hpp)
TEST_DIR = $(BUILD_DIR)/tests
ROUTER_TEST = $(TEST_DIR)/rpc_router_tests
REQUESTOR_TEST = $(TEST_DIR)/requestor_tests
CALLER_TEST = $(TEST_DIR)/rpc_caller_tests

all: client server
server : test_server.cpp $(HEADERS)
	$(CXX) $(CFLAG) -o $@ $< $(LFLAG)
client : test_client.cpp $(HEADERS)
	$(CXX) $(CFLAG) -o $@ $< $(LFLAG)

$(TEST_DIR):
	mkdir -p $@

$(ROUTER_TEST): ../server/test_rpc_router.cpp $(HEADERS) | $(TEST_DIR)
	$(CXX) $(CFLAG) -o $@ $< -ljsoncpp -pthread

test-rpc-router: $(ROUTER_TEST)
	$(ROUTER_TEST)

$(REQUESTOR_TEST): ../client/test_requestor.cpp $(HEADERS) | $(TEST_DIR)
	$(CXX) $(CFLAG) -o $@ $< -ljsoncpp -pthread

test-requestor: $(REQUESTOR_TEST)
	$(REQUESTOR_TEST)

$(CALLER_TEST): ../client/test_rpc_caller.cpp $(HEADERS) | $(TEST_DIR)
	$(CXX) $(CFLAG) -o $@ $< -ljsoncpp -pthread

test-rpc-caller: $(CALLER_TEST)
	$(CALLER_TEST)

.PHONY: all client server clean test-rpc-router test-requestor test-rpc-caller
clean:
	rm -f server client $(ROUTER_TEST) $(REQUESTOR_TEST) $(CALLER_TEST)
