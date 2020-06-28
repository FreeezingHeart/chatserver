#include "offlinemessagemodel.hpp"
#include "db.h"

// 存储用户离线消息
void OfflineMsgModel::insert(int userid, string msg)
{
    // 1. 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into offlinemessage values(%d, '%s')", userid, msg.c_str());
    // 初始化连接池
    ConnectionPool *cp = ConnectionPool::getConnectionPool();
    // 获取连接
    shared_ptr<MySQL> dbConn = cp->getConnection();
    dbConn->update(sql);
}

// 删除用户离线消息
void OfflineMsgModel::remove(int userid)
{
    // 1. 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "delete from offlinemessage where userid = %d", userid);
    // 初始化连接池
    ConnectionPool *cp = ConnectionPool::getConnectionPool();
    // 获取连接
    shared_ptr<MySQL> dbConn = cp->getConnection();
    dbConn->update(sql);
}
// 查询用户离线消息
vector<string> OfflineMsgModel::query(int userid)
{
    // 1. 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "select message from offlinemessage where userid = %d", userid);
    //
    vector<string> vec;
    // 初始化连接池
    ConnectionPool *cp = ConnectionPool::getConnectionPool();
    // 获取连接
    shared_ptr<MySQL> dbConn = cp->getConnection();
    MYSQL_RES *res = dbConn->query(sql);
    if(res != nullptr)
    {
        // 把userid用户的所有离线消息放入vec返回
        MYSQL_ROW row;
        while((row = mysql_fetch_row(res)) != nullptr)
        {
            vec.push_back(row[0]);
        }
        mysql_free_result(res);
        return vec;
    }
    return vec;
}