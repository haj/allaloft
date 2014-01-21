#include <IridiumSBD.h>
#include <QueueList.h>
#include <SoftwareSerial.h>
#include <Servo.h>
#include <OneWire.h>
#include <TinyGPS.h> // NMEA parsing: http://arduiniana.org
#include <PString.h> // String buffer formatting: http://arduiniana.org

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/common.h>
#include <avr/wdt.h>
#include <avr/sleep.h>


//mode dependent code for iridium
#define EMULATE
#ifdef EMULATE
IridiumSBD isbd(Serial, -1);
#define IRIDIUM_INTERVAL 5000
#define LOOP_DELAY 1000
#define DEFAULT_REPORT_INTERVAL 20000
#else
IridiumSBD isbd(Serial1, 39);
//TODO: Set this to something more than 1 min
#define IRIDIUM_INTERVAL 60000//900000  <--900k is 15 min
#define LOOP_DELAY 1000
#define DEFAULT_REPORT_INTERVAL 900000
#endif

//pin definitions
#define ledPin 13
#define INTERNAL_TEMP_PIN 30
#define INTERNAL_TEMP_PIN 32
#define RELEASE_PIN_0 7
#define RELEASE_PIN_1 6
#define RELEASE_PIN_2 5
#define RELEASE_PIN_3 4



#define CMD_GET_POS  0
#define CMD_RELEASE  1
#define CMD_SET_REPORT_INTERVAL 2

#define MSG_TEXT_REPORT 'U'
#define MSG_TEXT_REPORT_NO_FIX 'F'

//temp sensors
OneWire int_temp(30);
OneWire ext_temp(32);

TinyGPS tinygps;

//variables related to Iridium comms
int messagesWaiting = 0;   //how many messages in iridium system?
boolean sbdixTimeLapsed = false; //meh
float timeOfLastSBDIX = 0;         //keep track of time of last sbdix session
float timeOfLastReport = 0;
float reportInterval = DEFAULT_REPORT_INTERVAL;
QueueList <String> queue; //queue for outgoing messages

//arduino init
void setup()
{

  //setup outputs
  pinMode(ledPin, OUTPUT);
  pinMode(RELEASE_PIN_0, OUTPUT);
  pinMode(RELEASE_PIN_1, OUTPUT);
  pinMode(RELEASE_PIN_2, OUTPUT);
  pinMode(RELEASE_PIN_3, OUTPUT);
  
  digitalWrite(RELEASE_PIN_0,LOW);
  digitalWrite(RELEASE_PIN_1,LOW);
  digitalWrite(RELEASE_PIN_2,LOW);
  digitalWrite(RELEASE_PIN_3,LOW);
  //setup iridium comms
  #ifdef EMULATE
  Serial.begin(19200);
  isbd.setPowerProfile(1);
  isbd.useMSSTMWorkaround(true);
  #else
  Serial1.begin(19200);
  Serial.begin(19200);
  isbd.setPowerProfile(1);
  isbd.useMSSTMWorkaround(true);
  isbd.attachConsole(Serial);
  isbd.attachDiags(Serial);
  #endif
  
  //send a startup message to the server through iridium
  //queue.push ("AWAKE");
 
}

//running loop
void loop()
{  
  int err;  
  char outBuffer[255];
  String tmp = "";
  uint8_t buffer[255];
  size_t bufferSize = sizeof(buffer);
  unsigned long loopStartTime = millis();
   
  //produce outgoing msg
  doReport();

  //***********************************************
  //Iridium stuff
  //***********************************************

  
  //if time past, or message ready to send, or more messages to get
  //then run sbdix
  if(!queue.isEmpty() || sbdixTimeLapsed || (messagesWaiting > 0)){
    //FIXME: this is some flusing thing to help recover SBDRB.  Why do we need this?
    #ifdef EMULATE

    Serial.end();
    Serial.begin(19200);
    
    #else
    
    Serial1.end();
    Serial1.begin(19200);
    
    #endif
    
    isbd.begin();  //wake up modem
    
    //if there is an outgoing message in the queue, send it and check for incoming
    if(!queue.isEmpty())
    {
      tmp = queue.pop();
      tmp.toCharArray(outBuffer, tmp.length()+1);
      err = isbd.sendReceiveSBDText(outBuffer,buffer,bufferSize);
    }
    
    //otherwise, just check for incoming
    else
    {    
       err = isbd.sendReceiveSBDText("", buffer, bufferSize);
    }
    
    //check for errors after the sbdix is finished
    if (err != 0)
    {
      Serial.print("sendReceiveSBDText failed: error ");
      Serial.println(err);
      Serial.print("\r");
      return;
    }
    
    //if we receive a message process it
    if(bufferSize > 0){
      processIncoming(buffer,bufferSize);
    }
    
    //see how many other messages are waiting for us in the iridum system
    messagesWaiting = isbd.getWaitingMessageCount();
    
    //now put the modem back to sleep
    isbd.sleep();
    
    //note time 
    sbdixTimeLapsed = false;
    timeOfLastSBDIX = millis();
        
  }

  // Sleep
  Serial.print("Going to sleep mode\r");
  Serial1.end();
  Serial2.end();
  delay(1000); // Wait for serial ports to clear
  //digitalWrite(ledPin, LOW);
  //pinMode(ledPin, INPUT);
  int elapsedSeconds = (int)((millis() - loopStartTime) / 1000);
  if (elapsedSeconds < (reportInterval/1000))
    bigSleep((reportInterval/1000) - elapsedSeconds); // wait about an hour

  // Wake
  Serial.print("Wake up!\r");
  //pinMode(ledPin, OUTPUT);

}



void doRelease(uint8_t in[])
{
   switch(in[1])
   {
     case 0:
         digitalWrite(RELEASE_PIN_0,HIGH);
         delay(10000);
         digitalWrite(RELEASE_PIN_0,LOW);
     case 1:
         digitalWrite(RELEASE_PIN_1,HIGH);
         delay(10000);
         digitalWrite(RELEASE_PIN_1,LOW);
      case 2:
         digitalWrite(RELEASE_PIN_2,HIGH);
         delay(10000);
         digitalWrite(RELEASE_PIN_2,LOW);
      case 3:
         digitalWrite(RELEASE_PIN_3,HIGH);
         delay(10000);
         digitalWrite(RELEASE_PIN_3,LOW);
      case 4:
         digitalWrite(ledPin,HIGH);
         delay(10000);
         digitalWrite(ledPin,LOW);
   } 
  
   return; 
}

float get_internal_temp(){
 
 byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  float celsius, fahrenheit;
  
  if ( !int_temp.search(addr)) {
    //Serial.println("No more addresses.");
    //Serial.println();
    int_temp.reset_search();
    delay(250);
    return  999.999;
  }
  /*
  Serial.print("ROM =");
  for( i = 0; i < 8; i++) {
    Serial.write(' ');
    Serial.print(addr[i], HEX);
  }*/

  if (OneWire::crc8(addr, 7) != addr[7]) {
      //Serial.println("CRC is not valid!");
      return 999.999;
  }
  //Serial.println();
 
  // the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
      //Serial.println("  Chip = DS18S20");  // or old DS1820
      type_s = 1;
      break;
    case 0x28:
      //Serial.println("  Chip = DS18B20");
      type_s = 0;
      break;
    case 0x22:
      //Serial.println("  Chip = DS1822");
      type_s = 0;
      break;
    default:
      //Serial.println("Device is not a DS18x20 family device.");
      return 999.999;
  } 

  int_temp.reset();
  int_temp.select(addr);
  int_temp.write(0x44, 1);        // start conversion, with parasite power on at the end
  
  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  
  present = int_temp.reset();
  int_temp.select(addr);    
  int_temp.write(0xBE);         // Read Scratchpad

  //Serial.print("  Data = ");
  //Serial.print(present, HEX);
  //Serial.print(" ");
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = int_temp.read();
    //Serial.print(data[i], HEX);
    //Serial.print(" ");
  }
  //Serial.print(" CRC=");
  //Serial.print(OneWire::crc8(data, 8), HEX);
  //Serial.println();

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  celsius = (float)raw / 16.0;
  fahrenheit = celsius * 1.8 + 32.0;
  /*Serial.print("  Internal Temperature = ");
  Serial.print(celsius);
  Serial.print(" Celsius\r");
  Serial.print(fahrenheit);
  Serial.println(" Fahrenheit");*/
  return celsius;
}


float get_external_temp(){
 
 byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  float celsius, fahrenheit;
  
  if ( !ext_temp.search(addr)) {
    //Serial.println("No more addresses.");
    //Serial.println();
    ext_temp.reset_search();
    delay(250);
    return  999.999;
  }
  /*
  Serial.print("ROM =");
  for( i = 0; i < 8; i++) {
    Serial.write(' ');
    Serial.print(addr[i], HEX);
  }*/

  if (OneWire::crc8(addr, 7) != addr[7]) {
      //Serial.println("CRC is not valid!");
      return 999.999;
  }
  //Serial.println();
 
  // the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
      //Serial.println("  Chip = DS18S20");  // or old DS1820
      type_s = 1;
      break;
    case 0x28:
      //Serial.println("  Chip = DS18B20");
      type_s = 0;
      break;
    case 0x22:
      //Serial.println("  Chip = DS1822");
      type_s = 0;
      break;
    default:
      //Serial.println("Device is not a DS18x20 family device.");
      return 999.999;
  } 

  ext_temp.reset();
  ext_temp.select(addr);
  ext_temp.write(0x44, 1);        // start conversion, with parasite power on at the end
  
  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  
  present = ext_temp.reset();
  ext_temp.select(addr);    
  ext_temp.write(0xBE);         // Read Scratchpad

  //Serial.print("  Data = ");
  //Serial.print(present, HEX);
  //Serial.print(" ");
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ext_temp.read();
    //Serial.print(data[i], HEX);
    //Serial.print(" ");
  }
  //Serial.print(" CRC=");
  //Serial.print(OneWire::crc8(data, 8), HEX);
  //Serial.println();

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  celsius = (float)raw / 16.0;
  fahrenheit = celsius * 1.8 + 32.0;
  /*Serial.print("  External Temperature = ");
  Serial.print(celsius);
  Serial.print(" Celsius\r");*/
  return celsius;

}

//parse incoming messages and call respective function
void processIncoming(uint8_t in[], int bufferSize)
{
  Serial.write("Received incoming messasge\r");
  
  switch(in[0])
  {
     case CMD_GET_POS:
       doReport();
       break;
     case CMD_RELEASE:
       doRelease(in);
       break;
     case CMD_SET_REPORT_INTERVAL:
       setReportInterval(in);
       break;

  }
  
  return;
}

void setReportInterval(uint8_t in[])
{
   //TODO: real work here
   return;
}

void doReport()
{
  int year;
  byte month, day, hour, minute, second, hundredths;
  unsigned long dateFix, locationFix;
  float latitude, longitude;
  long altitude;
  bool fixFound = false;
  bool charsSeen = false;
  unsigned long loopStartTime = millis();
  float int_temp = 0.0;
  float ext_temp = 0.0;
  
  int_temp = get_internal_temp();
  ext_temp = get_external_temp();
   
   // Step 1: Reset TinyGPS and begin listening to the GPS
  //Serial.println("Beginning to listen for GPS traffic...\r");
  Serial2.begin(9600);
  tinygps = TinyGPS();
  //Serial2.listen();

  // Step 2: Look for GPS signal for up to 7 minutes
  for (unsigned long now = millis(); !fixFound && millis() - now < 7UL * 60UL * 1000UL;)
  {
    if (Serial2.available())
    {
      Serial.print(Serial2.read());
      /*
      charsSeen = true;
      if (tinygps.encode(Serial2.read()))
      {
        tinygps.f_get_position(&latitude, &longitude, &locationFix);
        tinygps.crack_datetime(&year, &month, &day, &hour, &minute, &second, &hundredths, &dateFix);
        altitude = tinygps.altitude();
        fixFound = locationFix != TinyGPS::GPS_INVALID_FIX_TIME && 
                   dateFix != TinyGPS::GPS_INVALID_FIX_TIME && 
                   altitude != TinyGPS::GPS_INVALID_ALTITUDE &&
                   year != 2000;
      }*/
    }
    ISBDCallback(); // We can call it during our GPS loop too.

    // if we haven't seen any GPS in 5 seconds, then the wiring is wrong.
    if (!charsSeen && millis() - now > 5000)
    break;
   }
   
   //Serial.print(charsSeen ? fixFound ? F("A GPS fix was found!\r") : F("No GPS fix was found.\r") : F("Wiring error: No GPS data seen.\r"));

  char outBuffer[60]; // Always try to keep message short

  if (fixFound)
  {
    sprintf(outBuffer, "%c:%d%02d%02d%02d%02d%02d,", MSG_TEXT_REPORT, year, month, day, hour, minute, second);
    int len = strlen(outBuffer);
    PString str(outBuffer + len, sizeof(outBuffer) - len);
    str.print(latitude, 6);
    str.print(",");
    str.print(longitude, 6);
    str.print(",");
    str.print(altitude / 100);
    str.print(",");
    str.print(tinygps.f_speed_knots(), 1);
    str.print(",");
    str.print(tinygps.course() / 100);
    str.print(",");
    str.print(ext_temp,1);
    str.print(",");
    str.print(int_temp,1);
  }
  else
  {
    sprintf(outBuffer, "%c:", MSG_TEXT_REPORT_NO_FIX);
    int len = strlen(outBuffer);
    PString str(outBuffer + len, sizeof(outBuffer) - len);
    str.print(ext_temp,1);
    str.print(",");
    str.print(int_temp,1);
  }
  queue.push(outBuffer);
  
}

bool ISBDCallback()
{
   
   //digitalWrite(ledPin, (millis() / 1000) % 2 == 1 ? HIGH : LOW);  
   
   return true;
}


// Sleep stuff
SIGNAL(WDT_vect) {
  wdt_disable();
  wdt_reset();
  WDTCSR &= ~_BV(WDIE);
}

void babySleep(uint8_t wdt_period) 
{
  wdt_enable(wdt_period);
  wdt_reset();
  WDTCSR |= _BV(WDIE);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_mode();
  wdt_disable();
  WDTCSR &= ~_BV(WDIE);
}

void smallSleep(int milliseconds) {
  while (milliseconds >= 8000) { babySleep(WDTO_8S); milliseconds -= 8000; }
  if (milliseconds >= 4000)    { babySleep(WDTO_4S); milliseconds -= 4000; }
  if (milliseconds >= 2000)    { babySleep(WDTO_2S); milliseconds -= 2000; }
  if (milliseconds >= 1000)    { babySleep(WDTO_1S); milliseconds -= 1000; }
  if (milliseconds >= 500)     { babySleep(WDTO_500MS); milliseconds -= 500; }
  if (milliseconds >= 250)     { babySleep(WDTO_250MS); milliseconds -= 250; }
  if (milliseconds >= 125)     { babySleep(WDTO_120MS); milliseconds -= 120; }
  if (milliseconds >= 64)      { babySleep(WDTO_60MS); milliseconds -= 60; }
  if (milliseconds >= 32)      { babySleep(WDTO_30MS); milliseconds -= 30; }
  if (milliseconds >= 16)      { babySleep(WDTO_15MS); milliseconds -= 15; }
}

void bigSleep(int seconds)
{
   while (seconds > 8) { smallSleep(8000); seconds -= 8;	}
   smallSleep(1000 * seconds);
}
