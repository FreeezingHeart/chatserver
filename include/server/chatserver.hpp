#ifndef CHATSERVER_H
#define CHATSERVER_H

#include<muduo/net/EventLoop.h>
#include<muduo/net/TcpServer.h>
using namespace muduo;
using namespace muduo::net;

//聊天服务器主类
class ChatServer
{
public:
    // 初始化聊天服务器
    ChatServer(EventLoop *loop,
               const InetAddress &listenAddr,
               const string &nameArg);
    // 启动服务
    void start();
private:
    // 处理连接的回调函数
    void onConnection(const TcpConnectionPtr&);
    // 处理读写事件的回调函数
    void onMessage(const TcpConnectionPtr&,
                   Buffer*,
                   Timestamp);
    TcpServer _server;  // muduo库实现服务器功能的类对象
    EventLoop *_loop;   // 指向循环对象的指针
};

#endif