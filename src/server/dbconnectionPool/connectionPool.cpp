#include "connectionPool.hpp"
#define LOG(str) cout << __FILE__ << ":" << __LINE__ << " " << __TIMESTAMP__ << " : " << str << endl;

// 单例
ConnectionPool* ConnectionPool::getConnectionPool()
{
    static ConnectionPool pool;
    return &pool;
}

// 创建初始数量的连接
bool ConnectionPool::loadConfigFile()
{
    char buf[1024] = {0};
    getcwd(buf, sizeof(buf));
    printf("%s", buf);
    FILE *pf = fopen("../cfg/mysql.conf", "r");
    if(pf == nullptr)
    {
        LOG("mysql.conf file is not exist!");
        return false;
    }
    while(!feof(pf))
    {
        char line[1024] = {0};
        fgets(line, 1024, pf);
        string str = line;
        int idx = str.find("=", 0);
        if(idx == -1)   // 无效
        {
            continue;
        }
        int endidx = str.find('\n', 0);
        string key = str.substr(0, idx);
        string value = str.substr(idx+1, endidx-idx-1);
        if(key=="ip")
        {
            _ip = value;
        }
        else if(key == "port")
        {
            _port = atoi(value.c_str());
        }
        else if(key == "dbname")
        {
            _dbname = value;
        }
        else if(key == "username")
        {
            _username = value;
        }
        else if(key == "password")
        {
            _password = value;
        }
        else if(key == "initSize")
        {
            _initSize = atoi(value.c_str());
        }
        else if(key == "maxSize")
        {
            _maxSize = atoi(value.c_str());
        }
        else if(key == "maxIdleTime")
        {
            _maxIdleTime = atoi(value.c_str());
        }
        else if(key == "connectionTimeOut")
        {
            _connectionTimeout = atoi(value.c_str());
        }
    }
    return true;
}

// 连接池的构造
ConnectionPool::ConnectionPool()
{
    // 加载配置项
    if(!loadConfigFile())
    {
        LOG("加载连接池配置失败");
        return;
    }
    LOG("ip: "+_ip);
    LOG("port: "+to_string(_port));
    LOG("dbname: "+_dbname);
    LOG("username: "+_username);
    LOG("password: "+_password);
    LOG("initSize: "+to_string(_initSize));
    LOG("maxSize: "+to_string(_maxSize));
    LOG("maxIdleTime: "+to_string(_maxIdleTime));
    LOG("connectionTimeout: "+to_string(_connectionTimeout));
    LOG("加载连接池配置成功");
    // 创建初始数量的连接
    for(int i=0;i<_initSize;i++)
    {
        MySQL *p = new MySQL();
        p->connect(_ip, _username, _password, _dbname, _port);
        p->resetAliveTime();
        _connectionQue.push(p);
        _connectionCnt++;
    }
    LOG("初始化连接");
    // 启动一个新线程，作为连接的生产者
    thread producer(std::bind(&ConnectionPool::produceConnectionTask, this));
    producer.detach();
    // 启动一个新线程，回收空闲连接
    thread checkIdle(std::bind(&ConnectionPool::freeIdleConnectionTask, this));
    checkIdle.detach();
}

// 生产者线程的回调函数，负责生产新连接
void ConnectionPool::produceConnectionTask()
{
    for(;;)
    {
        unique_lock<mutex> lock(_queueMutex);
        while(!_connectionQue.empty())
        {
           cv.wait(lock);  //队列不空， 生产者进入等待
        }
        // 连接数量没有到达上限，继续创建新连接
        if(_connectionCnt < _maxSize)
        {
            MySQL *p = new MySQL();
            p->connect(_ip, _username, _password, _dbname, _port);
            p->resetAliveTime();
            _connectionQue.push(p);
            _connectionCnt++;
        }
        // 通知消费者线程
        cv.notify_all();
    }
}

// 回收空闲线程
void ConnectionPool::freeIdleConnectionTask()
{
    for(;;)
    {
        this_thread::sleep_for(chrono::seconds(_maxIdleTime));

        // 扫描整个队列，释放空闲连接
        unique_lock<mutex> lock(_queueMutex);
        while(_connectionCnt > _initSize)
        {
            MySQL *p = _connectionQue.front();
            if(p->getAliveTime() > _maxIdleTime*1000)
            {
                _connectionQue.pop();
                _connectionCnt--;
                delete p;
            }
            else
            {
                break;
            }
            
        }
    }
}

// 获取一个连接
shared_ptr<MySQL> ConnectionPool::getConnection()
{
    unique_lock<mutex> lock(_queueMutex);
    while(_connectionQue.empty())
    {
        if(cv_status::timeout == cv.wait_for(lock, chrono::milliseconds(_connectionTimeout)))
        {
            if(_connectionQue.empty())
            {
                LOG("获取空闲连接超时...获取连接失败");
                return nullptr;
            }
        } 
    }
    shared_ptr<MySQL> sp(_connectionQue.front(), [&](MySQL *pconn){
        unique_lock<mutex> lock(_queueMutex);
        pconn->resetAliveTime();
        _connectionQue.push(pconn);
    });
    _connectionQue.pop();
    // 消费完连接后，通知生产者检查是否需要生产了。
    cv.notify_all();
    return sp;
}