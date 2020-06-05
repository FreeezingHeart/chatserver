#include "chatserver.hpp"
#include "chatservice.hpp"
#include "json.hpp"

#include <iostream>
#include <functional>
#include <string>
using namespace std;
using namespace placeholders;
using json = nlohmann::json;

ChatServer::ChatServer(EventLoop *loop,
                       const InetAddress &listenAddr,
                       const string &nameArg)
    : _server(loop, listenAddr, nameArg), _loop(loop)
{
    // 注册处理连接的回调函数
    _server.setConnectionCallback(std::bind(&ChatServer::onConnection, this, _1));
    // 注册处理读写的回调函数
    _server.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1, _2, _3));
    // 设置线程数量
    _server.setThreadNum(4);
}

void ChatServer::start()
{
    _server.start();
}

// 处理连接的回调函数
void ChatServer::onConnection(const TcpConnectionPtr& conn)
{
    // 连接中断
    if(!conn->connected())
    {
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown();
    }

}

// 处理读写的回调函数
void ChatServer::onMessage(const TcpConnectionPtr& conn,
                           Buffer *buffer,
                           Timestamp time)
{
    string buf = buffer->retrieveAllAsString();
    // 数据反序列化
    json js = json::parse(buf);
    // 达到的目的，完全解耦网络模块的代码和业务模块的代码
    // 通过js["msgid"]获取 => 业务handler => conn js
    auto msghandler = ChatService::instance()->getHandler(js["msgid"].get<int>());
    msghandler(conn, js, time);
}
