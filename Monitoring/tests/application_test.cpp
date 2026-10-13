#include <iostream>
#include "Arduino.h"
#include "WebServer.h"
#include "../src/Application.h"
#include "../src/Prediction.h"
unsigned calls=0,invalidCalls=0;
PredictionResult predictActuators(const SensorSnapshot &s, const ControlContext &context){
 (void)context;
 ++calls;if(!s.valid())++invalidCalls;
 return {{false,false},false,"Test hook: no trained model installed"};
}
#define CHECK(e) do {if(!(e)){std::cerr<<"FAIL "<<#e<<"\n";return 1;}}while(0)
int main(){
 Fake::reset();Application::begin();CHECK(WebServer::instance);CHECK(Fake::edge);
 for(auto e:Fake::pins)CHECK(e.pin==27||e.pin==35);
 for(unsigned i=0;i<8300;i++)Application::tick(); // Real acquisition state machine, missing sensors.
 CHECK(calls==2&&invalidCalls==2);
 auto &server=*WebServer::instance;server.request("/api/data",HTTP_GET);
 CHECK(server.code==200);std::string json=server.response.c_str();
 CHECK(json.find("Test hook: no trained model installed")!=std::string::npos);
 CHECK(json.find("\"sequence\":2")!=std::string::npos);
 CHECK(json.find("\"commanded\":{\"motor\":false,\"pump\":false,\"fan\":false}")!=std::string::npos);
 std::cout<<"PASS real application coordination: every completed invalid snapshot invokes ML and publishes result; disabled relays untouched\n";
}
