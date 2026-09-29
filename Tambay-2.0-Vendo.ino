#include <WiFi.h>

#include <WebServer.h>

#include <EEPROM.h>

#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 20, 4);

#define RELAY_WASH_FWD 23

#define RELAY_WASH_REV 19

#define RELAY_INLET 18

#define RELAY_DRAIN 5

#define RELAY_SPIN 13

#define PIN_PRESSURE 34

#define PIN_COIN 4

#define PIN_BTN_UP 15

#define PIN_BTN_DOWN 2

#define PIN_BTN_SELECT 25

#define PIN_DOOR_TUB1 16

#define PIN_DOOR_TUB2 17

#define PIN_CURRENT 26

#define PIN_BUZZER 27

WebServer server(80);

String USERNAME="admin", PASSWORD="1234";

bool isLoggedIn=false;

struct Settings{

int washFwd=6,washRev=6,washDelay=3,washMinDefault=15,washPricePerMin=1;

int rinseFwd=4,rinseRev=4,rinseDelay=3,rinseMinDefault=3,rinseCycleDefault=2,rinsePricePerMin=1;

int spinMinDefault=5,spinPricePerMin=1;

int drainFinal=30,drainManual=30,drainMin=10,drainMax=120;

int autoWash=20,autoRinseMin=3,autoRinseCycle=2;

int startWash=1,startRinse=1,startSpin=1;

int waterLow=400,waterHi=800;

int washOptions[4]={10,15,20,25};

int rinseMinOptions[3]={3,5,10};

int rinseCycleOptions[3]={2,3,4};

int spinOptions[3]={3,5,10};

};

Settings set;

volatile int coinCount=0; int pondo=0;

enum State{TAMBAYAN,MENU_AUTO,MENU_MANUAL,MENU_DRAIN,WASH_RUN,RINSE_RUN,SPIN_RUN,DRAIN_RUN,SELECT_WASH,SELECT_RINSE,SELECT_SPIN};

State state=TAMBAYAN;

bool tub1Busy=false,tub2Busy=false;

unsigned long endTime=0,drainEndTime=0;

int sel=0,manualSel=0; bool rinseQueued=false;

int queuedRinseMin=0,queuedRinseCycle=0,currentRinseMin=3,currentRinseCycle=2;

void IRAM_ATTR coinISR(){coinCount++;}

bool btnUp(){return digitalRead(PIN_BTN_UP)==LOW;}

bool btnDown(){return digitalRead(PIN_BTN_DOWN)==LOW;}

bool btnSelect(){return digitalRead(PIN_BTN_SELECT)==LOW;}

bool btnDownLong(){if(digitalRead(PIN_BTN_DOWN)==LOW){delay(800); if(digitalRead(PIN_BTN_DOWN)==LOW) return true;} return false;}

void buzz(int ms){digitalWrite(PIN_BUZZER,HIGH); delay(ms); digitalWrite(PIN_BUZZER,LOW);}

void handleCoinPondo(){if(coinCount>0){pondo+=coinCount; coinCount=0; buzz(100);}}

void handleDoorLogic(){bool o=digitalRead(PIN_DOOR_TUB1)==HIGH; if(tub1Busy&&o){if(millis()%1000<500) digitalWrite(PIN_BUZZER,HIGH); else digitalWrite(PIN_BUZZER,LOW);} else digitalWrite(PIN_BUZZER,LOW); if(tub1Busy&&o){digitalWrite(RELAY_WASH_FWD,HIGH); digitalWrite(RELAY_WASH_REV,HIGH);}}

bool checkCurrentLimiter(){if(digitalRead(PIN_CURRENT)==LOW){digitalWrite(RELAY_WASH_FWD,HIGH); digitalWrite(RELAY_WASH_REV,HIGH); digitalWrite(RELAY_SPIN,HIGH); lcd.clear(); lcd.setCursor(0,0); lcd.print("OVER CURRENT!"); for(int i=5;i>0;i--){lcd.setCursor(0,2); lcd.print("Resume "+String(i)+"s "); delay(1000);} lcd.clear(); return true;} return false;}

void saveSettings(){EEPROM.put(0,set); EEPROM.put(300,USERNAME); EEPROM.put(350,PASSWORD); EEPROM.commit();}

void loadSettings(){EEPROM.get(0,set); String u,p; EEPROM.get(300,u); EEPROM.get(350,p); if(u.length()>2) USERNAME=u; if(p.length()>2) PASSWORD=p; if(set.drainFinal<10||set.drainFinal>120) set.drainFinal=30; if(set.drainManual<10||set.drainManual>120) set.drainManual=30;}


void setup(){

Serial.begin(115200); EEPROM.begin(1024); loadSettings();

pinMode(RELAY_WASH_FWD,OUTPUT); pinMode(RELAY_WASH_REV,OUTPUT); pinMode(RELAY_INLET,OUTPUT); pinMode(RELAY_DRAIN,OUTPUT); pinMode(RELAY_SPIN,OUTPUT);

pinMode(PIN_BTN_UP,INPUT_PULLUP); pinMode(PIN_BTN_DOWN,INPUT_PULLUP); pinMode(PIN_BTN_SELECT,INPUT_PULLUP);

pinMode(PIN_DOOR_TUB1,INPUT_PULLUP); pinMode(PIN_DOOR_TUB2,INPUT_PULLUP); pinMode(PIN_CURRENT,INPUT_PULLUP); pinMode(PIN_COIN,INPUT_PULLUP); pinMode(PIN_BUZZER,OUTPUT);

digitalWrite(RELAY_WASH_FWD,HIGH); digitalWrite(RELAY_WASH_REV,HIGH); digitalWrite(RELAY_INLET,HIGH); digitalWrite(RELAY_DRAIN,HIGH); digitalWrite(RELAY_SPIN,HIGH); digitalWrite(PIN_BUZZER,LOW);

lcd.init(); lcd.backlight(); lcd.clear(); lcd.setCursor(0,0); lcd.print(" Tambay 2.0"); lcd.setCursor(0,1); lcd.print(" Slyhitchiker"); lcd.setCursor(0,2); lcd.print(" 09276270488"); lcd.setCursor(0,3); lcd.print(" Tambayan Mode..."); delay(3000);

WiFi.softAP("VENDO-WASH-V2","12345678"); server.on("/",handleRoot); server.on("/login",handleLogin); server.on("/settings",handleSettings); server.on("/save",handleSave); server.on("/calibrate",handleCalibrate); server.on("/drainOn",handleDrainOn); server.on("/drainOff",handleDrainOff); server.begin();

attachInterrupt(digitalPinToInterrupt(PIN_COIN),coinISR,FALLING);

}. void loop(){

server.handleClient();

if(checkCurrentLimiter()) return;

handleDoorLogic();

handleCoinPondo();


if(state==TAMBAYAN){

lcd.setCursor(0,0); lcd.print("Tambayan P:"+String(pondo)+" ");

lcd.setCursor(0,1); if(sel==0) lcd.print(">Auto "); else if(sel==1) lcd.print(">Manual "); else lcd.print(">Drain ");

lcd.setCursor(0,2); lcd.print("UP/DN SEL "); lcd.setCursor(0,3); lcd.print("Insert Coin ");

if(btnUp()){sel++; if(sel>2) sel=0; delay(150);}

if(btnDown()){sel--; if(sel<0) sel=2; delay(150);}

if(btnSelect()){

if(sel==0){if(tub1Busy){lcd.clear(); lcd.print("TUB1 BUSY!"); buzz(1000); delay(1000);} else{state=MENU_AUTO; lcd.clear();}}

if(sel==1){state=MENU_MANUAL; sel=0; lcd.clear();}

if(sel==2){if(tub1Busy){lcd.clear(); lcd.print("Drain CANCELED BUSY!"); buzz(1000); delay(1500);} else{if(digitalRead(PIN_DOOR_TUB1)==HIGH){state=MENU_DRAIN; lcd.clear();} else{lcd.clear(); lcd.print("Open Door First"); buzz(500); delay(1000);}}}

delay(300);

}

}


if(state==MENU_AUTO){

int total=set.autoWash+(set.autoRinseMin*set.autoRinseCycle);

int price=(set.autoWash*set.washPricePerMin)+(set.autoRinseMin*set.autoRinseCycle*set.rinsePricePerMin);

lcd.setCursor(0,0); lcd.print("Auto:"+String(set.autoWash)+"+"+String(set.autoRinseMin)+"x"+String(set.autoRinseCycle)+" ");

lcd.setCursor(0,1); lcd.print("Tot:"+String(total)+"m P"+String(price)+" ");

lcd.setCursor(0,2); lcd.print("Pondo:"+String(pondo)+" Need:"+String(price-pondo)+" ");

if(btnUp()){state=TAMBAYAN; sel=0; lcd.clear(); delay(200);}

if(btnSelect()){if(pondo>=price){pondo-=price; startWash(set.autoWash,true,set.autoRinseMin,set.autoRinseCycle);} else buzz(500);}

}


if(state==MENU_MANUAL){

lcd.setCursor(0,0); lcd.print("Manual Select:");

lcd.setCursor(0,1); if(sel==0) lcd.print(">Wash 10/15/20/25 "); else if(sel==1) lcd.print(">Rinse 3/5/10 x2/3/4"); else lcd.print(">Spin 3/5/10 ");

lcd.setCursor(0,2); lcd.print("Pondo:"+String(pondo));

if(btnUp()){sel++; if(sel>2) sel=0; delay(150);}

if(btnDown()){if(sel==0){state=TAMBAYAN; lcd.clear();} else sel--; delay(150);}

if(btnSelect()){if(sel==0){state=SELECT_WASH; manualSel=0; lcd.clear();} if(sel==1){state=SELECT_RINSE; manualSel=0; lcd.clear();} if(sel==2){state=SELECT_SPIN; manualSel=0; lcd.clear();} delay(300);}

}


if(state==SELECT_WASH){

int m=set.washOptions[manualSel]; int price=m*set.washPricePerMin;

lcd.setCursor(0,0); lcd.print("Wash Select:"); lcd.setCursor(0,1); lcd.print(">"+String(m)+" min P"+String(price)+" ");

if(btnUp()){manualSel++; if(manualSel>3) manualSel=0; delay(150);}

if(btnDown()){manualSel--; if(manualSel<0) manualSel=3; delay(150);}

if(btnSelect()){if(!tub1Busy&&pondo>=price){pondo-=price; startWash(m,false,0,0);} else buzz(500); delay(300);}

}. if(state==SELECT_RINSE){

int min=set.rinseMinOptions[manualSel%3]; int cyc=set.rinseCycleOptions[manualSel/3%3]; if(manualSel>8) manualSel=0;

int price=min*cyc*set.rinsePricePerMin;

lcd.setCursor(0,0); lcd.print("Rinse "+String(min)+"m x"+String(cyc)+" P"+String(price)+" ");

if(btnUp()){manualSel++; delay(150);}

if(btnDown()){manualSel--; if(manualSel<0) manualSel=8; delay(150);}

if(btnSelect()){

if(tub1Busy){long rem=(endTime-millis())/60000; if(rem<=1) buzz(1000); else if(pondo>=price){pondo-=price; rinseQueued=true; queuedRinseMin=min; queuedRinseCycle=cyc; buzz(200); state=WASH_RUN;}}

else if(pondo>=price){pondo-=price; startRinse(min,cyc);}

}

}

if(state==SELECT_SPIN){

int m=set.spinOptions[manualSel]; int price=m*set.spinPricePerMin;

lcd.setCursor(0,0); lcd.print("Spin "+String(m)+"m P"+String(price)+" ");

if(btnUp()){manualSel++; if(manualSel>2) manualSel=0; delay(150);}

if(btnDown()){manualSel--; if(manualSel<0) manualSel=2; delay(150);}

if(btnSelect()){if(!tub1Busy&&!tub2Busy&&pondo>=price){pondo-=price; startSpin(m);} else buzz(500); delay(300);}

}

if(state==MENU_DRAIN){

lcd.setCursor(0,0); lcd.print("Manual Drain TEST");

lcd.setCursor(0,1); lcd.print("Time:"+String(set.drainManual)+"s DEF30");

if(btnSelect()){if(tub1Busy){buzz(1000); state=TAMBAYAN;} else startManualDrain(); delay(300);}

}

if(state==DRAIN_RUN){

long rem=(drainEndTime-millis())/1000; if(rem<0) rem=0;

lcd.setCursor(0,0); lcd.print("DRAINING Rem:"+String(rem)+"s ");

if(millis()>=drainEndTime){digitalWrite(RELAY_DRAIN,HIGH); state=TAMBAYAN; buzz(500); lcd.clear();}

}

if(state==WASH_RUN){

washMotorLoop();

lcd.setCursor(0,0); lcd.print("Wash "+String((endTime-millis())/60000)+"m Rmn ");

if(millis()>=endTime){drainPhase(); if(rinseQueued){int rm=queuedRinseMin; int rc=queuedRinseCycle; rinseQueued=false; startRinse(rm,rc);} else{tub1Busy=false; state=TAMBAYAN; buzz(3000);}}

}

if(state==RINSE_RUN){

rinseMotorLoop();

lcd.setCursor(0,0); lcd.print("Rinse Rem:"+String((endTime-millis())/60000)+"m ");

if(millis()>=endTime){drainPhase(); tub1Busy=false; state=TAMBAYAN; buzz(3000);}

}

if(state==SPIN_RUN){

lcd.setCursor(0,0); lcd.print("Spin Rem:"+String((endTime-millis())/60000)+"m ");

if(millis()>=endTime){digitalWrite(RELAY_SPIN,HIGH); tub2Busy=false; state=TAMBAYAN; buzz(3000);}

}

} // END LOOP  void startWash(int mins,bool isAuto,int rMin,int rCyc){

tub1Busy=true; state=WASH_RUN; lcd.clear(); lcd.print("Inlet Wash...");

digitalWrite(RELAY_INLET,LOW); while(analogRead(PIN_PRESSURE)<set.waterHi){delay(200);}

digitalWrite(RELAY_INLET,HIGH); endTime=millis()+mins*60000L;

if(isAuto){queuedRinseMin=rMin; queuedRinseCycle=rCyc; rinseQueued=true;}

}

void startRinse(int mins,int cyc){

tub1Busy=true; state=RINSE_RUN; currentRinseMin=mins; currentRinseCycle=cyc;

lcd.clear(); lcd.print("Inlet Rinse...");

digitalWrite(RELAY_INLET,LOW); while(analogRead(PIN_PRESSURE)<set.waterHi){delay(200);}

digitalWrite(RELAY_INLET,HIGH); endTime=millis()+mins*60000L;

}

void startSpin(int mins){tub2Busy=true; state=SPIN_RUN; digitalWrite(RELAY_SPIN,LOW); endTime=millis()+mins*60000L;}

void startManualDrain(){state=DRAIN_RUN; digitalWrite(RELAY_DRAIN,LOW); drainEndTime=millis()+set.drainManual*1000L; buzz(200);}

void drainPhase(){lcd.clear(); lcd.print("Final Drain 30s"); digitalWrite(RELAY_DRAIN,LOW); delay(set.drainFinal*1000); while(analogRead(PIN_PRESSURE)>set.waterLow){delay(300);} digitalWrite(RELAY_DRAIN,HIGH);}


void washMotorLoop(){

static unsigned long t=0; static int p=0;

if(digitalRead(PIN_DOOR_TUB1)==HIGH) return;

if(millis()-t>(p==1?set.washDelay*1000:0)){

if(p==0){digitalWrite(RELAY_WASH_FWD,LOW); t=millis(); p=1;}

else if(p==1&&millis()-t>=set.washFwd*1000){digitalWrite(RELAY_WASH_FWD,HIGH); t=millis(); p=2;}

else if(p==2&&millis()-t>=set.washDelay*1000){digitalWrite(RELAY_WASH_REV,LOW); t=millis(); p=3;}

else if(p==3&&millis()-t>=set.washRev*1000){digitalWrite(RELAY_WASH_REV,HIGH); t=millis(); p=0;}

}

}

void rinseMotorLoop(){

static unsigned long t=0; static int p=0;

if(digitalRead(PIN_DOOR_TUB1)==HIGH) return;

if(millis()-t>(p==1?set.rinseDelay*1000:0)){

if(p==0){digitalWrite(RELAY_WASH_FWD,LOW); t=millis(); p=1;}

else if(p==1&&millis()-t>=set.rinseFwd*1000){digitalWrite(RELAY_WASH_FWD,HIGH); t=millis(); p=2;}

else if(p==2&&millis()-t>=set.rinseDelay*1000){digitalWrite(RELAY_WASH_REV,LOW); t=millis(); p=3;}

else if(p==3&&millis()-t>=set.rinseRev*1000){digitalWrite(RELAY_WASH_REV,HIGH); t=millis(); p=0;}

}

}


void handleRoot(){server.send(200,"text/html","<h2>TAMBAY 2.0 LOGIN</h2><form action='/login' method='POST'>User:<input name='user'><br>Pass:<input name='pass' type='password'><br><input type='submit'></form>");}

void handleLogin(){if(server.arg("user")==USERNAME&&server.arg("pass")==PASSWORD){isLoggedIn=true; server.sendHeader("Location","/settings"); server.send(303);} else server.send(200,"text/html","Wrong! <a href='/'>Back</a>");}

void handleSettings(){

if(!isLoggedIn){server.sendHeader("Location","/"); server.send(303); return;}

String h="<html><body><h2>Tambay 2.0 SETTINGS V2</h2><form action='/save' method='POST'>";

h+="<h3>Wash</h3>Fwd:<input name='wfwd' value='"+String(set.washFwd)+"'> Rev:<input name='wrev' value='"+String(set.washRev)+"'> Dly:<input name='wd' value='"+String(set.washDelay)+"'><br>Def:<input name='wdef' value='"+String(set.washMinDefault)+"'> P/Min:<input name='wp' value='"+String(set.washPricePerMin)+"'><br>";

h+="<h3>Rinse</h3>Fwd:<input name='rfwd' value='"+String(set.rinseFwd)+"'> Rev:<input name='rrev' value='"+String(set.rinseRev)+"'> Dly:<input name='rd' value='"+String(set.rinseDelay)+"'><br>DefMin:<input name='rdef' value='"+String(set.rinseMinDefault)+"'> Cyc:<input name='rcyc' value='"+String(set.rinseCycleDefault)+"'> P/Min:<input name='rp' value='"+String(set.rinsePricePerMin)+"'><br>";

h+="<h3>Spin</h3>Def:<input name='sdef' value='"+String(set.spinMinDefault)+"'> P/Min:<input name='sp' value='"+String(set.spinPricePerMin)+"'><br>";

h+="<h3>Auto</h3>Wash:<input name='aw' value='"+String(set.autoWash)+"'> RMin:<input name='arm' value='"+String(set.autoRinseMin)+"'> RCyc:<input name='arc' value='"+String(set.autoRinseCycle)+"'><br>";

h+="<h3>Drain</h3>Final:<input name='df' value='"+String(set.drainFinal)+"'> Manual:<input name='dm' value='"+String(set.drainManual)+"'><br>User:<input name='newuser' value='"+USERNAME+"'> Pass:<input name='newpass' value='"+PASSWORD+"'><br>Low:<input name='wl' value='"+String(set.waterLow)+"'> Hi:<input name='wh' value='"+String(set.waterHi)+"'><br><input type='submit' value='SAVE'></form></body></html>";

server.send(200,"text/html",h);

}

void handleSave(){

set.washFwd=server.arg("wfwd").toInt(); set.washRev=server.arg("wrev").toInt(); set.washDelay=server.arg("wd").toInt(); set.washMinDefault=server.arg("wdef").toInt(); set.washPricePerMin=server.arg("wp").toInt();

set.rinseFwd=server.arg("rfwd").toInt(); set.rinseRev=server.arg("rrev").toInt(); set.rinseDelay=server.arg("rd").toInt(); set.rinseMinDefault=server.arg("rdef").toInt(); set.rinseCycleDefault=server.arg("rcyc").toInt(); set.rinsePricePerMin=server.arg("rp").toInt();

set.spinMinDefault=server.arg("sdef").toInt(); set.spinPricePerMin=server.arg("sp").toInt();

set.autoWash=server.arg("aw").toInt(); set.autoRinseMin=server.arg("arm").toInt(); 

set.autoRinseCycle=server.arg("arc").toIn
