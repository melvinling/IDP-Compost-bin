// Native virtual bin: uses the firmware controller/predictor, never RelayDriver or GPIO.
#include "Controller.h"
#include "Prediction.h"
#include <algorithm>
#include <array>
#include <deque>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace {
const Config::RelaySettings virtualRelays{true,32,33,Config::Polarity::ActiveHigh,Config::Polarity::ActiveHigh,25,Config::Polarity::ActiveHigh};
const char *b(bool value) { return value ? "true" : "false"; }
std::string quote(const char *text) {
  std::string out="\"";
  for (; *text; ++text) { if (*text=='"'||*text=='\\') out+='\\'; if (static_cast<unsigned char>(*text)>=32) out+=*text; }
  return out+'"';
}
void number(std::ostream &out, float value) { if (std::isfinite(value)) out<<value; else out<<"null"; }
void actuator(std::ostream &out, ActuatorCommand c) { out<<"{\"motor\":"<<b(c.motor)<<",\"pump\":"<<b(c.pump)<<",\"fan\":"<<b(c.fan)<<'}'; }
void statusJson(std::ostream &out, const ApplicationStatus &s) {
  out<<"{\"mode\":"<<quote(modeName(s.mode))<<",\"stage\":"<<quote(stageName(s.stage))<<",\"stageElapsedMs\":"<<s.stageElapsedMs
    <<",\"stopLatched\":"<<b(s.stopLatched)<<",\"relayConfigured\":"<<b(s.relayConfigured)<<",\"reason\":"<<quote(s.reason)
    <<",\"periodicMixDue\":"<<b(s.periodicMixDue)<<",\"postWaterMixDue\":"<<b(s.postWaterMixDue)<<",\"cooldownMs\":{\"motor\":"<<s.cooldownMs[0]<<",\"pump\":"<<s.cooldownMs[1]<<",\"fan\":"<<s.cooldownMs[2]<<"},\"requested\":";
  actuator(out,s.requested);out<<",\"commanded\":";actuator(out,s.commanded);
  out<<",\"prediction\":{\"modelReady\":"<<b(s.prediction.modelReady)<<",\"reason\":"<<quote(s.prediction.reason)
    <<",\"action\":"<<quote(decisionName(s.prediction.action))<<",\"confidence\":"<<s.prediction.confidence
    <<",\"provenance\":\"synthetic_demo\",\"model\":\"Random Forest · 32 trees / depth 6\",\"command\":";
  actuator(out,s.prediction.command);out<<"}}";
}
class Bin {
 public:
  Controller controller{virtualRelays};
  uint64_t now=0, nextSample=0;
  uint32_t sequence=0, missing=0;
  bool frozen=false;
  std::array<float,6> values{100,1500,5,800,50,28}; // API order, original four first.
  SensorSnapshot latest;
  std::deque<SensorSnapshot> history;
  std::deque<std::pair<uint64_t,std::string>> events;
  ActuatorCommand previous;
  void event(const std::string &text) { events.emplace_back(now,text);if(events.size()>100)events.pop_front(); }
  void recordOutputs() {
    auto c=controller.status().commanded;
    const bool old[]={previous.motor,previous.pump,previous.fan},current[]={c.motor,c.pump,c.fan};
    const char *names[]={"Mixer","Water pump","Drying fan"};
    for(unsigned i=0;i<3;++i)if(old[i]!=current[i])event(std::string(names[i])+(current[i]?" ON":" OFF"));
    previous=c;
  }
  void sample() {
    latest={};latest.sequence=++sequence;latest.acquiredMs=uint32_t(now);latest.uptimeMs=now;
    Reading *readings[]={&latest.distance,&latest.ec,&latest.ph,&latest.co2,&latest.moisture,&latest.temperature};
    for(unsigned i=0;i<6;++i){auto &r=*readings[i];r.acquiredMs=uint32_t(now);r.lastSuccessMs=now;
      r.status=missing&(1u<<i)?SensorStatus::NoReply:SensorStatus::Ok;r.value=r.status==SensorStatus::Ok?values[i]:NAN;
    }
    controller.updateSample(latest,predictActuators(latest,controller.context(uint32_t(now))));
    history.push_back(latest);if(history.size()>Config::HISTORY_SIZE)history.pop_front();
  }
  void step(uint32_t duration) {
    const uint64_t end=now+duration;
    do {
      const uint32_t dt=uint32_t(std::min<uint64_t>(Config::SAFETY_TICK_MS,end-now));
      // Illustrative moisture/ventilation response, not a calibrated compost process.
      const float seconds=dt/1000.0f;auto c=controller.status().commanded;
      values[4]=std::clamp(values[4]+seconds*((c.pump?.5f:0)-(c.fan?.12f:0)-.001f),0.0f,100.0f);
      values[5]+=seconds*(25-values[5])/6000.0f;
      values[3]=std::clamp(values[3]+seconds*(800-values[3])*(c.fan?.01f:.0001f),0.0f,10000.0f);
      now+=dt;
      if(now>=nextSample){if(!frozen)sample();nextSample=now+Config::SAMPLE_MS;}
      controller.tick(uint32_t(now));recordOutputs();
    }while(now<end);
  }
  ControlReply command(const std::string &name) {
    ControlAction a{};if(!parseControlAction(name.c_str(),a))return {false,"Unknown command"};
    const auto reply=controller.command(a,uint32_t(now));recordOutputs();event(name+": "+reply.message);return reply;
  }
  void scenario(const std::string &name) {
    *this=Bin{};
    if(name=="dry")values[4]=25;
    if(name=="wet")values[4]=80;
    if(name=="missing")missing=1u<<4;
    if(name!="fermentation")command("stage_finishing");
    if(name=="mix_due"){now=Config::MIX_INTERVAL_MS;nextSample=now;}
    step(0);command("mode_auto"); // Invalidate earlier context; acquire once for new mode.
    sample();nextSample=now+Config::SAMPLE_MS;controller.tick(uint32_t(now));recordOutputs();
    if(name=="stale")frozen=true;
    event("Scenario: "+name);
  }
  void json(const ControlReply &reply={true,"OK"}) const {
    std::cout<<std::setprecision(8)<<"{\"accepted\":"<<b(reply.accepted)<<",\"message\":"<<quote(reply.message)
      <<",\"schemaVersion\":2,\"sampleMs\":"<<Config::SAMPLE_MS<<",\"freshMs\":"<<Config::FRESH_MS<<",\"simulation\":true,\"uptime\":"<<now<<",\"sequence\":"<<sequence<<",\"status\":";
    statusJson(std::cout,controller.status());
    std::cout<<",\"latest\":{\"sequence\":"<<latest.sequence<<",\"uptime\":"<<latest.uptimeMs<<",\"readings\":[";
    const Reading *readings[]={&latest.distance,&latest.ec,&latest.ph,&latest.co2,&latest.moisture,&latest.temperature};
    for(unsigned i=0;i<6;++i){if(i)std::cout<<',';const auto &r=*readings[i];const uint32_t age=uint32_t(now-r.acquiredMs);
      const bool fresh=r.valid()&&age<Config::FRESH_MS;
      std::cout<<"{\"value\":";number(std::cout,fresh?r.value:NAN);
      std::cout<<",\"valid\":"<<b(fresh)<<",\"status\":"<<quote(r.valid()&&!fresh?"Stale reading":statusName(r.status))
        <<",\"ageMs\":"<<age<<",\"lastSuccessMs\":"<<r.lastSuccessMs<<",\"exceptionCode\":0}";
    }
    std::cout<<"]},\"samples\":[";bool first=true;
    for(const auto &s:history){if(!first)std::cout<<',';first=false;std::cout<<'['<<s.sequence<<','<<s.uptimeMs;
      for(const auto *r:{&s.distance,&s.ec,&s.ph,&s.co2,&s.moisture,&s.temperature}){std::cout<<',';number(std::cout,r->value);}std::cout<<']';}
    std::cout<<"],\"inputs\":[";for(unsigned i=0;i<6;++i){if(i)std::cout<<',';number(std::cout,values[i]);}
    std::cout<<"],\"missingMask\":"<<missing<<",\"frozen\":"<<b(frozen)<<",\"events\":[";first=true;
    for(const auto &e:events){if(!first)std::cout<<',';first=false;std::cout<<'['<<e.first<<','<<quote(e.second.c_str())<<']';}
    std::cout<<"]}"<<std::endl;
  }
};
}
int main(){
  Bin bin;bin.step(0);std::string line;
  while(std::getline(std::cin,line)){
    std::istringstream in(line);std::string operation;in>>operation;ControlReply reply{true,"OK"};
    if(operation=="advance"){uint32_t ms=0;if(!(in>>ms)||ms>60000)reply={false,"Invalid advance"};else bin.step(ms);}
    else if(operation=="command"){std::string name;in>>name;reply=bin.command(name);}
    else if(operation=="reset"){bin=Bin{};bin.step(0);}
    else if(operation=="scenario"){std::string name;in>>name;bin.scenario(name);}
    else if(operation=="set"){
      auto values=bin.values;uint32_t missing=0;int frozen=0;
      bool ok=true;for(auto &v:values)if(!(in>>v)||!std::isfinite(v))ok=false;
      if(!(in>>missing>>frozen))ok=false;
      if(ok){bin.values=values;bin.missing=missing;bin.frozen=frozen!=0;bin.event("Sensor inputs updated; acquisition on next sample");}
      else reply={false,"Invalid sensor inputs"};
    }else if(operation!="data")reply={false,"Unknown operation"};
    bin.json(reply);
  }
}
