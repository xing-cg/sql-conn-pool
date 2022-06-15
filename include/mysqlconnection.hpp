#pragma once
#include<mysql/mysql.h>
#include<string>
#include<ctime> //clock_t  clock()
/**
 * 封装MySQL数据库增删改查操作
 */
class MySQLConnection
{
public:
    MySQLConnection();
    ~MySQLConnection();
public:
    bool connect(std::string ip, unsigned short port,
                 std::string user, std::string password, std::string dbname);
    bool update(std::string sql);
    MYSQL_RES * query(std::string sql);

    /**
     * 设置mysql连接的空闲状态起始点; 
     * clock()是当前时间;
     * - 在连接入队列时设置, 有两个设置点:
     *    1. 创建连接时 - 1) 初始化 2) 生产者生产
     *    2. 归还连接时 - getConnection中, shared_ptr的删除器函数, 归还队列时
     */
    void refreshAliveTime()
    {
        m_alivetime = clock();
    }
    //返回连接进入空闲状态后的存活时长
    clock_t getAlivetime() const
    {
        return clock() - m_alivetime;
    }
private:
    MYSQL * m_conn;
    clock_t m_alivetime;    //记录进入空闲状态的时刻
};