#include "config.h"

int main(int argc, char *argv[])
{
    //需要修改的数据库信息,登录名,密码,库名
    string user = "root";
    string passwd = "root";
    string databasename = "qgydb";

    //命令行解析
    Config config;
    config.parse_arg(argc, argv);

    WebServer server;

    //初始化
    // 网络端口port、数据库认证信息、日志开关LOGWrite、TCP选项OPT_LINGER、触发模式TRIGMode、数据库连接数sql_num
    // 线程数 thread_num、日志开关close_log 、事件模型actor_model
    server.init(config.PORT, user, passwd, databasename, config.LOGWrite, 
                config.OPT_LINGER, config.TRIGMode,  config.sql_num,  config.thread_num, 
                config.close_log, config.actor_model);
    

    //日志
    server.log_write();

    //数据库
    server.sql_pool();

    //线程池
    server.thread_pool();

    //触发模式
    server.trig_mode();

    //监听
    server.eventListen();

    //运行
    server.eventLoop();

    return 0;

    /*
    graph TD
    A[main启动] --> B[解析命令行]
    B --> C[初始化服务器]
    C --> D[启动日志系统]
    D --> E[初始化数据库连接池]
    E --> F[创建线程池]
    F --> G[设置触发模式]
    G --> H[开始监听端口]
    H --> I[进入事件循环]
    */
}
