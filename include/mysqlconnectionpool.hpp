#include<string>
#include<queue>
#include<mutex>
#include<condition_variable>
#include<atomic>
#include<memory>
#include"mysqlconnection.hpp"
class MySQLConnectionPool
{
public:
    static MySQLConnectionPool * getConnectionPool();
public:
    /**
     * 给外部提供的接口, 从队列中获取一个空闲连接;
     * 用智能指针管理, 重定义智能指针的删除器, 让它归还队列, 而不是释放;
     */
    std::shared_ptr<MySQLConnection> getConnection();
private:
    MySQLConnectionPool();
private:
    bool loadConfigFile();   //从配置文件中加载配置项
private:
    /**
     * 生产者线程函数, 创建新连接;
     * 定义在连接池类中, 是为了方便线程函数访问连接池类的成员属性; 
     */
    void produceConnectionTask();
private:
    std::string m_ip;
    unsigned short m_port;
    std::string m_username;
    std::string m_password;
    std::string m_dbname;
    int m_initSize;         //初始连接量
    int m_maxSize;          //最大连接量
    int m_maxIdleTime;      //多余连接最长空闲时间
    int m_connectionTimeout;//获取连接超时时间
private:
    std::queue<MySQLConnection*> m_connectionQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueNotEmpty;
private:
    std::atomic_int m_connectionCnt;    //记录连接池现在已创建的连接总数
};