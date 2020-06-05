#ifndef COMMON_H
#define COMMON_H

/* server 和 client 公共文件 */

enum EnMsgType{
    LOGIN_MSG = 1,      //登陆消息
    LOGIN_MSG_ACK,
    LOGINOUT_MSG, // 注销消息
    REG_MSG,            //注册消息
    REG_MSG_ACK,
    ONE_CHAT_MSG,       //聊天消息
    ADD_FRIEND_MSG,     //添加好友
    CREATE_GROUP_MSG,   //创建群组
    ADD_GROUP_MSG,      //删除群组
    GROUP_CHAT_MSG,     //群聊天
};

#endif