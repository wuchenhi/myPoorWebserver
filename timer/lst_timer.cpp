#include "lst_timer.h"
#include "../http/http_conn.h"

timer_heap::~timer_heap() {
    vec.clear();
}

void timer_heap::add_timer(heap_timer *timer) {
    if (!timer) {
        return;
    }
    vec.push_back(timer);
    int hole = vec.size() -1;
    int parent = 0;

    //对从空穴到根节点的路径上所有节点执行上虑操作
    for (; hole>0; hole = parent) {
        parent = (hole-1)/2;
        if (vec[parent]->expire <= timer->expire) {
            break;
        }
        vec[hole] = vec[parent];
    }
    vec[hole] = timer;
}

void timer_heap::del_timer(heap_timer *timer) {
    if (!timer) {
        return;
    }
    //仅仅将目标定时器的回调函数设为空，即延迟销毁。能真正节省删除定时器的开销，但容易使堆数组膨胀
    timer->cb_func = NULL;
}

void timer_heap::adjust_timer(heap_timer *timer) {
    // 调整指定id的结点
    //TODO  怎么知道这个timer的位置

    //percolate_down(ref_[id], heap_.size()); 
    percolate_down(0);
}

void timer_heap::pop_timer() {
    if(empty()) {
        return;
    }
    if(vec[0]) {
        vec[0] = (vec.back());
        percolate_down(0); /*对新的堆顶元素执行下虑操作*/
        vec.pop_back();
    }
}

void timer_heap::tick() {

    //获取当前时间
    time_t cur = time(NULL);
    heap_timer* tmp = vec[0];
    while (tmp) {
        if(!tmp) {
            break;
        }
        //如果堆定时器没到期，则退出循环
        if (tmp->expire > cur) {
            break;
        }
        //否则执行堆顶定时器中的任务
        if (vec[0] -> cb_func) {
            vec[0]->cb_func(vec[0]->user_data);//?
        }
        //堆顶定时器删除
        pop_timer();
        //生成新的堆顶定时器
        tmp = vec[0];
    }
}

void timer_heap::percolate_down(int hole) {
    heap_timer* temp = vec[hole];
    int child = 0;
    int cur_size = vec.size();
    for (;((hole*2+1) <= (cur_size-1)); hole = child) {
        child = hole * 2 + 1;
        if((child < (cur_size-1)) && (vec[child+1]->expire < vec[hole]->expire)) {
            ++child;
        }
        if (vec[child]->expire < temp->expire) {
            vec[hole] = vec[child];
        }
        else {
            break;
        }
    }
    vec[hole] = temp;
}

void Utils::init(int timeslot) {
    m_TIMESLOT = timeslot;
}

//对文件描述符设置非阻塞
int Utils::setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void Utils::addfd(int epollfd, int fd, bool one_shot) {
    epoll_event event;
    event.data.fd = fd;

    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//信号处理函数
void Utils::sig_handler(int sig) {
    //为保证函数的可重入性，保留原来的errno
    //可重入性表示中断后再次进入该函数，环境变量与之前相同，不会丢失数据
    int save_errno = errno;
    int msg = sig;
    //将信号值从管道写端写入，传输字符类型，而非整型
    send(u_pipefd[1], (char *)&msg, 1, 0);
    //将原来的errno赋值为当前的errno
    errno = save_errno;
}

//设置信号函数
void Utils::addsig(int sig, void(handler)(int), bool restart) {
    //创建sigaction结构体变量
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    //sa_handler是一个函数指针，指向信号处理函数
    //信号处理函数中仅仅发送信号值，不做对应逻辑处理
    sa.sa_handler = handler;     
    if (restart)
        //sa_flags用于指定信号处理的行为
        //SA_RESTART，使被信号打断的系统调用自动重新发起
        sa.sa_flags |= SA_RESTART;
    //sa_mask用来指定在信号处理函数执行期间需要被屏蔽的信号
    //sigfillset 用来将参数set信号集初始化，然后把所有的信号加入到此信号集里。
    sigfillset(&sa.sa_mask);
    //sig 操作的信号 &sa 对信号设置新的处理方式  NULL 信号原来的处理方式
    assert(sigaction(sig, &sa, NULL) != -1);    //0 成功，-1 有错误发生
}

//定时处理任务，重新定时以不断触发SIGALRM信号
void Utils::timer_handler() {
    m_timer_lst.tick();
    alarm(m_TIMESLOT);
}

void Utils::show_error(int connfd, const char *info) {
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

class Utils;
//定时器回调函数
void cb_func(client_data *user_data) {
    //删除非活动连接在socket上的注册事件
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    //关闭文件描述符
    close(user_data->sockfd);
    //减少连接数
    --(http_conn::m_user_count);
}
