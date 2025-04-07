#include "webserver.h"
// 基于 epoll 的多线程 Web 服务器。它涵盖了网络编程、多线程处理、定时器管理、日志记录等多个功能模块
WebServer::WebServer()
{
    //http_conn类对象
    users = new http_conn[MAX_FD]; //预分配连接对象 用于存储http_conn对象，每个对象表示一个客户端连接

    //root文件夹路径
    char server_path[200]; //定义一个字符数组，用于存储当前工作目录的路径
    getcwd(server_path, 200); //获取当前工作目录的路径
    char root[6] = "/root"; //定义一个字符串，表示服务器的根目录路径
    // strlen 用于计算字符串长度，malloc 用于动态分配内存
    m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1); //动态分配内存，用于存储完整的根目录路径
    // 将当前工作目录路径复制到 m_root 中
    strcpy(m_root, server_path);
    // 将 /root 拼接到 m_root 的末尾，形成完整的根目录路径
    strcat(m_root, root);

    //定时器
    users_timer = new client_data[MAX_FD];
}

WebServer::~WebServer()
{
    // 关闭 epoll 文件描述符
    close(m_epollfd);
    // 关闭监听套接字
    close(m_listenfd);
    // 关闭管道文件描述符的写端
    close(m_pipefd[1]);
    // 关闭管道文件描述符的读端
    close(m_pipefd[0]);
    // 释放 users 数组占用的内存
    delete[] users;
    // 释放 users_timer 数组占用的内存
    delete[] users_timer;
    // 释放线程池对象占用的内存
    delete m_pool;
}

void WebServer::init(int port, string user, string passWord, string databaseName, int log_write, 
                     int opt_linger, int trigmode, int sql_num, int thread_num, int close_log, int actor_model)
{
    m_port = port;     //服务器监听的端口号
    m_user = user;      //数据库用户名
    m_passWord = passWord;    //数据库密码
    m_databaseName = databaseName;    //数据库名称
    m_sql_num = sql_num;    //数据库连接池的大小
    m_thread_num = thread_num;    //线程池的大小
    m_log_write = log_write;    //日志写入方式
    m_OPT_LINGER = opt_linger;    //SO_LINGER 选项的设置
    m_TRIGMode = trigmode;    //触发模式（LT/ET）
    m_close_log = close_log;    //是否关闭日志
    m_actormodel = actor_model;     //使用的模型（Reactor/Proactor）
}
// 根据 m_TRIGMode 的值设置监听套接字和连接套接字的触发模式
// LT（Level Triggered）：水平触发，只要有事件就会一直触发
// ET（Edge Triggered）：边缘触发，只有在状态发生变化时才会触发
void WebServer::trig_mode()
{
    //LT + LT
    if (0 == m_TRIGMode)
    {
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 0;
    }
    //LT + ET
    else if (1 == m_TRIGMode)
    {
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 1;
    }
    //ET + LT
    else if (2 == m_TRIGMode)
    {
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 0;
    }
    //ET + ET
    else if (3 == m_TRIGMode)
    {
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 1;
    }
}
// 根据配置初始化日志系统
// Log::get_instance()：获取日志系统的单例对象，init：初始化日志文件路径、日志级别、日志缓冲区大小等参数
void WebServer::log_write()
{
    if (0 == m_close_log)
    {
        //初始化日志
        if (1 == m_log_write)
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 800);
        else
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 0);
    }
}

void WebServer::sql_pool()
{
    //初始化数据库连接池
    m_connPool = connection_pool::GetInstance();  //获取数据库连接池的单例对象
    // 初始化数据库连接池，连接到 MySQL 数据库
    m_connPool->init("localhost", m_user, m_passWord, m_databaseName, 3306, m_sql_num, m_close_log);

    //初始化数据库读取表
    users->initmysql_result(m_connPool);
}
// 创建一个线程池，用于处理客户端请求
void WebServer::thread_pool()
{
    //线程池
    // 使用的模型（Reactor/Proactor）、数据库连接池对象、线程池的大小
    m_pool = new threadpool<http_conn>(m_actormodel, m_connPool, m_thread_num);
}

void WebServer::eventListen()
{
    //网络编程基础步骤
    
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);        // 创建一个 TCP 套接字
    assert(m_listenfd >= 0);

    //优雅关闭连接
    if (0 == m_OPT_LINGER)
    {
        struct linger tmp = {0, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));  //设置套接字的 SO_LINGER 选项，控制关闭套接字时的行为
    }
    else if (1 == m_OPT_LINGER)
    {
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    int ret = 0;
    //定义一个 sockaddr_in 结构体，用于存储服务器的地址信息
    struct sockaddr_in address;
    // 将 address 结构体清零
    bzero(&address, sizeof(address));
    // 设置地址族为 IPv4
    address.sin_family = AF_INET;
    // 设置服务器可以监听所有网络接口的地址
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    // 设置服务器监听的端口号
    address.sin_port = htons(m_port);

    int flag = 1;
    // 设置套接字的 SO_REUSEADDR 选项，允许端口复用
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    // 将套接字绑定到指定的地址和端口
    ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    // 将套接字设置为监听状态，最大连接队列长度为 5
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);
    // 初始化工具类，设置定时器的时间间隔
    utils.init(TIMESLOT);

    //epoll创建内核事件表
    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(5);
    // 创建一个 epoll 实例，用于事件监听
    assert(m_epollfd != -1);
    // 将监听套接字添加到 epoll 实例中
    utils.addfd(m_epollfd, m_listenfd, false, m_LISTENTrigmode);
    // 将 epoll 文件描述符存储到 http_conn 类中，供后续使用
    http_conn::m_epollfd = m_epollfd;
    // 创建一个管道，用于信号处理
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    // 将管道的写端设置为非阻塞模式
    utils.setnonblocking(m_pipefd[1]);
    // 将管道的读端添加到 epoll 实例中
    utils.addfd(m_epollfd, m_pipefd[0], false, 0);
    // 忽略 SIGPIPE 信号，防止写入已关闭的管道时产生信号
    utils.addsig(SIGPIPE, SIG_IGN);
    // 为 SIGALRM 信号设置处理函数
    utils.addsig(SIGALRM, utils.sig_handler, false);
    // 为 SIGTERM 信号设置处理函数
    utils.addsig(SIGTERM, utils.sig_handler, false);
    // 设置定时器，每隔 TIMESLOT 秒触发一次 SIGALRM 信号
    alarm(TIMESLOT);

    //工具类,信号和描述符基础操作
    // 将管道文件描述符存储到工具类中
    Utils::u_pipefd = m_pipefd;
    // 将 epoll 文件描述符存储到工具类中
    Utils::u_epollfd = m_epollfd;
}

void WebServer::timer(int connfd, struct sockaddr_in client_address)
{
    // 初始化 http_conn 对象，设置连接的文件描述符、客户端地址、根目录等信息
    users[connfd].init(connfd, client_address, m_root, m_CONNTrigmode, m_close_log, m_user, m_passWord, m_databaseName);

    //初始化client_data数据
    //创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
    users_timer[connfd].address = client_address;
    // 将连接的文件描述符存储到 client_data 结构体中
    users_timer[connfd].sockfd = connfd;
    // 创建一个定时器对象
    util_timer *timer = new util_timer;
    // 将 client_data 结构体的地址存储到定时器的用户数据中
    timer->user_data = &users_timer[connfd];
    // 设置定时器的回调函数
    timer->cb_func = cb_func;
    // 获取当前时间
    time_t cur = time(NULL);
    // 设置定时器的超时时间，为当前时间加上 3 个时间间隔
    timer->expire = cur + 3 * TIMESLOT;
    // 将定时器对象存储到 client_data 结构体中
    users_timer[connfd].timer = timer;
    // 将定时器添加到定时器链表中
    utils.m_timer_lst.add_timer(timer);
}

//若有数据传输，则将定时器往后延迟3个单位
//并对新的定时器在链表上的位置进行调整
void WebServer::adjust_timer(util_timer *timer)
{
    // 获取当前时间
    time_t cur = time(NULL);
    // 将定时器的超时时间设置为当前时间加上 3 个时间间隔
    timer->expire = cur + 3 * TIMESLOT;
    // 调整定时器在链表中的位置
    utils.m_timer_lst.adjust_timer(timer);
    // 记录日志，表示定时器调整了一次
    LOG_INFO("%s", "adjust timer once");
}

void WebServer::deal_timer(util_timer *timer, int sockfd)
{
    // 调用定时器的回调函数，处理超时事件
    timer->cb_func(&users_timer[sockfd]);
    if (timer)
    {
        // 从定时器链表中删除定时器
        utils.m_timer_lst.del_timer(timer);
    }
    // 记录日志，表示关闭了文件描述符
    LOG_INFO("close fd %d", users_timer[sockfd].sockfd);
}

bool WebServer::dealclientdata()
{
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    if (0 == m_LISTENTrigmode)
    {
        // 接受客户端连接，返回新的文件描述符
        int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
        // 检查 accept 是否失败
        if (connfd < 0)
        {
            LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;
        }
        // 检查服务器是否已达到最大连接数
        if (http_conn::m_user_count >= MAX_FD)
        {
            // 向客户端发送错误信息，表示服务器繁忙
            utils.show_error(connfd, "Internal server busy");
            LOG_ERROR("%s", "Internal server busy");
            return false;
        }
        // 为新连接创建定时器
        timer(connfd, client_address);
    }

    else
    {
        while (1)
        {
            int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
            if (connfd < 0)
            {
                LOG_ERROR("%s:errno is:%d", "accept error", errno);
                break;
            }
            if (http_conn::m_user_count >= MAX_FD)
            {
                utils.show_error(connfd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
                break;
            }
            timer(connfd, client_address);
        }
        return false;
    }
    return true;
}

bool WebServer::dealwithsignal(bool &timeout, bool &stop_server)
{
    int ret = 0;
    int sig;
    char signals[1024];
    // 从管道读端读取信号
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if (ret == -1)
    {
        return false;
    }
    else if (ret == 0)
    {
        return false;
    }
    else
    {
        for (int i = 0; i < ret; ++i)
        {
            // 根据信号类型设置相应的标志
            switch (signals[i])
            {
            case SIGALRM:
            {
                // 设置超时标志
                timeout = true;
                break;
            }
            case SIGTERM:
            {
                // 设置停止服务器标志
                stop_server = true;
                break;
            }
            }
        }
    }
    return true;
}

void WebServer::dealwithread(int sockfd)
{
    util_timer *timer = users_timer[sockfd].timer;

    //reactor
    if (1 == m_actormodel)
    {
        if (timer)
        {
            adjust_timer(timer);
        }

        //若监测到读事件，将该事件放入请求队列
        m_pool->append(users + sockfd, 0);

        while (true)
        {
            if (1 == users[sockfd].improv)
            {
                if (1 == users[sockfd].timer_flag)
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    else
    {
        //proactor
        // 处理读事件，读取客户端数据
        if (users[sockfd].read_once())
        {
            LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            //若监测到读事件，将该事件放入请求队列
            // 将读事件放入线程池的请求队列中（Proactor 模型）
            m_pool->append_p(users + sockfd);

            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}

void WebServer::dealwithwrite(int sockfd)
{
    // 获取连接的定时器对象
    util_timer *timer = users_timer[sockfd].timer;
    //reactor
    if (1 == m_actormodel)
    {
        if (timer)
        {
            // 调整定时器的超时时间
            adjust_timer(timer);
        }
        // 将写事件放入线程池的请求队列中
        m_pool->append(users + sockfd, 1);

        while (true)
        {
            if (1 == users[sockfd].improv)
            {
                if (1 == users[sockfd].timer_flag)
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    else
    {
        //proactor
        // 处理写事件，向客户端发送数据
        if (users[sockfd].write())
        {
            LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}

void WebServer::eventLoop()
{
    bool timeout = false;
    bool stop_server = false;

    while (!stop_server)
    {
        // 等待事件发生，epoll_wait 阻塞直到有事件发生
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR)
        {
            LOG_ERROR("%s", "epoll failure");
            break;
        }
        // 遍历所有发生的事件
        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;

            //处理新到的客户连接
            // 处理新连接
            if (sockfd == m_listenfd)
            {
                bool flag = dealclientdata();
                if (false == flag)
                    continue;
            }
            // 处理异常事件，如客户端断开连接
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                //服务器端关闭连接，移除对应的定时器
                util_timer *timer = users_timer[sockfd].timer;
                deal_timer(timer, sockfd);
            }
            //处理信号
            // 处理信号事件
            else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                bool flag = dealwithsignal(timeout, stop_server);
                if (false == flag)
                    LOG_ERROR("%s", "dealclientdata failure");
            }
            //处理客户连接上接收到的数据
            // 处理读事件
            else if (events[i].events & EPOLLIN)
            {
                dealwithread(sockfd);
            }
            // 处理写事件
            else if (events[i].events & EPOLLOUT)
            {
                dealwithwrite(sockfd);
            }
        }
        if (timeout)
        {
            // 处理定时器事件
            utils.timer_handler();

            LOG_INFO("%s", "timer tick");

            timeout = false;
        }
    }
}
