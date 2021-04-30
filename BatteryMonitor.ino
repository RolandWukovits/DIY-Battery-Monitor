
// This is working software for my DIY Battery Monitor with Version 3 AC-Powerwall board
// Software Version: 1.00, 19.04.2021
// Author and system designer: Roland Wukovits
// e-mail: acpw@thehillside.net
// The code, or parts of it, can be used for private DIY projects only, as
// some parts of the code (especially libraries and standard routines) 
// are originated from other authors
// Code for the whole monitor, is intellectual property of the 
// system designer. Please contact me for special approval if you are
// planning to use the code in a commecial application!
// If you need customized features, you can contact me for assistance,
// as well I can source components and build a monitor for you 
// If I was able to help you out with valueable information to 
// realize your project, please consider an adequate donation by using
// PayPal: sales@thehillside.net
// Each donation, even small ones, are very much appreciated. Thanks!


// I2C
#include <Wire.h>
// NewLiquidCrystal Library for I2C
#include <LiquidCrystal_I2C.h>
// ADS1115 Library
#include <Adafruit_ADS1015.h>
// ESP01 and Blynk for Mega
#include <ESP8266_Lib.h>
#include <BlynkSimpleShieldEsp8266.h>
// INA219 shunt sensor
#include <INA219_WE.h>
// EEPROM
#include <EEPROM.h>


#define BLYNK_PRINT Serial
char auth[] = "yourAuth";
char ssid[] = "yourSSID";
char pass[] = "yourPassword";

// or Software Serial on Uno, Nano...
//#include <SoftwareSerial.h>
#define EspSerial Serial2

// Your ESP8266 baud rate:
#define ESP8266_BAUD 115200

ESP8266 wifi(&EspSerial);

//INA219
#define I2C_ADDRESS 0x40
INA219_WE ina219(I2C_ADDRESS);

// Define LCD pinout
const int  en = 2, rw = 1, rs = 0, d4 = 4, d5 = 5, d6 = 6, d7 = 7, bl = 3;
 // Define I2C Address - change if reqiuired
const int i2c_addr = 0x27;
LiquidCrystal_I2C lcd(i2c_addr, en, rw, rs, d4, d5, d6, d7, bl, POSITIVE);
 

// Power variables
float battcapacity=8000;             //enter your battery capacity in Wh
float calibrationvalue=0.002383;     //calibrate your voltage readout with a volt-meter 
float voltread;
float battvoltage;
float averagevolt;
float holdvolt;
int voltcounter=1;
float SOCvoltage;
float shuntVolt;
int shuntMaxmV=75;                       // enter your Shunt mV rating
int shuntMaxAmps=100;                    // enter your Shunt current rating in A
float current;
float current1;
float current2;
float mincurrent;
float maxcurrent;
float averagecurrent;
float holdcurrent;
float lastcurrent; 
float lastAhigh;
float lastAlow;
byte battSOC;
float watts;
float Cwatts;
int wattscounter=0;
float averagewatts=0;
float chgwatthours=0;
unsigned long wattsstartmillis;
unsigned long wattsinterval=600000;     //set 600000 to log Wh once every 10 min, 300000 for every 5 min, 60000 for every min 

// ADS1115
Adafruit_ADS1115 ads;

// Blynk/Wifi
byte linkState=0;
byte connectCounter=0;
unsigned long sendstartmillis;
unsigned long sendinterval=3500;         // interval of sending Blynk data in mSec

// Ext WDT
#define ExtWDTpin 8
unsigned long ExtWDTstartmillis;
unsigned long ExtWDTinterval=2500;      // interval of WDT reset in loop  in mSec

unsigned long loopstartmillis;
unsigned long loopinterval=350;         // interval of loop in mSec             
unsigned long wifistartmillis;
unsigned long wifiinterval=360000;      // interval wifi reset when connection down  in mSec
int disconnects;

// Temp probes
#define Probe1 A11                   // enter your Probe1 Analog Pin
#define Probe2 A9                    // enter your Probe2 Analog Pin
int proberead1;
int proberead2;
float probetemp1;
float probetemp2;

byte subpage;
byte blockpage;
byte displaycounter;

#define Pageswitch 22                   // enter your Button Digital Pin

int cycles=0;
byte cycleslobyte;
byte cycleshibyte;
int extractor;

// EEPROM
byte EEPROMaddress=0;
byte EEPROMvalue[3];            // Array accomodating 3 stored values, increase if needed

// Moving Average
const int numReadings=5;          // how many values are part of MA
byte firstnbrs=0;
float Areadings[numReadings];      
int readIndex=0;              
float total;                  // the running total for MA calc
byte homing=1;           


void setup(){
  
//  Serial.begin(9600);
  Wire.begin();

  for (EEPROMaddress=0;EEPROMaddress<3;EEPROMaddress++) {
    EEPROMvalue[EEPROMaddress]=EEPROM.read(EEPROMaddress);
    delay(50);
  }
  //firstwrite EEPROM
  if (EEPROMvalue[0]==255 or EEPROMvalue[0]==0) {
     EEPROM.write(0,1);                  // "1" shows EEPROM have data
     EEPROM.write(1,62);                // flashing with initial cycles (574), set (1,0) for no initial cycles
     EEPROM.write(2,2);                 // set (2,0) for no initial cycles 
  }
  else {
     cycleslobyte= EEPROMvalue[1];
     cycleshibyte= EEPROMvalue[2];
  }

  cycles=cycleslobyte+(cycleshibyte*256);

  //INA219
  ina219.init();
  ina219.setADCMode(SAMPLE_MODE_32);  // choose how many samples to take each measurement (2,4,8,16,32,64,128)
  ina219.setMeasureMode(TRIGGERED);   // Triggered measurements
  ina219.setPGain(PG_80);             // choose gain according your shunt voltage (40,80,160,320) mV max
  //ina219.setCorrectionFactor(0.99); // insert your correction factor if necessary

  // Set display type as 16 char, 2 rows  or 20 char, 4 rows
  lcd.begin(16,2);
  // lcd.begin(20,4);  
  
  // LCD Print on first row
  lcd.setCursor(0,0);
  lcd.print("System start up ");
  lcd.setCursor(0,1);
  lcd.print("                ");


  //Ext WDT
  pinMode(ExtWDTpin,OUTPUT);

  //Temp probes
  pinMode(Probe1,INPUT);
  pinMode(Probe2,INPUT);

  pinMode(Pageswitch,INPUT);
  
  delay(1000);


// Ext WatchdogTimer reset
  simpleWDTreset();

 
  // ESP and Blynk 
  EspSerial.begin(ESP8266_BAUD);
  delay(300);
  lcd.setCursor(0,1);
  lcd.print("connecting WiFi ");
  // do NOT use Blynk.begin() to initialize! It is a blocking function. If it cannot connect to the server, it 
  // would hang the MC indefinetly. You must use the following format for Blynk!
  wifi.setDHCP(1, 1, 1); //Enable dhcp in station mode and save in flash of esp8266
  Blynk.config(wifi, auth, "blynk-cloud.com", 8080); 
      simpleWDTreset();
  if (Blynk.connectWiFi(ssid, pass)) {
        simpleWDTreset();
     lcd.setCursor(0,1);
     lcd.print("connecting Blynk");
     Blynk.connect();
        simpleWDTreset();
     if (Blynk.connected()){
        simpleWDTreset();   
     lcd.setCursor(0,1);
     lcd.print("  Blynk... OK   ");
     linkState=1;
     }
     else{
       simpleWDTreset();     
     lcd.setCursor(0,1);
     lcd.print(" Blynk failed!  ");
     connectCounter=1;
     wifistartmillis=millis();
     }
     
     }
     else {
     linkState = 0;
     connectCounter=1;
     }


  // Ext WatchdogTimer reset
    simpleWDTreset();  

  // ADS1115
   ads.setGain(GAIN_TWOTHIRDS);        // full range
   ads.begin();
  
  delay(2000);
  
  lcd.clear();

  simpleWDTreset();

  sendstartmillis=millis();
  wattsstartmillis=millis();
  loopstartmillis=millis();

  mincurrent=shuntMaxAmps;
  maxcurrent=shuntMaxAmps*-1;

  // Moving Average
  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    Areadings[thisReading] = 0;
  }


}

void loop(){

// Main WatchdogTimer reset
  resetExtWDT();

// general loop
 if (millis()>=(loopstartmillis+loopinterval)){
  loopstartmillis=millis();
  Mode1();
 }

// WIFI connection reset if disconnected
 if (connectCounter==1){
   if (millis()>=(wifistartmillis+wifiinterval)) {
    connectCounter=0;
   }
 }

// Blynk
 if (millis()>=(sendstartmillis+sendinterval)) {
  sendstartmillis=millis();
    
  if (connectCounter==0){
   if (!Blynk.connected()) {                               //If Blynk is not connected
    linkState = 0;
    connectCounter=1;
    simpleWDTreset();
    delay(2000);    
    startBlynk();
   }
   else{
     Blynk.run();
     simpleWDTreset();
   }
  } 
 
    
  if (linkState==1){  
     BlynkDataTransmit();
    }

 }


}
 
 
 void Mode1(){

 delay(30);    
 getCurrent();
 
 delay(20);     
 getVoltage();
 
 getSOC();

 if (subpage==0){
    getTempprobes();
 }   


  if (digitalRead(Pageswitch)==HIGH){
    if (blockpage==0){
      subpage=1;
      blockpage=1;
      displaycounter=0;
    }  
  }
  else{
    subpage=0; 
    blockpage=0; 
  }

 if (displaycounter==0){     //building up display only once every 7 loops to avoid flickering

  if (subpage==0){            // page 1
  lcd.setCursor(0,0);
  lcd.print("                ");
  lcd.setCursor(0,0); 
  lcd.print(battvoltage,2); 
  lcd.print("V"); 
  lcd.setCursor(8,0);
  lcd.print(battSOC); 
  lcd.print("%"); 
  lcd.setCursor(15,0);
  if (linkState==1){
  lcd.print("@"); 
  }
  lcd.setCursor(0,1);
  lcd.print("                ");
  lcd.setCursor(0,1);
  lcd.print(current,1); 
  lcd.print("A"); 
   if(homing==1){ 
    lcd.setCursor(7,1);
    lcd.print("><"); 
   } 
  lcd.setCursor(10,1);
  lcd.print(watts,0); 
  lcd.print("W");  
  }
   if (subpage==1){            // page 2
  lcd.setCursor(0,0);
  lcd.print("B-T:  C  ");  
  lcd.setCursor(4,0);
  lcd.print(probetemp1,0);
  lcd.setCursor(9,0);
  lcd.print("I-T:  C");  
  lcd.setCursor(13,0);
  lcd.print(probetemp2,0);
  lcd.setCursor(0,1);
  lcd.print("Cy:             ");  
  lcd.setCursor(3,1);
  lcd.print(cycles);
  lcd.setCursor(9,1);
 // lcd.print(millis()/10000);
  lcd.print(chgwatthours,0);
  lcd.print("WhC"); 
  }
  if (subpage==2){            // page 3
  lcd.setCursor(0,0);
  lcd.print("Millis:         ");  
  lcd.setCursor(8,0);
  lcd.print(millis()/10000);
  lcd.setCursor(0,1);
  lcd.print("Disconnects:    ");  
  lcd.setCursor(12,1);
  lcd.print(disconnects);
  }

 }

 displaycounter++;
 if(displaycounter==7){
  
  displaycounter=0;
  if (subpage==1){
    subpage=2;
  }
  else{
    subpage=1;
  }
  
 }

 }


void resetExtWDT() {

 if (millis()>=(ExtWDTstartmillis+ExtWDTinterval)) {
    digitalWrite(ExtWDTpin,HIGH);
    delay(50);
    digitalWrite(ExtWDTpin,LOW);
    ExtWDTstartmillis=millis();
 }

} 

void simpleWDTreset() {

    digitalWrite(ExtWDTpin,HIGH);
    delay(50);
    digitalWrite(ExtWDTpin,LOW);
    
}



void startBlynk() {

  if (linkState == 0) {  
    wifistartmillis=millis();  
    disconnects++;
    if (disconnects>9999){
      disconnects=9999;                             
    }
    wifi.setDHCP(1, 1, 1);       //Enable dhcp in station mode and save in flash of esp8266
    Blynk.config(wifi, auth, "blynk-cloud.com", 8080); 
            simpleWDTreset();
    if (Blynk.connectWiFi(ssid, pass)) {
            simpleWDTreset();
         Blynk.connect();
            simpleWDTreset();
    if (Blynk.connected()){
      linkState = 1;
      }
    }
         simpleWDTreset();
  }


}

void BlynkDataTransmit(){

 if (linkState==1){ 
  
 if (Blynk.connected()){
         simpleWDTreset();
  
 Blynk.virtualWrite(V4, millis() / 10000);
 Blynk.virtualWrite(V0, battvoltage);
 Blynk.virtualWrite(V1, battSOC);
 Blynk.virtualWrite(V2, round(probetemp1));
 Blynk.virtualWrite(V3, round(probetemp2));
 Blynk.virtualWrite(V5, current);
 Blynk.virtualWrite(V6, round(chgwatthours));
 Blynk.virtualWrite(V7, cycles);
 Blynk.virtualWrite(V8, round(watts));
 
}
 
}
 
}

void  getTempprobes(){

  proberead1=analogRead(Probe1);
  proberead2=analogRead(Probe2);

  probetemp1=((proberead1-155)*0.125);            // change values according your sensor
  probetemp2=((proberead2-155)*0.135);            // change values according your sensor
 
}


void getCurrent(){

  shuntVolt=0;
  
  ina219.startSingleMeasurement();                   // triggers single-shot measurement and waits until completed
  shuntVolt = ina219.getShuntVoltage_mV();

  holdcurrent=(shuntVolt/shuntMaxmV)*shuntMaxAmps; 
    if (holdcurrent<mincurrent){
      mincurrent=holdcurrent;
    }
    if (holdcurrent>maxcurrent){
      maxcurrent=holdcurrent;
    } 
    
   averagecurrent=averagecurrent+holdcurrent;           //smoothing out current readings

 if (voltcounter>=7) {

   // initial filtering of possible outliers
   current1=(averagecurrent-(mincurrent+maxcurrent))/5; 
   
   if ((mincurrent<(averagecurrent*0.92))and(maxcurrent>(averagecurrent*1.08))){
     current2=(((mincurrent+maxcurrent)/2)+current1)/2; 
   }
   if ((mincurrent<(averagecurrent*0.92))and(maxcurrent<(averagecurrent*1.08))){
     current2=(((((maxcurrent*2)+mincurrent)/3)+current1)/2);
   }
   if ((mincurrent>(averagecurrent*0.92))and(maxcurrent>(averagecurrent*1.08))){
     current2=(((((mincurrent*2)+maxcurrent)/3)+current1)/2);
   }

   // Moving Average
   total = total-Areadings[readIndex];
   Areadings[readIndex]=current2;
   total = total+Areadings[readIndex];
   readIndex++;

   if (readIndex>=numReadings){
     readIndex = 0;
     firstnbrs=1;
   }


   // calculate the current to be displayed
   if (firstnbrs==0){
    current=current2;
    lastcurrent=current;
   }
   else{
    current = total/numReadings;
    current=current*10;
    current=round(current);
    current=current/10;

   if (current<-3.5 or current>3.5){         // setting window for "Homing" indication
    lastAlow=lastcurrent*0.97;
    lastAhigh=lastcurrent*1.03;
   }
   else{                                     // small currents require looser tolerance
    lastAlow=lastcurrent*0.95;
    lastAhigh=lastcurrent*1.05;   
   }

     if(current<0){
      if ((current>(lastAlow)) or (current<(lastAhigh))){
        homing=1;
      }
      else{
        homing=0;
      } 
     }
     if(current>=0){
      if ((current<(lastAlow)) or (current>(lastAhigh))){
        homing=1;
      }
      else{
        homing=0;
      } 
     }
    lastcurrent=current;
     
  } 
     
   averagecurrent=0;
   mincurrent=shuntMaxAmps;
   maxcurrent=shuntMaxAmps*-1;
 }
 

}


void getVoltage(){

 voltread=ads.readADC_SingleEnded(0);  
 
 holdvolt=voltread*calibrationvalue;   //calibrate your readout with a volt meter!
 
 averagevolt=averagevolt+holdvolt;           //smoothing out voltage readings
 if (voltcounter>=7) {
   battvoltage=averagevolt/7;
   watts=battvoltage*current;
   averagevolt=0;
   voltcounter=0;
   
   getCycles();  
 }
 voltcounter++;
 
}


void getCycles(){

 wattscounter++;

 if (watts<0){
  Cwatts=watts*-1;
  averagewatts=averagewatts+Cwatts;
 }
  
 if (millis()>=(wattsstartmillis+wattsinterval)){
  wattsstartmillis=millis();
  averagewatts=(averagewatts/wattscounter)/(3600000/wattsinterval); 
  chgwatthours=chgwatthours+averagewatts;
  averagewatts=0;
  wattscounter=0;
 }

 if (chgwatthours>=battcapacity){
  cycles++;
  cycleshibyte=0;
  cycleslobyte=0;
  chgwatthours=chgwatthours-battcapacity;
  extractor=cycles;
    while (extractor>256){
      cycleshibyte++;
      extractor=extractor-256;
    }
    cycleslobyte=extractor;
    EEPROM.write(1,cycleslobyte);
    EEPROM.write(2,cycleshibyte);
 }
  
}


void getSOC() {                    //this curve is for a 10kWh LiPo pack

  SOCvoltage=battvoltage;
   
if (current<=0){                       //charging
  if (battvoltage>=54.85){
    SOCvoltage=SOCvoltage+(current*0.025);        // you might have to change current corrections according
   }                                              // your battery capacity
  
  if (battvoltage>=52.50 and battvoltage<54.85){
   SOCvoltage=SOCvoltage+(current*0.025);  
  }
  

  if (battvoltage<52.50){
    SOCvoltage=SOCvoltage+(current*0.025);  
  }
}
else{                                  //discharging
  if (battvoltage>=54.85){
    SOCvoltage=SOCvoltage+(current*0.023);  
   }
  
  if (battvoltage>=52.50 and battvoltage<54.85){
   SOCvoltage=SOCvoltage+(current*0.024);  
  }
  

  if (battvoltage<52.5){
    SOCvoltage=SOCvoltage+(current*0.025);  
  }
}
                
  
battSOC=100;

if (SOCvoltage<=58.74) {
battSOC=99;
}
if (SOCvoltage<=58.44) {
battSOC=98;
}
if (SOCvoltage<=58.14) {
battSOC=97;
}
if (SOCvoltage<=57.89) {
battSOC=96;
}
if (SOCvoltage<=57.64) {
battSOC=95;
}
if (SOCvoltage<=57.39) {
battSOC=94;
}
if (SOCvoltage<=57.17) {
battSOC=93;
}
if (SOCvoltage<=56.89) {
battSOC=92;
}
if (SOCvoltage<=56.66) {
battSOC=91;
}
if (SOCvoltage<=56.44) {
battSOC=90;
}
if (SOCvoltage<=56.26) {
battSOC=89;
}
if (SOCvoltage<=56.08) {
battSOC=88;
}
if (SOCvoltage<=55.90) {
battSOC=87;
}
if (SOCvoltage<=55.72) {
battSOC=86;
}
if (SOCvoltage<=55.54) {
battSOC=85;
}
if (SOCvoltage<=55.36) {
battSOC=84;
}
if (SOCvoltage<=55.18) {
battSOC=83;
}
if (SOCvoltage<=55.00) {
battSOC=82;
}
if (SOCvoltage<=54.82) {
battSOC=81;
}
if (SOCvoltage<=54.64) {
battSOC=80;
}
if (SOCvoltage<=54.44) {
battSOC=79;
}
if (SOCvoltage<=54.24) {
battSOC=78;
}
if (SOCvoltage<=54.06) {
battSOC=77;
}
if (SOCvoltage<=54.91) {
battSOC=76;
}
if (SOCvoltage<=53.74) {
battSOC=75;
}
if (SOCvoltage<=53.54) {
battSOC=74;
}
if (SOCvoltage<=53.36) {
battSOC=73;
}
if (SOCvoltage<=53.20) {
battSOC=72;
}
if (SOCvoltage<=53.02) {
battSOC=71;
}
if (SOCvoltage<=52.84) {
battSOC=70;
} 
if (SOCvoltage<=52.76) {
battSOC=69;
}
if (SOCvoltage<=52.68) {
battSOC=68;
}
if (SOCvoltage<=52.60) {
battSOC=67;
}
if (SOCvoltage<=52.52) {
battSOC=66;
}
 if (SOCvoltage<=52.44) {
battSOC=65;
}
if (SOCvoltage<=52.34) {
battSOC=64;
}
if (SOCvoltage<=52.26) {
battSOC=63;
}
if (SOCvoltage<=52.24) {
battSOC=62;
}
if (SOCvoltage<=52.04) {
battSOC=61;
}
if (SOCvoltage<=51.94) {
battSOC=60;
}
if (SOCvoltage<=51.88) {
battSOC=59;
}
if (SOCvoltage<=51.82) {
battSOC=58;
}
if (SOCvoltage<=51.76) {
battSOC=57;
}
if (SOCvoltage<=51.70) {
battSOC=56;
}
if (SOCvoltage<=51.64) {
battSOC=55;
}
if (SOCvoltage<=51.58) {
battSOC=54;
}
if (SOCvoltage<=51.52) {
battSOC=53;
}
if (SOCvoltage<=51.46) {
battSOC=52;
}
if (SOCvoltage<=51.40) {
battSOC=51;
}
if (SOCvoltage<=51.34) {
battSOC=50;
}
if (SOCvoltage<=51.26) {
battSOC=49;
}
if (SOCvoltage<=51.18) {
battSOC=48;
}
if (SOCvoltage<=51.10) {
battSOC=47;
}
if (SOCvoltage<=51.02) {
battSOC=46;
}
if (SOCvoltage<=50.56) {
battSOC=45;
}
if (SOCvoltage<=50.87) {
battSOC=44;
}
if (SOCvoltage<=50.81) {
battSOC=43;
}
if (SOCvoltage<=50.75) {
battSOC=42;
}
if (SOCvoltage<=50.69) {
battSOC=41;
}
if (SOCvoltage<=50.63) {
battSOC=40;
}
if (SOCvoltage<=50.60) {
battSOC=39;
}
if (SOCvoltage<=50.54) {
battSOC=38;
}
if (SOCvoltage<=50.52) {
battSOC=37;
}
if (SOCvoltage<=50.48) {
battSOC=36;
}
if (SOCvoltage<=50.44) {
battSOC=35;
}
if (SOCvoltage<=50.38) {
battSOC=34;
}
if (SOCvoltage<=50.32) {
battSOC=33;
}
if (SOCvoltage<=50.26) {
battSOC=32;
}
if (SOCvoltage<=50.20) {
battSOC=31;
}
if (SOCvoltage<=50.14) {
battSOC=30;
}
if (SOCvoltage<=50.08) {
battSOC=29;
}
if (SOCvoltage<=50.02) {
battSOC=28;
}
if (SOCvoltage<=49.96) {
battSOC=27;
}
if (SOCvoltage<=49.90) {
battSOC=26;
}
if (SOCvoltage<=49.84) {
battSOC=25;
}
if (SOCvoltage<=49.76) {
battSOC=24;
}
if (SOCvoltage<=49.68) {
battSOC=23;
}
if (SOCvoltage<=49.60) {
battSOC=22;
}
if (SOCvoltage<=49.52) {
battSOC=21;
}
if (SOCvoltage<=49.44) {
battSOC=20;
}
if (SOCvoltage<=49.28) {
battSOC=19;
}
if (SOCvoltage<=49.16) {
battSOC=18;
}
if (SOCvoltage<=49.04) {
battSOC=17;
}
if (SOCvoltage<=48.89) {
battSOC=16;
}
if (SOCvoltage<=48.74) {
battSOC=15;
}
if (SOCvoltage<=48.66) {
battSOC=14;
}  
if (SOCvoltage<=48.59) {
battSOC=13;
}  
if (SOCvoltage<=48.49) {
battSOC=12;
}  
if (SOCvoltage<=48.34) {
battSOC=11;
}  
if (SOCvoltage<=48.14) {
battSOC=10;
} 
if (SOCvoltage<=47.94) {
battSOC=9;
}  
if (SOCvoltage<=47.64) {
battSOC=8;
}  
if (SOCvoltage<=47.34) {
battSOC=7;
}  
if (SOCvoltage<=47.04) {
battSOC=6;
}   
if (SOCvoltage<=46.64) {
battSOC=5;
} 
if (SOCvoltage<=46.24) {
battSOC=4;
}  
if (SOCvoltage<=45.74) {
battSOC=3;
}  
if (SOCvoltage<=45.24) {
battSOC=2;
}  
if (SOCvoltage<=44.64) {
battSOC=1;
}   
if (SOCvoltage<=43.56) {
battSOC=0;
}  
 
}
