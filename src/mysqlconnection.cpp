#include"public.hpp"
#include"mysqlconnection.hpp"
MySQLConnection::MySQLConnection()
{
    m_conn = mysql_init(nullptr);
}
MySQLConnection::~MySQLConnection()
{
    if(m_conn != nullptr)
    {
        mysql_close(m_conn);
    }
}
bool MySQLConnection::connect(std::string ip, unsigned short port,
                              std::string user, std::string password,
                              std::string dbname)
{
    MYSQL *p = mysql_real_connect(m_conn, ip.c_str(), user.c_str(),
                                  password.c_str(), dbname.c_str(),
                                  port, nullptr, 0);
    if(p != nullptr)
    {
        /**
         * C/C++代码默认的编码字符是ASCII, 
         * 如果不设置, 则从MySQL上拉下来的中文无法正常显示
         */
        mysql_query(m_conn, "set name gbk");
        LOG("connect mysql success!");
    }
    else
    {
        LOG("connect mysql failed!");
    }
    return p != nullptr;
}
bool MySQLConnection::update(std::string sql)
{
    if(mysql_query(m_conn, sql.c_str()) == 0)
    {
        LOG("update failed: " + sql);
        return false;
    }
    return true;
}
MYSQL_RES * MySQLConnection::query(std::string sql)
{
    if(mysql_query(m_conn, sql.c_str()) == 0)
    {
        LOG("select failed: " + sql);
        return nullptr;
    }
    return mysql_use_result(m_conn);
}