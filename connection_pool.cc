#include "connection_pool.h"
#include "log.h"

ConnectionPool* ConnectionPool::GetInstance()
{
    static ConnectionPool connPool;
    return &connPool;
}

void ConnectionPool::init(string url,int port,string user,string password,string dbname,unsigned int maxconn)
{
    this->url=url;
    this->port=port;
    this->user=user;
    this->password=password;
    this->databasename=dbname;
    for (int i=0;i<maxconn;i++)
    {
        MYSQL* con=nullptr;
        con=mysql_init(con);
        if(con==nullptr)
        {
            LOG_ERROR("%s","mysql init error");
            exit(1);
        }
        con=mysql_real_connect(con,url.c_str(),user.c_str(),password.c_str(),dbname.c_str(),port,nullptr,0);
        if(con==nullptr)
        {
            LOG_ERROR("%s","mysql connect error");
            exit(1);
        }
        conn_queue.push(con);
    }
    pthread_mutexattr_t mtx_attr;
    pthread_mutexattr_init(&mtx_attr);
    pthread_mutexattr_setpshared(&mtx_attr,PTHREAD_PROCESS_PRIVATE);
    pthread_mutex_init(&mtx,&mtx_attr);
    sem_init(&sem,0,maxconn);
}

ConnectionPool::~ConnectionPool()
    {
        //DestroyPool();
        //locker.Lock();
        pthread_mutex_lock(&mtx);
        if(conn_queue.size()>0)
        {
            for(int i=0;i<conn_queue.size();i++)
            {
                MYSQL* con=conn_queue.front();
                conn_queue.pop();
                mysql_close(con);
            }
        }

        //locker.Unlock();
        pthread_mutex_unlock(&mtx);
        pthread_mutex_destroy(&mtx);
        sem_destroy(&sem);
    }

MYSQL* ConnectionPool::GetConnection()
{
    MYSQL* con=nullptr;
    if(conn_queue.size()==0)
        return nullptr;
    sem_wait(&sem);
    //locker.Lock();
    pthread_mutex_lock(&mtx);
    con=conn_queue.front();
    conn_queue.pop();
    pthread_mutex_unlock(&mtx);
    //locker.Unlock();
    return con;
}

bool ConnectionPool::ReleaseConnection(MYSQL* conn)
{
    if(conn==nullptr)
        return false;
    //locker.Lock();
    pthread_mutex_lock(&mtx);
    conn_queue.push(conn);
    //locker.Unlock();
    pthread_mutex_unlock(&mtx);
    sem_post(&sem);
}

/*void connection_pool::DestroyPool()
{
    locker.Lock();
    if(conn_queue.size()>0)
    {
        for(int i=0;i<conn_queue.size();i++)
        {
            MYSQL* con=conn_queue.front();
            conn_queue.pop();
            mysql_close(con);
        }
        locker.Unlock();
    }
    else
        locker.Unlock();
}*/

