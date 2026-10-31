#include <Wire.h>
#include <math.h>

// SwapSched: 4-bay standalone demonstrator.
// LEDs indicate charging; relay contacts stay disconnected.
// Simulated SoC and vehicle arrivals are NOT physical measurements.

const byte RELAY[4] = {2, 4, 7, 8};
const byte LED[4]   = {3, 5, 6, 9};
const byte BUZZER = 12;
const byte DHT = 11;
const byte RTC_ADDR = 0x68;

const float PACK_KWH = 1.5;
const float CHARGER_KW = 0.75;
const float POWER_CAP_KW = 1.50;   // four-bay demo: max two chargers
const float READY_SOC = 0.95;
const float EPSILON = 0.05;
const byte LEAD_STEPS = 8;
const byte LOOKAHEAD_STEPS = 16;
const unsigned long STEP_MS = 15000UL; // accelerated: 15 sec = 15 simulated minutes

float soc[4] = {1.0, 1.0, 1.0, 1.0};
bool charging[4] = {false, false, false, false};
byte policy = 3; // P3 default; Serial Monitor: send 2 or 3
byte recent[8] = {0};
byte histCount = 0, histPos = 0;
byte arrivalsNow = 0, servedNow = 0, stockoutsNow = 0;
unsigned long stepNo = 0, lastStep = 0, rng = 0xA341316CUL;
byte tempC = 0, humidity = 0;
bool rtcOK = false, lcdOK = false, dhtOK = false;
byte lcdAddr = 0;

// ---------- LCD I2C backpack (common PCF8574 mapping) ----------
void expanderWrite(byte x) {
  Wire.beginTransmission(lcdAddr); Wire.write(x | 0x08); Wire.endTransmission();
}
void pulseNibble(byte x) {
  expanderWrite(x | 0x04); delayMicroseconds(1);
  expanderWrite(x & ~0x04); delayMicroseconds(50);
}
void lcdSend(byte value, byte mode) {
  pulseNibble((value & 0xF0) | mode);
  pulseNibble(((value << 4) & 0xF0) | mode);
}
void lcdCmd(byte c) { lcdSend(c, 0); }
void lcdChar(byte c) { lcdSend(c, 1); }
void lcdInit() {
  delay(50); pulseNibble(0x30); delay(5); pulseNibble(0x30);
  delayMicroseconds(150); pulseNibble(0x30); pulseNibble(0x20);
  lcdCmd(0x28); lcdCmd(0x0C); lcdCmd(0x06); lcdCmd(0x01); delay(2);
}
void lcdText(byte col, byte row, const char *s) {
  lcdCmd((row ? 0xC0 : 0x80) + col);
  while (*s) lcdChar(*s++);
}
void lcdClear() { lcdCmd(0x01); delay(2); }

// ---------- RTC DS1307 ----------
byte bcd(byte v) { return (v / 16) * 10 + (v % 16); }
bool getRTC(byte &h, byte &m, byte &s) {
  Wire.beginTransmission(RTC_ADDR); Wire.write((byte)0);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(RTC_ADDR, (byte)7) != 7) return false;
  s = bcd(Wire.read() & 0x7F);
  m = bcd(Wire.read() & 0x7F);
  h = bcd(Wire.read() & 0x3F);
  for (byte i=0; i<4; i++) Wire.read();
  return true;
}

// ---------- DHT11 3-pin module, no external library ----------
bool readDHT(byte &hum, byte &tc) {
  byte d[5] = {0,0,0,0,0};
  pinMode(DHT, OUTPUT); digitalWrite(DHT, LOW); delay(20);
  digitalWrite(DHT, HIGH); delayMicroseconds(30); pinMode(DHT, INPUT_PULLUP);
  if (!pulseIn(DHT, LOW, 120)) return false;
  if (!pulseIn(DHT, HIGH, 120)) return false;
  for (byte i=0; i<40; i++) {
    if (!pulseIn(DHT, LOW, 120)) return false;
    unsigned long t = pulseIn(DHT, HIGH, 120);
    if (!t) return false;
    d[i/8] <<= 1;
    if (t > 40) d[i/8] |= 1;
  }
  if ((byte)(d[0]+d[1]+d[2]+d[3]) != d[4]) return false;
  hum=d[0]; tc=d[2]; return true;
}

// ---------- Deterministic random generator ----------
unsigned long random32() {
  rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng;
}
float rand01() { return (random32() % 100000UL) / 100000.0; }
byte poisson(float lambda) {
  float limit=exp(-lambda), product=1.0; byte k=0;
  do { ++k; product *= (rand01()+0.00001); } while (product>limit && k<20);
  return k-1;
}
float demandLambda(float hour) {
  float morning=1.2*exp(-((hour-9.0)*(hour-9.0))/2.0);
  float evening=2.2*exp(-((hour-18.5)*(hour-18.5))/(2.0*1.2*1.2));
  return 0.4+morning+evening;
}
float tariff(float hour) {
  if (hour>=22.0 || hour<6.0) return 5.0;
  if (hour<18.0) return 7.0;
  return 10.0;
}
byte poissonQuantile(float probability, float mean) {
  float p=exp(-mean), cdf=p; byte k=0;
  while (cdf<probability && k<20) { ++k; p*=mean/k; cdf+=p; }
  return k;
}
byte readyCount() {
  byte n=0; for(byte i=0;i<4;i++) if(soc[i]>=READY_SOC) n++; return n;
}
byte stepsToReady(byte i) {
  float missing=(READY_SOC-soc[i])*PACK_KWH;
  if(missing<=0) return 0;
  return (byte)ceil(missing/(CHARGER_KW*0.25));
}
float forecastMean(float hour) {
  float multiplier=1.0, actual=0, expected=0;
  for(byte i=0;i<histCount;i++) {
    byte idx=(histPos+8-histCount+i)%8;
    actual += recent[idx];
    // Approximate nominal historical expectation over recent 15-min intervals.
    float h = hour - (histCount-i)*0.25;
    while(h<0) h+=24;
    expected += demandLambda(h);
  }
  if(histCount) {
    multiplier=(actual+2.0)/(expected+2.0);
    if(multiplier<0.5) multiplier=0.5;
    if(multiplier>2.0) multiplier=2.0;
  }
  float mu=0;
  for(byte j=0;j<LEAD_STEPS;j++) {
    float h=hour+j*0.25; while(h>=24) h-=24;
    mu += demandLambda(h)*multiplier;
  }
  return mu;
}

// P2/P3 safety-stock policy. P3 is the chance-constrained default.
void schedule(float hour) {
  bool chosen[4]={false,false,false,false};
  byte candidates[4], n=0;
  byte full=readyCount();
  float mu=forecastMean(hour);
  byte target=(policy==2) ? (byte)round(mu) : poissonQuantile(1.0-EPSILON,mu);
  if(target>4) target=4;
  int needed=(int)target-(int)full; if(needed<0) needed=0;
  for(byte i=0;i<4;i++) if(soc[i]<READY_SOC) candidates[n++]=i;

  for(byte i=0;i<n;i++) for(byte j=i+1;j<n;j++) {
    if(stepsToReady(candidates[j])<stepsToReady(candidates[i])) {
      byte t=candidates[i]; candidates[i]=candidates[j]; candidates[j]=t;
    }
  }
  byte maxOn=(byte)(POWER_CAP_KW/CHARGER_KW);
  byte on=0, must=(needed<n)?needed:n;
  for(byte i=0;i<must && on<maxOn;i++) { chosen[candidates[i]]=true; on++; }

  float nowPrice=tariff(hour), cheapestFuture=10000;
  for(byte j=1;j<LOOKAHEAD_STEPS;j++) {
    float h=hour+j*0.25; while(h>=24) h-=24;
    float p=tariff(h); if(p<cheapestFuture) cheapestFuture=p;
  }
  if(nowPrice<=cheapestFuture) {
    for(byte i=must;i<n && on<maxOn;i++) { chosen[candidates[i]]=true; on++; }
  }
  for(byte i=0;i<4;i++) charging[i]=chosen[i];
}

void applyOutputs() {
  for(byte i=0;i<4;i++) {
    digitalWrite(LED[i], charging[i] ? HIGH : LOW);
    digitalWrite(RELAY[i], charging[i] ? LOW : HIGH); // active-LOW relay
  }
}

void stepSimulation() {
  float hour=(stepNo%96)*0.25;
  schedule(hour);
  arrivalsNow=poisson(demandLambda(hour));
  recent[histPos]=arrivalsNow; histPos=(histPos+1)%8;
  if(histCount<8) histCount++;

  servedNow=0; stockoutsNow=0;
  bool swapped[4]={false,false,false,false};
  for(byte k=0;k<arrivalsNow;k++) {
    int b=-1;
    for(byte i=0;i<4;i++) if(soc[i]>=READY_SOC && !swapped[i]) { b=i; break; }
    if(b<0) stockoutsNow++;
    else {
      soc[b]=0.10+(rand01()*0.20); // returned vehicle battery SoC, simulated
      swapped[b]=true; charging[b]=false; servedNow++;
    }
  }
  for(byte i=0;i<4;i++) {
    if(charging[i] && !swapped[i] && soc[i]<1.0) {
      soc[i]+=CHARGER_KW*0.25/PACK_KWH;
      if(soc[i]>1.0) soc[i]=1.0;
    }
  }
  applyOutputs();
  stepNo++;
}

void updateDisplaysAndSerial() {
  byte hh=0,mm=0,ss=0;
  rtcOK=getRTC(hh,mm,ss);
  dhtOK=readDHT(humidity,tempC);

  char l1[17], l2[17];
  if(rtcOK) snprintf(l1,sizeof(l1),"%02u:%02u:%02u P%u",hh,mm,ss,policy);
  else snprintf(l1,sizeof(l1),"RTC not found");
  snprintf(l2,sizeof(l2),"Ready:%u Wait:%u",readyCount(),(byte)(4-readyCount()));
  if(lcdOK) { lcdClear(); lcdText(0,0,l1); lcdText(0,1,l2); }

  // CSV protocol consumed by the laptop dashboard:
  // DATA,step,policy,clock,arrivals,served,stockouts,ready,tariff,temp,humidity,soc1..soc4,mask
  char clockText[9];
  if(rtcOK) snprintf(clockText,sizeof(clockText),"%02u:%02u:%02u",hh,mm,ss);
  else strcpy(clockText,"--:--:--");
  byte mask=0; for(byte i=0;i<4;i++) if(charging[i]) mask |= (1<<i);
  Serial.print("DATA,"); Serial.print(stepNo); Serial.print(",");
  Serial.print(policy); Serial.print(","); Serial.print(clockText); Serial.print(",");
  Serial.print(arrivalsNow); Serial.print(","); Serial.print(servedNow); Serial.print(",");
  Serial.print(stockoutsNow); Serial.print(","); Serial.print(readyCount()); Serial.print(",");
  Serial.print(tariff((stepNo%96)*0.25),1); Serial.print(",");
  if(dhtOK) Serial.print(tempC); else Serial.print(-1);
  Serial.print(","); if(dhtOK) Serial.print(humidity); else Serial.print(-1);
  for(byte i=0;i<4;i++) { Serial.print(","); Serial.print((int)(soc[i]*100)); }
  Serial.print(","); Serial.println(mask);
}

void setup() {
  Serial.begin(9600); Wire.begin();
  for(byte i=0;i<4;i++) {
    pinMode(RELAY[i],OUTPUT); digitalWrite(RELAY[i],HIGH); // safe OFF
    pinMode(LED[i],OUTPUT); digitalWrite(LED[i],LOW);
  }
  pinMode(BUZZER,OUTPUT); digitalWrite(BUZZER,LOW);

  Wire.beginTransmission(RTC_ADDR); rtcOK=(Wire.endTransmission()==0);
  byte addr[2]={0x27,0x3F};
  for(byte i=0;i<2;i++) {
    Wire.beginTransmission(addr[i]);
    if(Wire.endTransmission()==0) { lcdAddr=addr[i]; lcdOK=true; break; }
  }
  if(lcdOK) { lcdInit(); lcdText(0,0,"SwapSched"); lcdText(0,1,"4 Bay Starting"); }

  digitalWrite(BUZZER,HIGH); delay(150); digitalWrite(BUZZER,LOW);
  rng ^= micros();
  lastStep=millis();
  Serial.println("SwapSched dashboard protocol ready");
  Serial.println("Send 2 for P2 or 3 for P3");
}

void loop() {
  while(Serial.available()) {
    char c=Serial.read();
    if(c=='2' || c=='3') {
      policy=c-'0';
      digitalWrite(BUZZER,HIGH); delay(60); digitalWrite(BUZZER,LOW);
    }
  }
  if(millis()-lastStep>=STEP_MS) {
    lastStep += STEP_MS;
    stepSimulation();
    updateDisplaysAndSerial();
    if(stockoutsNow>0) { digitalWrite(BUZZER,HIGH); delay(80); digitalWrite(BUZZER,LOW); }
  }
}
