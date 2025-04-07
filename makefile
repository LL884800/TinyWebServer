# 设置默认的 C++ 编译器
CXX ?= g++
# 设置默认的调试模式
DEBUG ?= 1
# 根据 DEBUG 的值设置编译器的编译选项
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2

endif
# server 是目标名称，表示要生成的可执行文件
# main.cpp  ./timer/lst_timer.cpp ./http/http_conn.cpp ./log/log.cpp ./CGImysql/sql_connection_pool.cpp  webserver.cpp config.cpp 是构建 server 所依赖的源文件
# 是 Makefile 中的自动变量，表示所有依赖文件，$(CXX) 是编译器变量，表示使用的 C++ 编译器 -o server 指定输出文件的名称为 server
# $(CXXFLAGS) 是编译选项变量，包含之前定义的编译选项
# -lpthread 链接线程库，因为项目中可能使用了多线程
# -lmysqlclient 链接 MySQL 客户端库，因为项目中可能涉及数据库操作
server: main.cpp  ./timer/lst_timer.cpp ./http/http_conn.cpp ./log/log.cpp ./CGImysql/sql_connection_pool.cpp  webserver.cpp config.cpp
	$(CXX) -o server  $^ $(CXXFLAGS) -lpthread -lmysqlclient

# 定义如何清理生成的文件
clean:
	rm  -r server
