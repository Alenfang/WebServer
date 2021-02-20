#pragma once
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <list>
#include <iostream>

class CSocketContext
{
public:
    CSocketContext()
    {
        m_buffer_size = 0;
        m_socket = -1;
        m_psocket_context = NULL;
    }

    CSocketContext(const int buffer_size)
    {
        if(0 < buffer_size)
        {
            m_buffer_size = buffer_size;
            m_psocket_context = new char[m_buffer_size];
            memset(m_psocket_context, 0, m_buffer_size);
        }
    }

    const char *GetBuffer()
    {
        return m_psocket_context;
    }

    void SetBuffer(char *szBuffer, int nlen)
    {
        m_buffer_size = nlen;
        m_psocket_context = new char[m_buffer_size];
        memset(m_psocket_context, 0, m_buffer_size);
        memcpy(m_psocket_context, szBuffer, nlen);
    }

    void SetSocket(int socket)
    {
        m_socket = socket;
    }

    int GetSocket()
    {
        return m_socket;
    }

    int GetBufferSize()
    {
        return m_buffer_size;
    }

    ~CSocketContext()
    {
        if(NULL != m_psocket_context)
        {
            delete m_psocket_context;
            m_psocket_context = NULL;
        }
    }

    void process_socketinfo()
    {
        //获取请求的数据
        const char *prequest_buffer = GetBuffer();
        //解析出请求的网页文件名
        char file_name[20] = "";
        sscanf(prequest_buffer, "GET /%s", file_name);//获取请求的文件名
        std::cout<<file_name<<std::endl;

        char mine[10] = "";
        if(strstr(file_name, ".html"))
        {
            strcpy(mine, "text/html");
        }
        else if(strstr(file_name, ".jpg"))
        {
            strcpy(mine, "image/jpg");
        }
                                    
        char response_buffer[1024*1024] = "";
        sprintf(response_buffer, "HTTP/1.1 200 OK \r\nContent-Type: %s\r\n\r\n", mine);
        char file_path[100] = "/home/lyf/LYF/Servers_Clients_Test/server_epoll/Web/";
        strcat(file_path, file_name);
        std::cout<<"文件路径 ："<<file_path<<std::endl;
        int file_html1 = open(file_path, O_RDONLY);
        if(-1 != file_html1)
        {
            int response_length = read(file_html1, response_buffer+strlen(response_buffer), 
                                sizeof(response_buffer));
            if(0 < response_length)
            {
                send(m_socket, response_buffer, sizeof(response_buffer), 0);
            }
        }
            
    }

private:
    char *m_psocket_context;
    int m_buffer_size;
    int m_socket;
};

class threadpool
{
public:
    threadpool( int thread_number = 8, int max_requests = 10000 );
    ~threadpool();
    bool append( CSocketContext* request );

private:
	/*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void* worker( void* arg );
    void run();

private:
    int m_thread_number;	/*线程池中的线程数*/
    int m_max_requests;		/*请求队列中允许的最大请求数*/
    pthread_t* m_threads;	/*描述线程池的数组，其大小为m_thread_number*/
    std::list< CSocketContext* > m_workqueue;	/*请求队列*/

    pthread_mutex_t m_mutex; //关锁：pthread_mutex_lock( &m_mutex ); 开锁：pthread_mutex_unlock( &m_mutex );
    pthread_cond_t m_cond;

    //locker m_queuelocker;	/*保护请求队列的互斥锁*/
    //sem m_queuestat;	/*是否有任务需要处理*/
    bool m_stop;	/*是否结束线程*/
};

threadpool::threadpool( int thread_number, int max_requests ) : 
        m_thread_number( thread_number ), m_max_requests( max_requests ), m_stop( false ), m_threads( NULL )
{
    if((thread_number <= 0 ) || ( max_requests <= 0 ) )
    {
        std::cout<<"(thread_number <= 0 ) || ( max_requests <= 0 )"<<std::endl;
        throw std::exception();
    }

    m_threads = new pthread_t[ m_thread_number ];
    if(NULL ==  m_threads)
    {
        throw std::exception();
    }

    for (int i=0; i<thread_number; ++i )
    {
        std::cout<<"create the "<<i<<"th thread"<<std::endl;
        //创建线程
        if( pthread_create( m_threads + i, NULL, worker, this ) != 0 )
        {
            delete [] m_threads;
            throw std::exception();
        }
        //将该线程设置为运行结束后会自动释放所有资源
        if( pthread_detach( m_threads[i] ) )
        {
            delete [] m_threads;
            throw std::exception();
        }
    }
}

threadpool::~threadpool()
{
    delete [] m_threads;
    m_stop = true;
}

//添加任务队列
bool threadpool::append( CSocketContext* request )
{
    pthread_mutex_lock(&m_mutex);
    if ( static_cast<int>(m_workqueue.size()) > m_max_requests )
    {
        pthread_mutex_unlock(&m_mutex);;
        return false;
    }
    m_workqueue.push_back( request );
    pthread_mutex_unlock(&m_mutex);
    return true;
}

void* threadpool::worker( void* arg )
{
    threadpool* pool = ( threadpool* )arg;
    pool->run();
    return pool;
}

void threadpool::run()
{
    while(!m_stop)
    {
        pthread_mutex_lock( &m_mutex);//关锁：
        if ( m_workqueue.empty() )//任务队列为空
        {
               pthread_mutex_unlock( &m_mutex );//开锁
               continue;
        }

        CSocketContext *pDest = m_workqueue.front();
        m_workqueue.pop_front();//任务完成（结束）
        pthread_mutex_unlock( &m_mutex );//开锁

        if(NULL == pDest)
        {
            continue;
        }
        //执行处理（对请求报文处理）
        pDest->process_socketinfo();

        //处理完成清理空间
        delete pDest;
        pDest = NULL;
    }
}