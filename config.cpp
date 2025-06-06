#include "config.h"

Config::Config(){
    //端口号,默认9006
    PORT = 9006;

    //日志写入方式，默认同步
    LOGWrite = 0;

    //触发组合模式,默认listenfd LT + connfd LT
    TRIGMode = 0;

    //listenfd触发模式，默认LT
    LISTENTrigmode = 0;

    //connfd触发模式，默认LT
    CONNTrigmode = 0;

    //优雅关闭链接，默认不使用
    OPT_LINGER = 0;

    //数据库连接池数量,默认8
    sql_num = 8;

    //线程池内的线程数量,默认8
    thread_num = 8;

    //关闭日志,默认不关闭
    close_log = 0;

    //并发模型,默认是proactor
    actor_model = 0;
}

void Config::parse_arg(int argc, char*argv[]){
    int opt;
    const char *str = "p:l:m:o:s:t:c:a:";
    // 循环解析命令行参数，直到没有更多的选项
    // 标准库函数，用于解析命令行选项
    while ((opt = getopt(argc, argv, str)) != -1)
    {
        switch (opt)
        {
            // 端口号
        case 'p':
        {
            PORT = atoi(optarg);
            break;
        }
            // 日志写入方式
        case 'l':
        {
            LOGWrite = atoi(optarg);
            break;
        }
            // 触发组合模式
        case 'm':
        {
            TRIGMode = atoi(optarg);
            break;
        }
            // 优雅关闭连接选项
        case 'o':
        {
            OPT_LINGER = atoi(optarg);
            break;
        }
            // 数据库连接池数量
        case 's':
        {
            sql_num = atoi(optarg);
            break;
        }
            // 线程池内的线程数量
        case 't':
        {
            thread_num = atoi(optarg);
            break;
        }
            // 是否关闭日志
        case 'c':
        {
            close_log = atoi(optarg);
            break;
        }
            // 并发模型
        case 'a':
        {
            actor_model = atoi(optarg);
            break;
        }
        default:
            break;
        }
    }
}
