# ZMQ测试程序Makefile

CXX = g++
CXXFLAGS = -std=c++11 -Wall -pthread
LIBS = -lzmq

# 目标文件
TARGETS = zmqServerTest zmqClientTest safeQueueTest

# 默认目标
all: $(TARGETS)

# 服务器测试程序
zmqServerTest: zmqServerTest.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LIBS)

# 客户端测试程序  
zmqClientTest: zmqClientTest.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LIBS)

# 安全队列测试程序
safeQueueTest: safeQueueTest.cpp
	$(CXX) $(CXXFLAGS) -I../src -o $@ $< $(LIBS)

# 清理
clean:
	rm -f $(TARGETS)

# 安装ZMQ依赖 (Ubuntu/Debian)
install-deps:
	sudo apt-get update
	sudo apt-get install libzmq3-dev

# 运行服务器
run-server: zmqServerTest
	./zmqServerTest

# 运行客户端 (本地)
run-client: zmqClientTest
	./zmqClientTest

# 运行客户端 (指定服务器IP)
run-client-remote: zmqClientTest
	@echo "Usage: make run-client-remote SERVER_IP=<ip_address>"
	./zmqClientTest $(SERVER_IP)

# 帮助信息
help:
	@echo "可用目标:"
	@echo "  all           - 编译所有程序"
	@echo "  zmqServerTest - 编译服务器测试程序"
	@echo "  zmqClientTest - 编译客户端测试程序"
	@echo "  clean         - 清理编译文件"
	@echo "  install-deps  - 安装ZMQ依赖库"
	@echo "  run-server    - 运行服务器"
	@echo "  run-client    - 运行客户端(连接localhost)"
	@echo "  help          - 显示此帮助信息"
	@echo ""
	@echo "运行客户端连接远程服务器示例:"
	@echo "  make run-client-remote SERVER_IP=192.168.1.100"

.PHONY: all clean install-deps run-server run-client run-client-remote help
