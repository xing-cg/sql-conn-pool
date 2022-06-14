#pragma once
#include<mysql/mysql.h>
#include<string>
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
private:
    MYSQL * m_conn;
};