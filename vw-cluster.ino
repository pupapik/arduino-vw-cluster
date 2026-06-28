#include <SPI.h>
#include <mcp2515.h>
MCP2515 mcp2515(10);

int speed_target = 0;
float speed_kmh = 0;
int rpm = 800;
float displayCorr = 1.16;
unsigned long lastSend = 0, lastSlow = 0;

uint16_t distCounter = 0;
float distRemainder = 0;
uint16_t fuelCounter = 0;

uint8_t spd_pkt[] = {0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0xAD};
uint8_t rpm_pkt[] = {0x49,0x0E,0x00,0x00,0x0E,0x00,0x1B,0x0E};
uint8_t fcs_pkt[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
uint8_t abg_pkt[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
uint8_t ind_pkt[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
uint8_t otp_pkt[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
uint8_t ctp_pkt[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
uint8_t ssm_pkt[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
uint8_t abs_pkt[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

const uint8_t ctp_code[] = {
  0x01,0x08,0x0E,0x15,0x1C,0x22,0x29,0x30,0x36,0x3D,
  0x44,0x4A,0x51,0x58,0x5F,0x66,0x6C,0x72,0x79,0x80,
  0x85,0x89,0x8E,0x92,0x96,0x9A,0x9E,0xA2,0xD1,0xD4,
  0xD7,0xDB,0xDE,0xE1,0xE6,0xED
};

String inBuf = "";

void setRPM(int n){ rpm=n; rpm_pkt[2]=(uint8_t)((rpm*4)&0xFF); rpm_pkt[3]=(uint8_t)((rpm*4)>>8); }
void setSpeed(uint16_t v){ uint16_t raw=(uint16_t)(v*148); spd_pkt[1]=raw&0xFF; spd_pkt[2]=raw>>8; }
void setDist(uint16_t d){ spd_pkt[5]=d&0xFF; spd_pkt[6]=d>>8; }
void setFcBytes(uint16_t c){ fcs_pkt[3]=c&0xFF; fcs_pkt[2]=c>>8; }
void setBacklight(uint8_t l){ ind_pkt[2]=l; }
void setTurnSignal(uint8_t ts){ ind_pkt[0]&=~0x03; ind_pkt[0]|=ts&0x03; }
void setFogLight(uint8_t s){ if(s)ind_pkt[7]|=0x20; else ind_pkt[7]&=~0x20; }
void setHighBeam(uint8_t s){ if(s)ind_pkt[7]|=0x40; else ind_pkt[7]&=~0x40; }
void setCruiseCtl(uint8_t s){ ctp_pkt[2]&=~0x80; ctp_pkt[2]|=(s<<7)&0x80; }
void setBatInd(uint8_t s){ ind_pkt[0]&=~0x80; ind_pkt[0]|=(s<<7)&0x80; }
void setTirePress(uint8_t s){ spd_pkt[3]&=~0x08; spd_pkt[3]|=(s<<3)&0x08; }
void setEpc(uint8_t s){ fcs_pkt[1]&=~0x04; fcs_pkt[1]|=(s<<2)&0x04; }
void setDpf(uint8_t s){ fcs_pkt[5]&=~0x02; fcs_pkt[5]|=(s<<1)&0x02; }
void setDoor(uint8_t d){ ind_pkt[1]&=~0x3F; ind_pkt[1]|=d&0x3F; }
void setSeatbelt(uint8_t s){ abg_pkt[2]&=~0x04; abg_pkt[2]|=(s<<2)&0x04; }
void setOilTemp(int t){ if((t<195)&&(t>49)) otp_pkt[7]=t+60; else otp_pkt[7]=0; }
void setCoolantTemp(int t){ uint8_t i=0; if((t<131)&&(t>-46)) i=(t+45)/5; ctp_pkt[1]=ctp_code[i]; }
void setDispMsg(uint8_t m){ ssm_pkt[0]=(m>0&&m<16)?m:0; }

void canSend(uint16_t id, uint8_t *buf){
  struct can_frame f; f.can_id=id; f.can_dlc=8;
  for(int i=0;i<8;i++) f.data[i]=buf[i];
  mcp2515.sendMessage(&f);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  mcp2515.reset();
  mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);
  mcp2515.setNormalMode();
  setRPM(500); setSpeed(0); setOilTemp(90); setCoolantTemp(90);
  setTurnSignal(0); setFogLight(0); setHighBeam(0);
  setBatInd(0); setTirePress(0); setEpc(0); setDpf(0);
  setDoor(0); setDispMsg(0);
  Serial.println(F("Bereit. 'help' fuer Befehle"));
}

void printHelp(){
  Serial.println(F("--- BEFEHLE ---"));
  Serial.println(F("sXXX Speed | rXXX RPM | kX.XX Korrektur"));
  Serial.println(F("tX Blinker(0-3) | lX BlinkL | hX BlinkR"));
  Serial.println(F("HX Fernlicht | fX Nebel | ccX Tempomat"));
  Serial.println(F("bwX Batt | tpX Reifen | epcX EPC | dpfX DPF"));
  Serial.println(F("bX Gurt | ToXX Oel | TcXX Kuehlw"));
  Serial.println(F("dXX Tueren | blXX Backlight | mXX Meldung"));
  Serial.println(F("---------------"));
}

void applyCmd(String t){
  t.trim();
  if(t.length()<1) return;
  if(t=="help"){ printHelp(); return; }

  if(t.startsWith("Tc")){ setCoolantTemp(t.substring(2).toInt()); return; }
  if(t.startsWith("To")){ setOilTemp(t.substring(2).toInt()); return; }
  if(t.startsWith("epc")){ setEpc(t.substring(3).toInt()); return; }
  if(t.startsWith("dpf")){ setDpf(t.substring(3).toInt()); return; }
  if(t.startsWith("tp")){ setTirePress(t.substring(2).toInt()); return; }
  if(t.startsWith("bw")){ setBatInd(t.substring(2).toInt()); return; }
  if(t.startsWith("cc")){ setCruiseCtl(t.substring(2).toInt()); return; }
  if(t.startsWith("bl")){ setBacklight((uint8_t)t.substring(2).toInt()); return; }

  char c=t.charAt(0);
  String a=t.substring(1);
  if(c=='s'||c=='S'){ speed_target=a.toInt(); }
  else if(c=='r'||c=='R'){ setRPM(a.toInt()); }
  else if(c=='k'){ displayCorr=a.toFloat(); }
  else if(c=='t'){ setTurnSignal(a.toInt()); }
  else if(c=='l'){ if(a.toInt())ind_pkt[0]|=0x01; else ind_pkt[0]&=~0x01; }
  else if(c=='h'){ if(a.toInt())ind_pkt[0]|=0x02; else ind_pkt[0]&=~0x02; }
  else if(c=='f'){ setFogLight(a.toInt()); }
  else if(c=='H'){ setHighBeam(a.toInt()); }
  else if(c=='b'){ setSeatbelt(a.toInt()); }
  else if(c=='d'){ setDoor((uint8_t)a.toInt()); }
  else if(c=='m'){ setDispMsg((uint8_t)a.toInt()); }
}

void readSerial() {
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch==';') {
      applyCmd(inBuf);
      inBuf="";
    } else if (ch=='\n' || ch=='\r') {
      inBuf="";
    } else {
      inBuf += ch;
      if (inBuf.length()>40) inBuf="";
    }
  }
}

void loop() {
  unsigned long now=millis();
  readSerial();

  speed_kmh = speed_target;
  float realSpeed = speed_kmh / displayCorr;
  setSpeed((uint16_t)realSpeed);

  if (now-lastSend>=20){
    float dt=(now-lastSend)/1000.0; lastSend=now;
    float meters=(realSpeed/3.6)*dt;
    distRemainder += meters*50.0;
    while(distRemainder>=1.0){ distCounter++; distRemainder-=1.0; if(distCounter>=30000)distCounter=0; }
    setDist(distCounter);
    fuelCounter+=2; setFcBytes(fuelCounter);
    canSend(0x5A0, spd_pkt);
    canSend(0x280, rpm_pkt);
    canSend(0x480, fcs_pkt);
    canSend(0x1A0, abs_pkt);
  }

  if (now-lastSlow>=100){ lastSlow=now;
    canSend(0x050, abg_pkt);
    canSend(0x470, ind_pkt);
    canSend(0x588, otp_pkt);
    canSend(0x288, ctp_pkt);
    canSend(0x58C, ssm_pkt);
  }
}