# chatserver

可以工作在nginx tcp负载均衡环境中的集群聊天服务器和客户端源码...

![](md_picture/聊天服务器.png)

## 项目需求

1. 客户端新用户注册
2. 客户端用户登陆
3. 添加好友和添加群组
4. 好友聊天
5. 群组聊天
6. 离线消息
7. tcp负载均衡
8. 集群聊天系统支持客户端跨服务器通信

## 开发环境

1. Linux环境  (ubuntu)
2. Json开发库  (JSON for Modern C++)
3. muduo网络库
4. redis  (hiredis)
5. mysql
6. nginx
7. CMake

## 具体设计

服务器采用分层的设计思想，分为网络I/O模块、业务模块、数据模块

### 一、网络I/O模块

基于muduo库采用的网络并发模型为**one loop per thread**，即通过一个**main reactor**来负责accept连接，然后该连接的I/O事件，计算任务等都在**sub reactor**中完成。多个连接可能被分派到多个线程中，来充分利用CPU。

利用muduo库写网络模块的代码非常简洁，只需要在定义的ChatServer中定义muduo的TcpServer和EventLoop，然后注册好onConnection和onMessage两个回调，就可以很方便的起一个服务。

onConnection是建立连接

onMessage则是根据不同的json消息中的msgid字段，来在业务层调用不同的回调函数来处理请求。

### 二、业务模块

基于单例模式设计一个负责处理业务的ChatService类，成员：所有的数据库操作类的对象，一个用来映射msgid到回调函数的map，一个互斥锁保证map的访问安全，还有一个存储在线用户的通信连接的map，redis的操作对象。然后所有的业务处理函数都在构造函数中注册到了对应的map上。

**2.1 登陆**

首先判断是否已经登陆，如已经登陆则返回错误提示信息

登陆成功，记录用户的连接信息，更新用户的状态

向redis订阅channel(id)

将离线消息封装到json消息中，然后调用offlinemessageModel中的`remove`方法删除读过的离线消息

显示好友列表：通过friendModel中的`query`方法，通过用户id查找所有的好友id，返回对应的好友user对象

显示群组列表：同显示好友列表，通过groupuserModel中的`query`方法，返回这个用户所在的所有群组的信息

登陆失败，提示错误信息（用户不存在或密码错误）

**2.2 注册**

根据json中的相关信息，新建一个user对象，调用userModel中的`insert`方法插入到数据库中

**2.3 注销**

加锁，删掉userConnMap中的对应的id的TCP连接信息

调用userModel中的`updateState`更新用户的状态信息

**2.4 客户端异常退出**

加锁，删掉对应的连接信息

取消redis的订阅通道

更新当时客户端登陆的用户的状态信息

**2.5 添加好友**

解析json中的字段，调用friendModel的`insert`方法就可以

**2.6 创建群组**

调用groupModel中的`createGroup`和`addGroup`方法，创建群，然后将当前创建人的权限改为创建者

**2.7 添加群组**

同理，非常简单

**2.8 好友聊天**

查询对方id是否在线（首先在本服务器中的userConnMap中查，如在，则将请求的json消息推给该用户即可；如果不在，需要在数据库中查是否在线，如果在线则在redis中发布对方id的channel， 推送消息）

如果不在线，直接存为离线消息

**2.9 群组聊天**

类似好友聊天，区别在于需要获取当前群id的所有user，然后根据他们在线与否，选择直接推送还是存为离线消息。（当不在本机找不到时，通过redis推送）

**2.10 从redis中获取订阅消息**

订阅的channel有消息了，查看对应id的TCP连接，然后通过绑定的回调函数处理(service直接把Json推送到对应的客户端)。

### 三、数据模块

为user, gourpuser, group表设计一个[ORM](http://www.ruanyifeng.com/blog/2019/02/orm-tutorial.html)类，为每一个ORM类添加一个数据操作类，操作ORM类对象。这样的设计将复杂的业务逻辑和数据处理逻辑分离，降低系统的耦合度，有利于扩展。

#### 3.1 数据库设计

**user表**

| 字段名称 |         字段类型          |    字段说明    |            约束             |
| :------: | :-----------------------: | :------------: | :-------------------------: |
|    id    |            INT            |     用户id     | PRIMARY KEY, AUTO_INCREMENT |
|   name   |        VARCHAR(50)        |     用户名     |      NOT NULL, UNIQUE       |
| password |        VARCHAR(50)        |    用户密码    |          NOT NULL           |
|  state   | ENUM('online', 'offline') | 当前的登陆状态 |      DEFAULT 'offline'      |

**Friend表**

| 字段名称 | 字段类型 | 字段说明 |        约束        |
| :------: | :------: | :------: | :----------------: |
|  userid  |   INT    |  用户id  | NOT NULL、联合主键 |
| friendid |   INT    |  好友id  | NOT NULL、联合主键 |

**ALLGroup表**

| 字段名称  |   字段类型   | 字段说明 |            约束             |
| :-------: | :----------: | :------: | :-------------------------: |
|    id     |     INT      |   群id   | PRIMARY KEY、AUTO_INCREMENT |
| roupname  |     INT      |  群名称  |      NOT NULL、UNIQUE       |
| groupdesc | VARCHAR(200) |  群描述  |         DEFAULT ''          |

**GroupUser表**

| 字段名称  |         字段类型          | 字段说明 |        约束        |
| :-------: | :-----------------------: | :------: | :----------------: |
|  groupid  |            INT            |   群id   | NOT NULL、联合主键 |
|  userid   |            INT            | 群成员id | NOT NULL、联合主键 |
| grouprole | ENUM('creator', 'normal') | 群内权限 |  DEFAULT 'normal'  |

**OfflineMessage表**

| 字段名称 |   字段类型   |         字段说明         |   约束   |
| :------: | :----------: | :----------------------: | :------: |
|  userid  |     INT      |          用户id          | NOT NULL |
| message  | VARCHAR(200) | 离线消息(存储Json字符串) | NOT NULL |

#### 3.2 连接池设计

为了提高MySQL数据库（基于C/S设计）的访问瓶颈，除了在服务器端增加缓存服务器缓存常用的数据 之外（例如redis），还可以增加连接池，来提高MySQL Server的访问效率，在高并发情况下，大量的 TCP三次握手、MySQL Server连接认证、MySQL Server关闭连接回收资源和TCP四次挥手所耗费的 性能时间也是很明显的，增加连接池就是为了减少这一部分的性能损耗。 

连接池的设计如下图所示：

![](md_picture/MySQL连接池.png)

**初始化方法**

加载配置文件

初始化链接（创建链接，刷新存活时间，入队）

初始化生产者线程

初始化释放超时空闲连接的线程

**生产者线程**

判断连接队列是否为空，如果不为空则继续wait等待

如果不为空，则判断是否当前的连接数下于最大连接数，如果是则生产新的连接。通知消费者进行消费了。（如果达到上限了，通知消费者消费也消费不了，超时后会发生获取连接失败的错误）

**消费方法**

判断队列是否为空，如果为空，则等待一个计时器的时长，如果还为空就报错。

不为空的话，用智能指针shared_ptr接受一个连接池中的连接，出队一个连接，定义删除器是将连接的空闲时长刷新后重新入队而不是释放。最后通知生产者检查队列是不是为空了。

**超时释放连接方法**

设置sleep时长为最大空闲时长

如果当前连接数大于初始连接数，那么如果当前队列不为空，则获取对首的连接，判断它的空闲时间是否超过阈值，如果超过就释放它，然后重新判断这个步骤。如果没有超过则跳出循环（因为队列是先入先出的，所以第一个没有超时的话，后面肯定都没超时）
