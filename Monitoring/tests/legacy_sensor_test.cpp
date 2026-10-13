#include "Arduino.h"
#include "Sensors.h"
#include "Config.h"
#include <iostream>
#define CHECK(e) do {if(!(e)){std::cerr<<"FAIL "<<#e<<"\n";return 1;}}while(0)
std::vector<uint8_t> reply(uint16_t value) {
 std::vector<uint8_t> data{1,3,2,uint8_t(value>>8),uint8_t(value)};
 auto crc=ModbusProtocol::crc16(data.data(),data.size());data.push_back(crc&255);data.push_back(crc>>8);return data;
}
int main(){
 CHECK(Config::SOIL_PROFILE==Config::SoilProfile::EcPh&&Config::READ_COUNT==3);
 Fake::reset();Sensors sensors;sensors.begin();Fake::responses={reply(1234),reply(70),reply(900)};
 SensorSnapshot snapshot;bool complete=false;
 for(unsigned i=0;i<3000&&!complete;++i){complete=sensors.tick(snapshot);Fake::advance(1);}
 CHECK(complete&&snapshot.ec.value==1234&&snapshot.ph.value==7&&snapshot.co2.value==900);
 CHECK(Fake::tx.size()==3&&Fake::tx[2].baud==9600);
 CHECK(snapshot.moisture.status==SensorStatus::Unsupported&&snapshot.temperature.status==SensorStatus::Unsupported);
 CHECK(std::isnan(snapshot.moisture.value)&&std::isnan(snapshot.temperature.value)&&!snapshot.valid());
 std::cout<<"PASS EC/pH-only profile: three reads, CO2 baud switch, unsupported moisture/temperature and no invented automatic inputs\n";
}
