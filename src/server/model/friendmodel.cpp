#include "friendmodel.hpp"
#include "db.h"
#include "json.hpp"
using json = nlohmann::json;
// 添加好友关系
void FriendModel::insert(int userid, int friendid)
{
    // 1. 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into friend values(%d, %d)", userid, friendid);
    // 初始化连接池
    ConnectionPool *cp = ConnectionPool::getConnectionPool();
    // 获取连接
    shared_ptr<MySQL> dbConn = cp->getConnection();
    dbConn->update(sql);
}

// 返回用户好友列表
vector<string> FriendModel::query(int userid)
{
    // 1. 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "select a.id, a.name, a.state from user a inner join friend b on b.friendid = a.id where b.userid = %d", userid);
    //
    vector<string> vec;
    // 初始化连接池
    ConnectionPool *cp = ConnectionPool::getConnectionPool();
    // 获取连接
    shared_ptr<MySQL> dbConn = cp->getConnection();
    MYSQL_RES *res =dbConn->query(sql);
    if(res != nullptr)
    {
        // 把userid用户的所有离线消息放入vec返回
        MYSQL_ROW row;
        while((row = mysql_fetch_row(res)) != nullptr)
        {
            json js;
            js["id"] = atoi(row[0]);
            js["name"] = row[1];
            js["state"] = row[2];
            vec.push_back(js.dump());
        }
        mysql_free_result(res);
        return vec;
    }
    return vec;
}