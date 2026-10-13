#pragma once
#include "Arduino.h"
enum HTTPMethod {HTTP_ANY,HTTP_GET,HTTP_POST};
class WebServer {
 struct Route {std::string path;HTTPMethod method;std::function<void()> handler;};
 std::vector<Route> routes_;
 std::function<void()> missing_;
 public:
 inline static WebServer *instance=nullptr;
 HTTPMethod requestMethod=HTTP_GET;
 std::vector<std::pair<String,String>> arguments;
 std::map<std::string,String> headers;
 int code=0;String response,type;
 explicit WebServer(int){instance=this;}
 void collectHeaders(const char**,size_t){}
 void on(const char*p,HTTPMethod m,std::function<void()>h){routes_.push_back({p,m,h});}
 void onNotFound(std::function<void()>h){missing_=h;}
 void begin(){}
 void handleClient(){}
 void sendHeader(const char*,const char*){}
 void send(int c,const char*t,const String &s){code=c;type=t;response=s;}
 void send_P(int c,const char*t,const char*s){send(c,t,s);}
 String header(const char*n){return headers[n];}
 int args(){return int(arguments.size());}
 String argName(int i){return arguments.at(i).first;}
 String arg(int i){return arguments.at(i).second;}
 String arg(const char*n){for(auto&a:arguments)if(a.first==String(n))return a.second;return "";}
 bool hasArg(const char*n){for(auto&a:arguments)if(a.first==String(n))return true;return false;}
 void request(const char*p,HTTPMethod m,std::vector<std::pair<String,String>>a={},std::map<std::string,String>h={}){
  requestMethod=m;arguments=a;headers=h;code=0;
  for(auto&r:routes_)if(r.path==p&&(r.method==m||r.method==HTTP_ANY)){r.handler();return;}
  if(missing_)missing_();
 }
};
