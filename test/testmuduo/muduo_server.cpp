#include<muduo/net/TcpServer.h>
#include<muduo/net/EventLoop.h>
#include<iostream>
#include<functional>
using namespace std;

/*
基于muduo网络库开发服务器程序
1. 组合TcpServer对象
2. 创建EventLoop时间循环对象指针
3. 明确TcpServer构造函数需要什么参数， 输出
*/
class ChatServer
{
public:
    ChatServer(muduo::net::EventLoop *loop,
               const muduo::net::InetAddress &listenAddr,
               const string &nameArg)
        :_server(loop, listenAddr, nameArg), _loop(loop)
    {
        //给服务器注册用户连接的创建和断开的回调
        _server.setConnectionCallback(std::bind(&ChatServer::onConnection, this, placeholders::_1));
        //给服务器注册用户读写时间回调
        _server.setMessageCallback(std::bind(&ChatServer::onMessage,
                                   this,
                                   placeholders::_1,
                                   placeholders::_2,
                                   placeholders::_3)); 
        // 设置服务器端的线程数量, 1个IO线程， 3个worker线程
        _server.setThreadNum(4);
    }
    // 开启事件循环
    void start()
    {
        _server.start();
    }
private:
    // 专门处理用户的连接创建和断开
    void onConnection(const muduo::net::TcpConnectionPtr &conn)
    {
        if(conn->connected())
        {
            cout<< conn->peerAddress().toIpPort()<<"->"<<
                conn->localAddress().toIpPort()<<" state:online"<<endl;
        }
        else
        {
            cout<< conn->peerAddress().toIpPort()<<"->"<<
                conn->localAddress().toIpPort()<<" state:offline"<<endl;
            conn->shutdown();
            //_loop->quit();
        }
        
    }
    // 专门处理用户的读写事件
    void onMessage(const muduo::net::TcpConnectionPtr &conn,
                   muduo::net::Buffer *buffer,
                   muduo::Timestamp time)
    {
        string buf = buffer->retrieveAllAsString();
        cout<<"recv data:"<<buf<<"time:"<<time.toString()<<endl;
        conn->send(buf);
    }
    muduo::net::TcpServer _server;
    muduo::net::EventLoop *_loop;
};

int main()
{
    muduo::net::EventLoop loop;
    muduo::net::InetAddress addr("127.0.0.1", 6000);
    ChatServer server(&loop, addr, "ChatServer");

    server.start();
    loop.loop();

    return 0;
}