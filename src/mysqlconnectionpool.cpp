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
            MySQLConnection *p = new MySQLConnection();
            p->connect(m_ip, m_port, m_username, m_password, m_dbname);
            m_connectionQueue.push(p);
            ++m_connectionCnt;
        }
        //通知消费者线程, 队列不空了, 可以拿取连接资源了
        m_queueNotEmpty.notify_all();
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
        MySQLConnection * p = new MySQLConnection();
        p->connect(m_ip, m_port, m_username, m_password, m_dbname);
        m_connectionQueue.push(p);
        ++m_connectionCnt;
    }
    /* 启动一个新的线程, 作为连接的生产者 */
    std::thread producer(std::bind(
        &MySQLConnectionPool::produceConnectionTask, this));
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
            m_connectionQueue.push(pconn);
        });
    m_connectionQueue.pop();

    m_queueNotEmpty.notify_all();//通知生产者
    return sp;
}