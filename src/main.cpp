#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/epoll.h>
#include<sys/socket.h>
#include<sys/time.h>
#include<memory.h>
#include<time.h>
#include<arpa/inet.h>
#include<fcntl.h>
#include<errno.h>
#include<signal.h>
#include<iostream>
#include<string>
#include<fstream>
#include"ThreadProc.h"
using namespace std;

//#define BUFFER_SIZE 1024
#define BUFFER_SIZE 4
#define EPOLL_SIZE 50

#define SOCKET_HANDLE(statement) \
if(-1 == (statement))\
{\
    write_log("[Error]",strerror(errno));\
    exit(0);\
}

//设置非阻塞io
void set_nonblocking_mode(int fd);
//字符串追加
void StrAppend(char *pDest, const char *pSrc, int nsize);
//写入日志
void write_log(string log_type, string log_info);
//初始化服务端套接字
void init_network(int port=80);
//服务退出
void exit_network(int flag);

//全局变量
ofstream log_fd;//日志文件句柄
int socket_server = 0;
sockaddr_in addr_sock;

int main(void)
{
    log_fd.open("log.txt", ofstream::out | ofstream::app);
    //初始化服务端套接字
    init_network(80);
    signal(SIGINT, exit_network);
    write_log("[Info]", "服务器已启动 server ：" + to_string(socket_server));
    //将监听套接子设置成非阻塞套接字
    set_nonblocking_mode(socket_server);

    epoll_event *ep_events = NULL;
    epoll_event event;

    //创建epoll的文件句柄
    int epoll_fd = epoll_create(EPOLL_SIZE);
    
    //初始化监听套接字的事件
    event.events = EPOLLIN;
    event.data.fd = socket_server;
    
    //将epol的文件句柄和sock_listen以及其响应事件绑定
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_server, &event);

    ep_events = new epoll_event[EPOLL_SIZE];

    //开启线程池
    threadpool *pthread_pool = new threadpool(1,100);

    while(1)
    {
        int event_cnt = epoll_wait(epoll_fd, ep_events, EPOLL_SIZE, -1);
        //cout<<"event_count : "<<event_cnt<<endl;

        puts("reuturn epoll wait");

        if(-1 == event_cnt)
        {
            cout<<"epoll_wait() error"<<endl;
            break;
        }
        else
        {
            //处理响应事件的socket
            for(int i=0; i<event_cnt; i++)
            {
                if(socket_server == ep_events[i].data.fd)
                {
                    int len_addr = sizeof(sockaddr_in);
                    
                    //accept
                    int sock_client = accept(socket_server, (sockaddr*)&addr_sock, (socklen_t*)&len_addr);
                    write_log("[Info]", "socket : " + to_string(sock_client) + " 客户端请求连接......");

                    if(-1 != sock_client)
                    {
                        set_nonblocking_mode(sock_client);
                        event.events = EPOLLIN|EPOLLET;//将套接字注册为边缘触发
                        event.data.fd = sock_client;
                        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock_client, &event);
                    }
                    else
                    {
                        cout<<"accept() error"<<endl;
                        continue;
                    }
                }
                else
                {
                    
                    int sock_client = ep_events[i].data.fd;
                    char request_buffer[1024] = "";
                    char buffer_recv[BUFFER_SIZE] = "";
                    while(1)
                    {
                        memset(buffer_recv,'\0', BUFFER_SIZE);
                        int count_recv = read(sock_client, buffer_recv, BUFFER_SIZE);
                        
                        if(count_recv <= 0)
                        {
                            if(EAGAIN == errno)//缓冲区没有数据可读
                            {
                                //continue;
                                int request_length = strlen(request_buffer);
                                if(0 < request_length)
                                {
                                    cout<<request_buffer<<endl;
                                    CSocketContext *pNew = new CSocketContext(request_length);
                                    pNew->SetBuffer(request_buffer, request_length);//填充请求信息
                                    pNew->SetSocket(sock_client);//填充套接字
                                    
                                    cout<<"添加线程任务 ：\n";
                                    //添加到线程池
                                    pthread_pool->append(pNew);
                                }

                                write_log("[Info]", "来自客户端 socket : "+to_string(sock_client) + "的数据接受完毕");

                                break;
                            }
                            else
                            {
                                //解除断开连接的套接字与epoll文件的绑定
                                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sock_client, &event);
                        
                                //除非已确认客户端断开连接，服务端不可主动断开连接
                                close(sock_client);
                        
                                //cout<<sock_client<<" 已断开连接"<<endl;
                                write_log("[Info]", "客户端 socket : " + to_string(sock_client) + "断开连接");

                                break;
                            }
                        }
                        else
                        {
                            StrAppend(request_buffer, buffer_recv, BUFFER_SIZE);
                        }  
                    }
                }
            }            

        } 
    }

    close(epoll_fd);
    close(socket_server);
    delete ep_events;

    return 0;
}

//设置非阻塞套接字
void set_nonblocking_mode(int fd)
{
    int flag = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, flag|O_NONBLOCK);
}

//字符串的追加
void StrAppend(char *pDest, const char *pSrc, int nsize)
{
    char *pTemp = pDest;
    if(NULL != pTemp)
    {
        while('\0' != *pTemp)
        {
            pTemp++;
        }

        for(int i=0; i<nsize; i++)
        {
            *pTemp = pSrc[i];
            pTemp++;
        }
    }
}

//写入日志
void write_log(string log_type, string log_info)
{
    time_t t;
    time(&t);

    //记录时间信息
    char time_info[32] = "";
    strftime(time_info, sizeof(time_info), "%Y-%m-%d.%H:%M:%S", localtime(&t));
    log_fd<<"["<<time_info<<"] : "<<log_type<<"    "<<log_info<<endl;
}

//初始化服务socket
void init_network(int port)
{
    socket_server = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    //SO_REUSEADDR可以使得端口被释放后可以立即被绑定，不用等待两分钟
    int n = 1;
    setsockopt(socket_server, SOL_SOCKET, SO_REUSEADDR, &n, 4);
    cout<<"server : "<<socket_server<<endl;

    SOCKET_HANDLE(socket_server)

    //初始化地址信息
    memset(&addr_sock, 0, sizeof(sockaddr_in));
    addr_sock.sin_family = AF_INET;
    addr_sock.sin_port = htons(port);//80是http服务器的默认端口
    addr_sock.sin_addr.s_addr = inet_addr("172.21.0.7");

    SOCKET_HANDLE(bind(socket_server, (sockaddr *)&addr_sock, sizeof(sockaddr_in)));
    SOCKET_HANDLE(listen(socket_server,3));
}

//服务退出
void exit_network(int flag)
{
    write_log("[Info]", "正常关闭服务器");
    exit(0);
}