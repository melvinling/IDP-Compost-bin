#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <deque>
#include <vector>
#include <functional>
#include <map>
#include <sstream>
#include <iomanip>
#include <type_traits>
#include <cstdio>
#include <cstring>
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define CHANGE 3
#define SERIAL_8N1 0x800001c
#define PROGMEM
#define ARDUINO_ISR_ATTR
using portMUX_TYPE = int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
#define portENTER_CRITICAL_ISR(x) ((void)(x))
#define portEXIT_CRITICAL_ISR(x) ((void)(x))
class String {
 std::string s_;
 public:
 String()=default;
 String(const char *s):s_(s?s:""){}
 String(const std::string &s):s_(s){}
 String(char c):s_(1,c){}
 template<class T, typename std::enable_if<std::is_integral<T>::value,int>::type = 0>
 String(T n):s_(std::to_string(n)){}
 String(float n,int precision=2){std::ostringstream s;s<<std::fixed<<std::setprecision(precision)<<n;s_=s.str();}
 const char *c_str()const{return s_.c_str();}
 int indexOf(char c)const{auto p=s_.find(c);return p==std::string::npos?-1:int(p);}
 String substring(int start,int end)const{return s_.substr(start,end-start);}
 void reserve(size_t n){s_.reserve(n);}
 String &operator+=(const String &s){s_+=s.s_;return *this;}
 friend String operator+(String a,const String &b){return a+=b;}
 friend bool operator==(const String&a,const String&b){return a.s_==b.s_;}
 friend bool operator!=(const String&a,const String&b){return !(a==b);}
};
namespace Fake {
struct PinEvent {int pin,value;bool mode;};
struct Tx {uint32_t baud;std::vector<uint8_t> bytes;};
inline uint64_t us=0;
inline std::vector<PinEvent> pins;
inline std::map<int,int> levels;
inline void (*edge)()=nullptr;
inline std::deque<uint8_t> rx;
inline std::vector<Tx> tx;
inline std::deque<std::vector<uint8_t>> responses;
inline uint32_t baud=0;
inline int serialRx=-1,serialTx=-1;
inline bool inverted=false,continuousNoise=false;
inline std::deque<char> consoleRx;
inline std::string consoleLog;
inline void reset(){us=0;pins.clear();levels.clear();edge=nullptr;rx.clear();tx.clear();responses.clear();continuousNoise=false;consoleRx.clear();consoleLog.clear();inverted=false;}
inline void advance(uint32_t ms){us+=uint64_t(ms)*1000;}
inline void pulseLevel(int level){levels[35]=level;if(edge)edge();}
}
inline uint32_t millis(){return uint32_t(Fake::us/1000);}
inline uint32_t micros(){return uint32_t(Fake::us);}
inline void delayMicroseconds(uint32_t us){Fake::us+=us;}
inline void delay(uint32_t ms){Fake::advance(ms);}
inline void pinMode(int p,int mode){Fake::pins.push_back({p,mode,true});}
inline void digitalWrite(int p,int v){Fake::levels[p]=v;Fake::pins.push_back({p,v,false});}
inline int digitalRead(int p){return Fake::levels[p];}
inline int digitalPinToInterrupt(int p){return p;}
inline void attachInterrupt(int,void(*fn)(),int){Fake::edge=fn;}
class HardwareSerial {
 public:
 explicit HardwareSerial(int){}
 void begin(uint32_t baud,int,int rx,int tx){Fake::baud=baud;Fake::serialRx=rx;Fake::serialTx=tx;}
 bool setRxInvert(bool value){Fake::inverted=value;return true;}
 void updateBaudRate(uint32_t baud){Fake::baud=baud;}
 int available(){return Fake::continuousNoise?1000:int(Fake::rx.size());}
 int read(){if(Fake::rx.empty())return 0x55;int b=Fake::rx.front();Fake::rx.pop_front();return b;}
 size_t write(const uint8_t *p,size_t n){
  Fake::tx.push_back({Fake::baud,{p,p+n}});
  if(!Fake::responses.empty()){for(auto b:Fake::responses.front())Fake::rx.push_back(b);Fake::responses.pop_front();}
  return n;
 }
};
struct FakeSerial {
 void begin(int){}
 int available(){return int(Fake::consoleRx.size());}
 int read(){auto c=Fake::consoleRx.front();Fake::consoleRx.pop_front();return c;}
 template<class... T>void printf(const char *format,T... args){char line[256];std::snprintf(line,sizeof(line),format,args...);Fake::consoleLog+=line;}
 void println(const char *line){Fake::consoleLog+=line;Fake::consoleLog+='\n';}
};
inline FakeSerial Serial;
