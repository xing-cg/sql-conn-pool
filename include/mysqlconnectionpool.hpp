#include<string>
#include<queue>
#include<mutex>
#include"mysqlconnection.hpp"
class MySQLConnectionPool
{
public:
    static MySQLConnectionPool * getConnectionPool();
private:
    MySQLConnectionPool();
private:
    std::string m_ip;
    unsigned short m_port;
    std::string m_username;
    std::string m_password;
    int m_initSize;
    int m_maxSize;
    int m_connectionTimeout;
private:
    std::queue<MySQLConnection*> m_connectionQueue;
    std::mutex m_queueMutex;
};