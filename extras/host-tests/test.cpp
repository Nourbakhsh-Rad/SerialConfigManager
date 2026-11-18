#include "SerialConfigManager.h"
#include <cassert>
#include <iostream>
int pinModes[64], pinLevels[64]; HardwareSerial Serial, Serial1; FakeFS LittleFS;
#define CHECK(c) do{ if(!(c)){std::cout<<"FAIL line "<<__LINE__<<": " #c "\n";fails++;} }while(0)
int fails=0;
int main(){
  { SerialConfigManager m; CHECK(m.begin(true,true)); CHECK(m.getBaudRate(SER0)==115200 && m.getBaudRate(SER1)==9600);
    CHECK(LittleFS.exists("/serial_config.json")); }
  // roundtrip
  { SerialConfigManager m; m.begin(true,true);
    m.setSerialBaudRate(SER0,57600); m.setSerialParity(SER0,2); m.setSerialPins(SER1,16,17);
    m.setRS485Pins(SER1,18,19); m.setRS485Mode(SER1,SCM_RS485_HALF_DUPLEX); m.setRS485Invert(SER1,true);
    CHECK(m.saveConfig());
    String j=m.configToJson(); SerialConfigManager n; n.begin(true,true);
    CHECK(n.getBaudRate(SER0)==57600 && n.getParity(SER0)==2 && n.getRxPin(SER1)==16 && n.getDePin(SER1)==18 && n.isRS485Inverted(SER1));
    SerialConfigManager k; CHECK(k.parseJsonConfig(j)); CHECK(k.getRS485Mode(SER1)==SCM_RS485_HALF_DUPLEX); }
  // invalid import rejected, config untouched
  { SerialConfigManager m; m.begin(true,true); uint32_t b=m.getBaudRate(SER0);
    CHECK(!m.parseJsonConfig("{\"serial0\":{\"baudRate\":999999},\"serial1\":{}}")); CHECK(m.getBaudRate(SER0)==b); CHECK(m.getLastError().length()>0);
    CHECK(!m.parseJsonConfig("garbage")); }
  // missing baud in serial1 keeps 9600 factory default
  { SerialConfigManager m; m.begin(true,true); CHECK(m.parseJsonConfig("{\"serial0\":{},\"serial1\":{}}")); CHECK(m.getBaudRate(SER1)==9600); }
  // corrupt file -> defaults AND diagnostic preserved
  { LittleFS.files["/serial_config.json"]="{bad"; SerialConfigManager m; m.begin(true,true);
    CHECK(m.getBaudRate(SER0)==115200); std::cout<<"corrupt-file error: ["<<m.getLastError().c_str()<<"]\n"; CHECK(m.getLastError().length()>0); }
  // fs unavailable
  { LittleFS.ok=false; SerialConfigManager m; CHECK(m.begin(true,true)); CHECK(m.getLastError().length()>0); LittleFS.ok=true; }
  // RS485 write toggles + alias
  { SerialConfigManager m; m.begin(true,true); m.setRS485Pins(SER1,18,19); m.enableRS485(SER1,true); m.setSerialPins(SER1,16,17); CHECK(m.applySerialConfig(SER1));
    CHECK(Serial1.began && Serial1.rx==16 && Serial1.tx==17); CHECK(pinLevels[18]==LOW);
    CHECK(m.writeRS485(SER1,"hi")); CHECK(Serial1.wr=="hi"); CHECK(m.writeSerial(SER1,String("!"))); CHECK(pinLevels[18]==LOW && !m.isTransmitMode(SER1)); }
  // only one pin custom is still honoured on ESP32
  { SerialConfigManager m; m.begin(true,true); m.setSerialPins(SER1,-1,17); m.applySerialConfig(SER1); CHECK(Serial1.tx==17 && Serial1.rx==-1); }
  // printConfig into any Stream
  { SerialConfigManager m; m.begin(true,true); Stream s; m.printConfig(s); CHECK(s.out.find("Baud Rate")!=std::string::npos); }
  CHECK(!SerialConfigManager::isValidBaudRate(1234) && SerialConfigManager::isValidBaudRate(2000000));
  std::cout<<(fails?"FAILED":"ALL PASSED")<<"\n"; return fails;
}
