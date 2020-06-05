#include "chatservice.hpp"
#include "common.hpp"
#include <muduo/base/Logging.h>
#include <vector>
using namespace std;
using namespace muduo;



// 获取单例对象的接口函数
ChatService* ChatService::instance()
{
    static ChatService service;
    return &service;
}

// 注册消息以及对应的Handler回调操作
ChatService::ChatService()
{
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({LOGINOUT_MSG, std::bind(&ChatService::loginout, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});
    // 群组业务
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});

    // 连接redis服务器
    if(_redis.connect())
    {
        // 设置上报消息的回调
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}

// 获取消息所对应的处理方法
MsgHandler ChatService::getHandler(int msgid)
{
    // 记录错误日志， msgid没有对应事件的回调函数
    auto it = _msgHandlerMap.find(msgid);
    if( it == _msgHandlerMap.end())
    {
        // 返回一个默认的处理器，空操作
        return [=](const TcpConnectionPtr &, json &, Timestamp) {
            LOG_ERROR << "msgid:" << msgid << "can not find handler!";
        };
    }
    else
    {
        return _msgHandlerMap[msgid];
    }
}

// 处理登陆业务 ORM 业务层处理的都是对象
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    LOG_INFO<<"do login service";
    int id = js["id"].get<int>();
    string pwd = js["password"];
    User user = _userModel.query(id);
    if(user.getId() == id && user.getPwd() == pwd)
    {
        if(user.getState() == "online")
        {
            // 该用户已经登陆，不允许重复登陆
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "User name has been logged in";
            conn->send(response.dump());

        }
        else
        {
            // 登陆成功,记录用户连接信息  要注意线程安全
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({user.getId(), conn});
            }

            // id 用户登陆成功后，向redis订阅channel id
            _redis.subscribe(id);

            // 更新用户状态信息
            user.setState("online");
            _userModel.updateState(user);

            // 登陆成功
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();
            
            // 查询用户是否有离线消息
            vector<string> vec = _offlineMsgModel.query(id);
            if(!vec.empty())
            {
                response["offlinemessage"] = vec;
                // 读取该用户的离线消息后，把该用户的所有离线消息删除掉
                _offlineMsgModel.remove(id);
            }
            // 查询用户的好友信息，并返回
            vector<string> userVec = _friendModel.query(id);
            if(!userVec.empty())
            {
                response["friends"] = userVec;
            }
            // 查询用户的群组信息，并返回
            vector<Group> groupuserVec = _groupModel.queryGroups(id);
            if(!groupuserVec.empty())
            {
                vector<string> groupInfo;
                for(Group group : groupuserVec)
                {
                    json groupjs;
                    groupjs["id"] = group.getId();
                    groupjs["name"] = group.getName();
                    groupjs["desc"] = group.getDesc();
                    vector<string> userInfo;
                    for(GroupUser user : group.getUsers())
                    {
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        userInfo.push_back(js.dump());
                    }
                    groupjs["users"] = userInfo;
                    groupInfo.push_back(groupjs.dump());
                }
                response["groups"] = groupInfo;
            }
            conn->send(response.dump());
        }
        
    }
    else
    {
        // 登陆失败，该用户不存在，用户存在但是密码错误
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "Wrong account or password";
        conn->send(response.dump());
    }
    
}
// 处理注册业务
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    //LOG_INFO<< "do reg service";
    string name = js["name"];
    string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPwd(pwd);
    bool state = _userModel.insert(user);
    if(state)
    {
        //注册成功
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
        conn->send(response.dump());
    }
    else
    {
        //注册失败
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
        conn->send(response.dump());
    }
    
}

// 添加好友业务
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    // 存储好友信息到数据库
    _friendModel.insert(userid, friendid);
    
}

// 一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int toid = js["to"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if(it != _userConnMap.end())
        {
            // 对方在线
            // 转发消息
            it->second->send(js.dump());
            return;
        }
    }

    // 查询toid是否在线
    User user = _userModel.query(toid);
    if(user.getState() == "online")
    {
        _redis.publish(toid, js.dump());
        return;
    }

    // 对方不在线
    // 存储消息
    _offlineMsgModel.insert(toid, js.dump());

}

// 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    string groupname = js["groupname"];
    string groupdesc = js["groupdesc"];
    Group group(-1, groupname, groupdesc);
    if(_groupModel.createGroup(group))
    {
        // 存储群组创建人信息
        _groupModel.addGroup(userid, group.getId(), "creator");
    }
}

// 添加群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupModel.addGroup(userid, groupid, "normal");
}

// 群聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> useridVec = _groupModel.queryGroupUsers(userid, groupid);
    lock_guard<mutex> lock(_connMutex);
    for(int id : useridVec)
    {
        
        auto it = _userConnMap.find(id);
        if(it != _userConnMap.end())
        {
            // 转发群消息
            it->second->send(js.dump());
        }
        else
        {
            // 查询toid是否在线
            User user = _userModel.query(id);
            if(user.getState() == "online")
            {
                _redis.publish(id, js.dump());
            }
            else
            {
                // 存储离线消息
                _offlineMsgModel.insert(id, js.dump());
            }
            
        }
        
    }
}

// 服务器异常， 业务重置方法
void ChatService::reset()
{
    // 把online状态的用户 设置成offline
    _userModel.resetState();
}

// 处理注销业务
void ChatService::loginout(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if(it != _userConnMap.end())
        {
            // 删除用户
            _userConnMap.erase(it);
        }
    }

    // 在redis中取消channel的订阅
    _redis.unsubscribe(userid);

    // 更新用户状态信息
    User user(userid, "", "", "offline");
    _userModel.updateState(user);
}

// 处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    User user;
    {
        lock_guard<mutex> lock(_connMutex);   
        for(auto it = _userConnMap.begin(); it != _userConnMap.end(); it++)
        {
            if(it->second == conn)
            {
                // 从map表中删除连接信息
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }

    // 在redis在取消id订阅
    _redis.unsubscribe(user.getId());
    
    // 更新用户信息
    user.setState("offline");
    _userModel.updateState(user);
}

// 从redis消息队列中获取订阅的消息
void ChatService::handleRedisSubscribeMessage(int userid, string msg)
{
    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if (it != _userConnMap.end())
    {
        it->second->send(msg);
        return;
    }

    // 存储该用户的离线消息
    _offlineMsgModel.insert(userid, msg);

}