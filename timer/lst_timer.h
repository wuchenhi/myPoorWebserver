#ifndef m_LST_TIMER
#define m_LST_TIMER

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <vector>
#include <time.h>
#include "spdlog/spdlog.h"

using namespace std;

const int BUFFER_SIZE = 64;
//利用alarm函数周期性地触发SIGALRM信号,
//该信号的信号处理函数利用 管道 通知主循环执行定时器链表上的定时任务

//连接资源结构体成员需要用到定时器类 前向声明
class heap_timer;

//定时器和socke绑定
struct client_data {
    sockaddr_in address;  //客户端socket地址
    int sockfd;           //socket文件描述符
    heap_timer *timer;    //定时器
    //char buf[BUFFER_SIZE];
};

//定时器类
class heap_timer {
public:
    heap_timer(int delay) {
        expire = time(NULL) + delay;
    }

public:
    time_t expire;                  //超时时间(绝对时间)
    void (* cb_func)(client_data *);//回调函数
    client_data *user_data;
};

//定时器容器类
class timer_heap {
public:
    timer_heap() {
        vec.reserve(64);
    }

    ~timer_heap();
    //添加定时器
    void add_timer(heap_timer *timer);
    //heap
    //void doWork(int id);
    void adjust_timer(heap_timer *timer);
    void pop_timer();        
    //int GetNextTick(); //?
    //删除定时器
    void del_timer(heap_timer *timer);
    //定时任务处理函数
    void tick();
    bool empty() const { return vec.empty(); }
    //类似重载 []
    heap_timer* at(int index) {
        return vec[index];
    }
private:
    //最小堆的下虑操作，确保数组中以底hole个节点为根的子树拥有最小堆性质
    void percolate_down(int hole);

    //上滤 下滤 TODO
    //void del_(size_t i);
    
    //void siftup_(size_t i);

    //bool siftdown_(size_t index, size_t n);

    //void SwapNode_(size_t i, size_t j);

    //std::vector<TimerNode> heap_;
    //std::unordered_map<int, size_t> ref_;
    vector<heap_timer* > vec;
};

class Utils {
public:
    Utils() = default;

    ~Utils() = default;

    void init(int timeslot);

    //对文件描述符设置非阻塞
    int setnonblocking(int fd);

    //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot);

    //信号处理函数
    static void sig_handler(int sig);

    //设置信号函数
    void addsig(int sig, void(handler)(int), bool restart = true);

    //定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();

    void show_error(int connfd, const char *info);

public:
    static int *u_pipefd;
    timer_heap m_timer_lst;
    static int u_epollfd;
    int m_TIMESLOT;
};

void cb_func(client_data *user_data);

#endif
