#include <cassert>
#include <iostream>
#include <limits>
#include <fstream>
#include <sstream>
#include "Arduino.h"
#include "../src/ModbusProtocol.h"
#include "../src/Sensors.h"
#include "../src/Controller.h"
#include "../src/Prediction.h"
#include "../src/RelayDriver.h"
#include "../src/History.h"
#include "../src/Api.h"
#include "../src/WebServerModule.h"
#undef assert
#define assert(expr) do { if (!(expr)) { std::cerr << "FAIL " << #expr << " at " << __LINE__ << "\n"; std::exit(1); } } while (0)
using namespace ModbusProtocol;
std::vector<uint8_t> frame(uint16_t value,uint8_t address=1) {
 std::vector<uint8_t> b{address,3,2,uint8_t(value>>8),uint8_t(value)};
 auto crc=crc16(b.data(),b.size());b.push_back(crc&255);b.push_back(crc>>8);return b;
}
std::vector<uint8_t> exception(){
 std::vector<uint8_t>b{1,0x83,2};auto crc=crc16(b.data(),b.size());b.push_back(crc&255);b.push_back(crc>>8);return b;
}
int parse(const std::vector<uint8_t>&b,uint16_t &value){
 ReplyStream p;int result=0;for(auto x:b){result=p.feed(x,1,1,&value);if(result)break;}return result;
}
SensorSnapshot valid(uint32_t now=1000){
 SensorSnapshot s;s.sequence=1;s.acquiredMs=now;s.uptimeMs=now;
 for(auto r:{&s.distance,&s.ec,&s.ph,&s.co2,&s.moisture,&s.temperature}){r->status=SensorStatus::Ok;r->value=0;r->acquiredMs=now;r->lastSuccessMs=now;}
 return s;
}
Config::RelaySettings verified(){return {true,32,33,Config::Polarity::ActiveHigh,Config::Polarity::ActiveLow,25,Config::Polarity::ActiveHigh};}
void protocol(){
 const uint8_t known[]{1,3,0,2,0,1};assert(crc16(known,6)==0xCA25);
 uint8_t requestBytes[8];request(requestBytes,1,3,2,1);assert(requestBytes[6]==0x25&&requestBytes[7]==0xCA);
 uint16_t v=999;assert(parse(frame(0),v)==1&&v==0);
 auto noisy=std::vector<uint8_t>{0x55,0xff,1,3,9};auto good=frame(42);noisy.insert(noisy.end(),good.begin(),good.end());assert(parse(noisy,v)==1&&v==42);
 std::vector<uint8_t>echo(requestBytes,requestBytes+8);assert(parse(echo,v)==0);
 echo.insert(echo.end(),good.begin(),good.end());assert(parse(echo,v)==1&&v==42);
 assert(parse({0x7F,0x7E,0xFB,0xFF,0xFD,0xB5,0x6B},v)==0);
 assert(parse({0x7F,0x7E,0xF9,0xFF,0xFD,0x17,0xEB,0},v)==0);
 assert(parse({0x7F,0x7E,0xFF,0xFF,0xFD,0xF7,0x3D},v)==0);
 assert(parse(frame(42,2),v)==0);auto bad=good;bad.back()^=1;assert(parse(bad,v)==0);
 assert(parse({1,3,2,0},v)==0);assert(parse(exception(),v)==-3);
 ReplyStream stream;for(unsigned i=0;i<5000;i++)assert(stream.feed(0x55,1,1,&v)==0);
 std::cout<<"PASS protocol CRC, zeros, echoes, captured inverted echoes, noise, exceptions, malformed/wrong address\n";
}
SensorSnapshot sample(Sensors &s,bool pulse=true,uint32_t pulseWidth=5750){
 SensorSnapshot out;bool rose=false,fell=false,triggered=false;uint64_t trigger=0;
 size_t events=Fake::pins.size();
 for(unsigned i=0;i<7500;i++){
  if(s.tick(out))return out;
  for(;events<Fake::pins.size();events++){auto e=Fake::pins[events];if(!e.mode&&e.pin==27&&e.value==LOW){triggered=true;trigger=Fake::us;}}
  if(pulse&&triggered&&!rose&&Fake::us>=trigger+5000){Fake::pulseLevel(HIGH);rose=true;}
  if(pulse&&rose&&!fell&&Fake::us>=trigger+5000+pulseWidth){Fake::pulseLevel(LOW);fell=true;}
  Fake::advance(1);
 }
 assert(false&&"Acquisition failed to complete within bounded window");return {};
}
void sensorTests(){
 Fake::reset();Sensors s;s.begin();Fake::advance(1000);
 assert(Fake::serialRx==16&&Fake::serialTx==17);
 Fake::responses={frame(0),frame(70),frame(500),frame(uint16_t(int16_t(-123))),frame(0)};auto a=sample(s);
 assert(a.distance.valid()&&a.distance.value>99&&a.distance.value<105);
 assert(a.ec.valid()&&a.ec.value==0&&a.ph.value==7&&a.co2.valid()&&a.co2.value==0);
 assert(a.ec.lastSuccessMs>0&&a.co2.lastSuccessMs>a.ec.lastSuccessMs);
 assert(Fake::tx.size()==5&&Fake::tx[0].baud==4800&&Fake::tx[3].baud==4800&&Fake::tx[4].baud==9600);
 assert(a.moisture.value==50&&a.temperature.value==-12.3f);
 assert(Fake::tx[0].bytes[3]==2&&Fake::tx[1].bytes[3]==3&&Fake::tx[2].bytes[3]==0&&Fake::tx[3].bytes[3]==1&&Fake::tx[4].bytes[3]==0);
 Fake::responses={{},frame(141),frame(1001),frame(801),exception()};auto b=sample(s,false);
 assert(!b.distance.valid()&&b.distance.status==SensorStatus::NoPulse);
 assert(!b.ec.valid()&&isnan(b.ec.value)&&b.ec.status==SensorStatus::NoReply&&b.ec.lastSuccessMs==a.ec.lastSuccessMs);
 assert(b.ph.status==SensorStatus::OutOfRange&&!b.ph.valid()&&isnan(b.ph.value));
 assert(b.co2.status==SensorStatus::Exception&&b.co2.exceptionCode==2);
 assert(b.moisture.status==SensorStatus::OutOfRange&&b.temperature.status==SensorStatus::OutOfRange);
 Fake::responses={frame(12),{},frame(0),frame(0),frame(56)};auto c=sample(s,true,30000);
 assert(c.distance.status==SensorStatus::OutOfRange&&c.ec.value==12&&!c.ph.valid()&&c.co2.value==56);
 assert(c.acquiredMs-b.acquiredMs==4000);
 Fake::responses={{},{},{},{},{}};auto started=millis();auto d=sample(s,false);
 assert(millis()-d.acquiredMs<3300&&millis()-started<7500);
 assert(d.ec.status==SensorStatus::NoReply&&d.ph.status==SensorStatus::NoReply&&d.co2.status==SensorStatus::NoReply);
 Fake::continuousNoise=true;auto e=sample(s,false);assert(e.ec.status==SensorStatus::InvalidReply);
 assert(!a.fresh(a.co2.acquiredMs+6000,6000));
 std::cout<<"PASS cooperative sensors, per-device baud/registers, independent failures, zeros, invalid pH, pulse bounds, timestamps, noise/timeouts\n";
}
void diagnosticTests(){
 Fake::reset();Sensors s;s.begin();SensorSnapshot ignored;
 auto console=[](const std::string &command){for(auto c:command)Fake::consoleRx.push_back(c);};
 console("trace on\nRX INVERTED\n");s.tick(ignored);
 assert(!Fake::inverted); // Never switch polarity partway through a transaction/sample.
 Fake::advance(1000);
 uint8_t requestBytes[8];request(requestBytes,1,3,2,1);
 Fake::responses={std::vector<uint8_t>(requestBytes,requestBytes+8),frame(70),frame(500),frame(250),frame(600)};
 auto a=sample(s,false);
 assert(Fake::inverted&&!a.ec.valid()&&a.ec.status==SensorStatus::InvalidReply);
 assert(a.ph.value==7&&a.co2.value==600); // CRC-valid replies still accepted in diagnostic mode.
 assert(Fake::consoleLog.find("TX: 01 03 00 02 00 01 25 CA")!=std::string::npos);
 assert(Fake::consoleLog.find("RX (8 bytes, first 8): 01 03 00 02 00 01 25 CA")!=std::string::npos);
 assert(Fake::consoleLog.find("RX INVERTED")!=std::string::npos);
 assert(Fake::tx.size()==5&&Fake::tx[4].baud==9600&&Fake::inverted);
 console(std::string(80,'X')+"\n");for(unsigned i=0;i<4;i++)s.tick(ignored);
 assert(Fake::consoleLog.find("too long; ignored")!=std::string::npos);
 console("TRACE OFF\nRX NORMAL\n");s.tick(ignored);
 Fake::responses={frame(12),frame(60),frame(500),frame(250),frame(500)};
 auto b=sample(s,false);assert(!Fake::inverted&&b.ec.value==12);
 auto logSize=Fake::consoleLog.size();Fake::responses={frame(12),frame(60),frame(500),frame(250),frame(500)};sample(s,false);
 assert(Fake::consoleLog.substr(logSize).find("TX:")==std::string::npos);
 for(const auto &tx:Fake::tx)assert(tx.bytes[1]==3); // No configuration writes.
 for(const auto &pin:Fake::pins)assert(pin.pin==27||pin.pin==35); // No relay operations.
 std::cout<<"PASS bounded Serial commands, raw request-echo trace, CRC retention, sample-boundary RX polarity, baud changes, trace OFF\n";
}
void install(Controller &c, uint32_t now, ActuatorCommand command, bool ready=true){
 auto snapshot=valid(now);snapshot.moisture.value=50;snapshot.temperature.value=28;
 PredictionResult p{command,ready,"Test model"};p.contextRevision=c.context(now).revision;c.updateSample(snapshot,p);
}
void controlTests(){
 Controller disabled;assert(disabled.status().mode==Mode::Manual);assert(!disabled.command(ControlAction::MotorStart,0).accepted);
 Controller sealed(verified());assert(!sealed.command(ControlAction::FanStart,0).accepted);assert(!sealed.tick(1).fan);
 Controller c(verified());c.command(ControlAction::FinishingStage,0);
 assert(c.command(ControlAction::MotorStart,100).accepted&&c.status().commanded.motor);
 assert(c.tick(5099).motor);assert(!c.tick(5100).motor);assert(c.status().cooldownMs[0]==60000);
 assert(!c.command(ControlAction::MotorStart,5101).accepted);
 assert(c.command(ControlAction::PumpStart,6000).accepted);assert(!c.command(ControlAction::FanStart,6001).accepted);
 assert(c.command(ControlAction::AutoMode,6002).accepted&&!c.status().commanded.pump);
 install(c,6003,{false,false,true});assert(c.tick(6003).fan);
 c.command(ControlAction::FanStop,6004);install(c,6005,{false,false,true});assert(!c.tick(6005).fan);
 c.command(ControlAction::AutoMode,40005);assert(!c.tick(40005).fan); // Old-context inference cannot restart.
 install(c,40006,{false,false,true});assert(c.tick(40006).fan);
 c.command(ControlAction::StopAll,40007);assert(c.status().stopLatched&&!c.tick(40008).fan);
 c.command(ControlAction::AutoMode,40009);install(c,40010,{false,false,true});assert(!c.tick(40010).fan);
 c.command(ControlAction::ResetStop,40011);assert(c.status().mode==Mode::Manual&&!c.tick(40011).fan);
 c.command(ControlAction::AutoMode,80000);install(c,80000,{false,false,true});assert(c.tick(80000).fan);
 assert(!c.tick(88000).fan&&c.status().cooldownMs[2]==30000); // Fan pulse 8s.
 install(c,90000,{false,false,true});assert(!c.tick(90000).fan);
 install(c,118000,{false,false,true});assert(c.tick(118000).fan);
 c.command(ControlAction::FermentationStage,118001);assert(!c.status().commanded.fan);install(c,118002,{false,false,true});assert(!c.tick(118002).fan);
 Controller water(verified());water.command(ControlAction::FinishingStage,0);water.command(ControlAction::AutoMode,0);
 install(water,1000,{false,true,false});assert(water.tick(1000).pump);assert(water.tick(2999).pump);assert(!water.tick(3000).pump);
 assert(water.status().postWaterMixDue&&water.status().cooldownMs[1]==60000);
 install(water,3001,{true,false,false});assert(water.tick(3001).motor);assert(!water.tick(8001).motor&&!water.context(8001).postWaterMixDue);
 assert(!water.context(8001+1799999).periodicMixDue&&water.context(8001+1800000).periodicMixDue);
 Controller stale(verified());stale.command(ControlAction::FinishingStage,0);stale.command(ControlAction::AutoMode,0);
 install(stale,1000,{false,false,true});assert(stale.tick(1000).fan);
 assert(!stale.tick(9000).fan);
 // Reacquire before cooldown expiry, then start with 11.99s-old values.
 install(stale,32010,{false,false,true});assert(!stale.tick(32010).fan);
 assert(stale.tick(44000).fan);assert(!stale.tick(44010).fan);
 assert(std::string(stale.status().reason)=="Automatic inputs invalid or stale");
 auto broken=valid(50000);broken.moisture.status=SensorStatus::NoReply;PredictionResult p{{true,false,false},true,"Test"};p.contextRevision=stale.context(50000).revision;
 stale.updateSample(broken,p);assert(!stale.tick(50000).motor);
 install(stale,50001,{true,false,false},false);assert(!stale.tick(50001).motor);
 Controller conflict(verified());conflict.command(ControlAction::FinishingStage,0);conflict.command(ControlAction::AutoMode,0);
 install(conflict,1000,{false,true,true});assert(!conflict.tick(1000).pump&&!conflict.tick(1000).fan);
 Controller runtime(verified());runtime.command(ControlAction::FinishingStage,0);runtime.command(ControlAction::MotorStart,100);
 runtime.command(ControlAction::MotorStart,4000);runtime.command(ControlAction::MotorStart,8000);
 assert(runtime.tick(10099).motor);assert(!runtime.tick(10100).motor&&runtime.status().stopLatched);
 runtime.command(ControlAction::ResetStop,10101);assert(!runtime.tick(10102).motor);
 Controller rollover(verified());rollover.command(ControlAction::FinishingStage,UINT32_MAX-2001);rollover.command(ControlAction::MotorStart,UINT32_MAX-2000);
 assert(rollover.tick(100).motor);assert(!rollover.tick(3000).motor);
 auto missing=predictActuators(SensorSnapshot{});assert(!missing.modelReady&&!missing.command.motor);
 auto snapshot=valid();snapshot.moisture.value=25;snapshot.temperature.value=28;snapshot.ph.value=5;snapshot.ec.value=1500;snapshot.co2.value=800;snapshot.distance.value=100;
 auto dry=predictActuators(snapshot,{Stage::Finishing,false,false,123});assert(dry.modelReady&&dry.action==Decision::Water&&dry.command.pump&&dry.contextRevision==123);
 std::cout<<"PASS shared control: stages, manual, automatic pulses/cooldowns, post-water/periodic mixing, stop precedence, invalid/stale/context guards, interlock, runtime, rollover\n";
}
void parity(){
 std::ifstream input("ml/parity.csv");assert(input.good());std::string line;std::getline(input,line);unsigned count=0;
 while(std::getline(input,line)){
  std::istringstream stream(line);std::string field;float x[9],expected[4],actual[4];
  for(auto &v:x){assert(bool(std::getline(stream,field,',')));v=std::stof(field);}
  for(auto &v:expected){assert(bool(std::getline(stream,field,',')));v=std::stof(field);}
  modelProbabilities(x,actual);for(unsigned i=0;i<4;++i)assert(std::fabs(expected[i]-actual[i])<0.000002f);++count;
 }
 assert(count>=4000);std::cout<<"PASS sklearn/C++ probability parity across "<<count<<" rows and threshold neighbours\n";
}
void relayTests(){
 Fake::reset();RelayDriver disabled;disabled.begin();disabled.apply({true,true});assert(Fake::pins.empty());
 auto bad=verified();bad.motorPin=bad.pumpPin;RelayDriver duplicate(bad);duplicate.begin();assert(Fake::pins.empty());
 for(int p:{0,1,3,6,16,17,27,34,35,36,39}){bad=verified();bad.motorPin=p;assert(relayConfigurationError(bad));RelayDriver driver(bad);driver.begin();assert(Fake::pins.empty());}
 bad=verified();bad.motorPolarity=Config::Polarity::Unconfigured;assert(relayConfigurationError(bad));
 RelayDriver configured(verified());configured.begin();assert(Fake::pins.size()==6);
 assert(Fake::pins[0].pin==32&&Fake::pins[0].value==LOW&&!Fake::pins[0].mode);
 assert(Fake::pins[2].pin==33&&Fake::pins[2].value==HIGH);
 configured.apply({true,true,true});assert(Fake::levels[32]==HIGH&&Fake::levels[33]==LOW&&Fake::levels[25]==HIGH);
 configured.apply({});assert(Fake::levels[32]==LOW&&Fake::levels[33]==HIGH&&Fake::levels[25]==LOW);
 for(auto e:Fake::pins)assert(e.pin!=26);
 std::cout<<"PASS disabled GPIO silence, duplicate/conflict/polarity rejection, configured OFF initialization, polarity, unused channels untouched\n";
}
Controller apiController;
ApplicationStatus provide(){return apiController.status();}
ControlReply execute(ControlAction a){return apiController.command(a,millis());}
std::string extract(const std::string&s,const std::string&key){auto p=s.find("\""+key+"\":\"");assert(p!=std::string::npos);p+=key.size()+4;return s.substr(p,s.find('"',p)-p);}
void apiTests(){
 Fake::us=1000000;
 uint32_t n;assert(Api::unsignedNumber("4294967295",n)&&n==UINT32_MAX);for(auto s:{"","-1"," 2","2x","4294967296"})assert(!Api::unsignedNumber(s,n));
 ControlAction a;assert(!Api::validateControl("GET","application/x-www-form-urlencoded","a","a",1,"command","motor_start",a).accepted);
 History history;auto latest=valid(1000);history.append(latest);
 WebServerModule web;web.begin(history,latest,provide,execute);auto &server=*WebServer::instance;
 server.request("/api/data",HTTP_GET);assert(server.code==200);
 const std::string json=server.response.c_str();const auto token=extract(json,"controlToken"),boot=extract(json,"boot");
 assert(json.find("\"mode\":\"manual\"")!=std::string::npos&&json.find("\"relayConfigured\":false")!=std::string::npos);
 assert(json.find("\"value\":0.00")!=std::string::npos);
 Fake::advance(12000);server.request("/api/data",HTTP_GET);
 assert(std::string(server.response.c_str()).find("\"value\":null,\"valid\":false,\"status\":\"Stale reading\"")!=std::string::npos);
 Fake::us=1000000;
 server.request("/api/control",HTTP_GET,{{"command","motor_start"}});assert(server.code==405&&!apiController.status().commanded.motor);
 server.request("/api/control",HTTP_POST,{{"command","motor_start"}});assert(server.code==415);
 const std::map<std::string,String>headers{{"Content-Type","application/x-www-form-urlencoded"},{"X-Control-Token",token}};
 server.request("/api/control",HTTP_POST,{{"command","motor_start"}},headers);assert(server.code==409&&std::string(server.response.c_str()).find("Physical relay control disabled")!=std::string::npos);
 server.request("/api/control",HTTP_POST,{{"command","unknown"}},headers);assert(server.code==400);
 server.request("/api/control",HTTP_POST,{{"command","stop_all"},{"command","motor_start"}},headers);assert(server.code==400);
 server.request("/api/control",HTTP_POST,{{"command","stop_all"}},{{"Content-Type","application/x-www-form-urlencoded"}});assert(server.code==400);
 server.request("/api/control",HTTP_POST,{{"command","stop_all"}},headers);assert(server.code==200&&apiController.status().stopLatched);
 server.request("/api/control",HTTP_POST,{{"command","reset_stop"}},headers);assert(server.code==200&&!apiController.status().stopLatched&&!apiController.status().commanded.motor);
 latest.ph.status=SensorStatus::NoReply;latest.ph.value=7;
 server.request("/api/data",HTTP_GET,{{"boot",boot},{"after","1"}});assert(server.code==200);assert(std::string(server.response.c_str()).find("\"value\":null")!=std::string::npos);
 server.request("/api/data",HTTP_GET,{{"after","-1"}});assert(server.code==400);
 for(unsigned i=2;i<920;i++){latest.sequence=i;history.append(latest);}
 assert(history.count()==900&&history.at(0).sequence==20);
 server.request("/api/data",HTTP_GET,{{"boot",boot},{"after","1"}});assert(server.code==200&&std::string(server.response.c_str()).find("\"more\":true")!=std::string::npos);
 server.request("/api/data",HTTP_GET,{{"boot","restart"},{"after","919"}});assert(std::string(server.response.c_str()).find("\"reset\":true")!=std::string::npos);
 std::cout<<"PASS actual API handlers (stub transport), method/content/token/schema rejection, authoritative disabled/stop/reset state, JSON null, incremental/restart/history paging\n";
}
int main(){protocol();sensorTests();diagnosticTests();controlTests();parity();relayTests();apiTests();std::cout<<"All core checks passed\n";}
