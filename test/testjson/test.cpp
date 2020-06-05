#include "json.hpp"
using json = nlohmann::json;

#include<iostream>
#include<vector>
#include<map>
using namespace std;

void func1(){
    json js;
    js["msg_type"] = 2;
    js["from"] = "zhang san";
    js["to"] = "li si";
    js["msg"] = "hello, world";
    cout<<js<<endl;
}

string func2(){
    json js;

    vector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);
    js["list"] = vec;

    map<int, string> m;
    m.insert({1,"黄山"});
    m.insert({2,"华山"});
    m.insert({3,"嵩山"});
    js["path"] = m;

    cout<<js<<endl;
    string str = js.dump();
    return str;
}

int main()
{
    string recvBuf = func2();
    json jsbuf = json::parse(recvBuf);
    cout<<jsbuf["list"]<<endl;
    cout<<jsbuf["path"]<<endl;
    auto arr = jsbuf["list"];
    cout<<arr[0]<<endl;
    return 0;
}

