#ifndef CONNECTIONPOOL_HPP
#define CONNECTIONPOOL_HPP

#include <iostream>
#include <string>
#include <mutex>
#include <queue>
#include <atomic>
#include <thread>
#include <functional>
#include <memory>
#include <unistd.h>
#include <condition_variable>
#include "db.h"
using namespace std;

class ConnectionPool
{
public:
    // 获取连接池对象
    static ConnectionPool* getConnectionPool();
    // 获取连接
    shared_ptr<MySQL> getConnection();
private:
    ConnectionPool();
    // 从配置文件中加载配置项
    bool loadConfigFile();
    // 生产者线程的回调函数，负责生产新连接
    void produceConnectionTask();
    // 回收空闲连接
    void freeIdleConnectionTask();
    string _ip;             // mysql 初始ip
    unsigned short _port;   // mysql 初始端口号
    string _dbname;         // mysql 数据表名称
    string _username;       // mysql 用户名
    string _password;       // mysql 登陆密码
    int _initSize;          // 连接池的初始化连接量
    int _maxSize;           // 连接池的最大连接量
    int _maxIdleTime;       // 连接池最大空闲时间
    int _connectionTimeout; // 连接池获取连接的超时时间

    queue<MySQL*> _connectionQue;
    mutex _queueMutex;
    atomic_int _connectionCnt;      // 记录连接所创建的connection连接的总数
    condition_variable cv;
};

#endif