#include"public.hpp"
#include"mysqlconnectionpool.hpp"
#include<thread>
#include<functional>
void MySQLConnectionPool::produceConnectionTask()
{
    for(;;)
    {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        while(!m_connectionQueue.empty())
        {
            m_queueNotEmpty.wait(lock);
        }
        if(m_connectionCnt < m_maxSize)
        {
            MySQLConnection *pconn = new MySQLConnection();
            pconn->connect(m_ip, m_port, m_username, m_password, m_dbname);
            pconn->refreshAliveTime(); //为连接p设置进入空闲状态的时间点
            m_connectionQueue.push(pconn);
            ++m_connectionCnt;
        }
        //通知消费者线程, 队列不空了, 可以拿取连接资源了
        m_queueNotEmpty.notify_all();
    }
}
/* 定时线程, 扫描超过maxIdleTime的多余空闲连接, 释放 */
void MySQLConnectionPool::sweepConnectionTask()
{
    for(;;)
    {
        // 通过sleep模拟定时效果
        std::this_thread::sleep_for(std::chrono::seconds(m_maxIdleTime));
        // 扫描整个队列, 释放多余的连接
        std::unique_lock<std::mutex> lock(m_queueMutex);
        while(m_connectionCnt > m_initSize)
        {
            MySQLConnection *pconn = m_connectionQueue.front();
            if(pconn->getAlivetime() >= (m_maxIdleTime * 1000))
            {
                m_connectionQueue.pop();
                --m_connectionCnt;
                delete pconn;   //即调用~MySQLConnection
            }
            else //如果队头没有超时, 队列后面的连接一定不会超时, 跳过;
            {
                break;
            }
        }
    }
}
MySQLConnectionPool::MySQLConnectionPool()
{
    /* 加载配置文件, 初始化成员熟悉 */
    if(!loadConfigFile())
    {
        exit(-1);
    }
    /* 向队列创建初始数量的连接 */
    for(int i = 0; i < m_initSize; ++i)
    {
        MySQLConnection * pconn = new MySQLConnection();
        pconn->connect(m_ip, m_port, m_username, m_password, m_dbname);
        pconn->refreshAliveTime();  //为连接p设置进入空闲状态的时间点
        m_connectionQueue.push(pconn);
        ++m_connectionCnt;
    }
    /* 启动生产连接线程 */
    std::thread producer(std::bind(
        &MySQLConnectionPool::produceConnectionTask, this));
    producer.detach();
    /* 启动定时线程, 扫描超过maxIdleTime的多余空闲连接, 释放 */
    std::thread sweeper(std::bind(
        &MySQLConnectionPool::sweepConnectionTask, this));
    sweeper.detach();
}
MySQLConnectionPool * MySQLConnectionPool::getConnectionPool()
{
    static MySQLConnectionPool pool;
    return &pool;
}
bool MySQLConnectionPool::loadConfigFile()
{
    FILE * pf = fopen("../conf/mysqlconnectionpool.cnf", "r");
    if(pf == nullptr)
    {
        LOG("mysqlconnectionpool.cnf file open failed!");
        return false;
    }
    while(!feof(pf))//文件没到末尾
    {
        char line[1024] = {0};
        fgets(line, 1024, pf);  //读取一行
        std::string str = line;
        int idx = str.find('=', 0);
        if(idx == std::string::npos)//npos的值为-1, 表示没找到
        {
            continue;
        }

        std::string key = str.substr(0, idx);
        //需要消除\n影响
        int end_idx = str.find('\n', idx);  //从idx往后找
        std::string value = str.substr(idx + 1, end_idx - idx - 1);

        if(key == "ip")
        {
            m_ip = value;
        }
        else if(key == "port")
        {
            m_port = atoi(value.c_str());
        }
        else if(key == "username")
        {
            m_username = value;
        }
        else if(key == "password")
        {
            m_password = value;
        }
        else if(key == "dbname")
        {
            m_dbname = value;
        }
        else if(key == "initSize")
        {
            m_initSize = atoi(value.c_str());
        }
        else if(key == "maxSize")
        {
            m_maxSize = atoi(value.c_str());
        }
        else if(key == "maxIdleTime")
        {
            m_maxIdleTime = atoi(value.c_str());
        }
        else if(key == "connectionTimeOut")
        {
            m_connectionTimeout = atoi(value.c_str());
        }
    }
    return true;
}
std::shared_ptr<MySQLConnection> MySQLConnectionPool::getConnection()
{
    std::unique_lock<std::mutex> lock(m_queueMutex);
    while(m_connectionQueue.empty())
    {
        if(std::cv_status::timeout == 
            m_queueNotEmpty.wait_for(
                lock, std::chrono::milliseconds(m_connectionTimeout)))
        {
            if(m_connectionQueue.empty())
            {
                LOG("get the idle connection timeout, failed!");
                return nullptr;
            }
        }
    }
    /**
     * shared_ptr智能指针析构时, 会把connection资源delete, 
     * 即调用connection析构函数, 会把connection close,
     * 我们不想让资源释放, 就需要自定义shared_ptr的释放资源方式,
     * 即重写删除器, 让connection直接归还到queue;
     */
    std::shared_ptr<MySQLConnection> sp(m_connectionQueue.front(),
        [&](MySQLConnection *pconn){
            std::unique_lock<std::mutex> lock(m_queueMutex);
            pconn->refreshAliveTime();
            m_connectionQueue.push(pconn);
        });
    m_connectionQueue.pop();

    m_queueNotEmpty.notify_all();//通知生产者
    return sp;
}