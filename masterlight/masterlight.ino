//add libraries to project
//libraries for barometer
#include <SFE_BMP180.h>
#include <Wire.h>
//libraries for humidity sensor
#include <dht.h>
//used to connect GPRS shield.
#include <SoftwareSerial.h> 
//used to store data when device is off.
#include <EEPROM.h>
//how EEPROM IS LAID OUT////////////////
//registers 0 - 10 : slave ids
//11 : light status code
//12 - 15: manual mode color
//257 : last slave index.
////////////////////////////////////////
#include <string.h>

//set light id in code.
const String lightId = "aaa001";

//initalize some vars.
String slaveWeather = "";
String slaveId = "";
String conSlaveId = "";
int printFlop = 0;
//set up string buffer for xbee.
String xbee = "";
int code;

//get current light mode and slave light id if it is avaliable.
int lightMode = EEPROM.read(11);
int slaveNum = 2;


//use software serial for gprs shield since xbee and pc use hardware serial.
//set pins 7 and 8 for software serial.
SoftwareSerial gprsSerial(7, 8);

//PHOTOCELL VARS HERE///////////////////////////////////////////////////////////////////
int photocellPin = 0; 
int photocellReading; 

//RAIN SENSOR VARS HERE/////////////////////////////////////////////////////////////////
int aRainVal;
boolean raining = false;

//BAROMETER / THERMOMETER VARS HERE/////////////////////////////////////////////////////
SFE_BMP180 pressure;
#define ALTITUDE 102.0 // Altitude of Starkville in METERS
char status;
double T,P,p0,a;
double inHg = 0.00;

//HUMIDTY / THERMOMETER VARS HERE///////////////////////////////////////////////////////
dht DHT;
#define DHT11_PIN 4

//set dimmer pins.
int redDimmer = 3;
int redRelay = 10;
int redFader = 0;

int greenDimmer = 5; 
int greenRelay = 11;
int greenFader = 0;

int blueDimmer = 6;
int blueRelay = 13; 
int blueFader = 0;

int whiteDimmer = 9;  
int whiteRelay = 12;
int whiteFader = 0;

int fadeAmount = 5;

//initalize milli clocks for time based functions.
unsigned long previousWeatherMillis = 0;       
unsigned long previousWeatherUpdateMillis = 0;   
unsigned long previousFaderMillis = 0;        
unsigned long previousZigbeeMillis = 0;       

//amount of time to run each clock function in ms.
const long weatherInterval = 1000;           //get weather data.
const long weatherUpdateInterval = (60000); //send weather data clock
const long faderInterval = 100;              //set fader color.
const long zigbeeInterval = 500;             //send zigbee data.

//initalize vars for manual settings.
String redRequest = "";
String greenRequest = "";
String blueRequest = "";
String whiteRequest = "";

float memRedRequest = 0;
float memGreenRequest = 0;
float memBlueRequest = 0;
float memWhiteRequest = 0;

//initalize vars for slave weather data.
int slavePhotocellReading = 8000;
int slaveARainVal = 0;
boolean slaveRaining = false;
double slaveTemp = 0.00;
double slaveTemp2 = 0.00;
double slavePres = 0.00;
double slaveHumidity = 0.00;
  
//setup function runs on setup.
void setup()
{
  //set gprs baud rate.
  gprsSerial.begin(19200);
  //set hardware serial to 9600 baud rate.
  Serial.begin(9600);

  //set digital pin to input for rain sensor boolean.
  pinMode(2,INPUT);
  
  Serial.println("WORKING..");

//  //initalize barometer 
//  if (pressure.begin())
//    Serial.println("BMP180 init success");
//  else
//  {
//      Serial.println("BMP180 init fail\n\n");
////    while(1); // Pause forever.
//   }
  //print to serial console to confirm arduino is working.
  delay(10);

  Serial.println("WORKING..");
  //get slave id from EEPROM.
  slaveNum = 2;

  //process slave number into actual string id.
  conSlaveId = "";
  if(slaveNum > 0)
  {
    if(slaveNum < 10)
    {
      conSlaveId = "aaa00";
      conSlaveId.concat(slaveNum); 
    }  
    else if(slaveNum > 9 && slaveNum < 100)
    { 
      conSlaveId = "aaa0";
      conSlaveId.concat(slaveNum);
    }
    else if(slaveNum < 256)
    {
      conSlaveId = "aaa";
      conSlaveId.concat(slaveNum); 
    }
    else
    {
      conSlaveId = "NO LIGHT CONNECTED";  
    }
  }

  //set light dimmers as outputs
  pinMode(redDimmer, OUTPUT);
  pinMode(greenDimmer, OUTPUT);
  pinMode(blueDimmer, OUTPUT);
  pinMode(whiteDimmer, OUTPUT);

  //set relay controls as outputs
  pinMode(redRelay, OUTPUT);
  pinMode(greenRelay, OUTPUT);
  pinMode(blueRelay, OUTPUT);
  pinMode(whiteRelay, OUTPUT);

  digitalWrite(redRelay, HIGH);
  digitalWrite(greenRelay, HIGH);
  digitalWrite(blueRelay, HIGH);
  digitalWrite(whiteRelay, HIGH);

  Serial.println("WORKING..");

  //if light is in manual settings, get the color percentages from EEPROM.
  if(lightMode == 6)
  {
      memRedRequest = EEPROM.read(12);
      memGreenRequest = EEPROM.read(13);
      memBlueRequest = EEPROM.read(14);
      memWhiteRequest = EEPROM.read(15);
  }
  //update light last mode before shut off.
  updateLight(lightMode);
}

//loop function loops and loops and loops and loops
void loop()
{
  //get current milliseconds to compare
  unsigned long currentMillis = millis();
  
  //run if loop every interval of milliseconds, send weather data here.
  if (currentMillis - previousWeatherUpdateMillis >= weatherUpdateInterval)
  {
    previousWeatherUpdateMillis = currentMillis;  
    //send weather data to NEXMO.
    Serial.println("sent text message");
    sendWeatherMessage();
  }
  //run if loop every interval of milliseconds, update faders.
  if (currentMillis - previousFaderMillis >= faderInterval)
  {
    previousFaderMillis = currentMillis; 
    updateFaders();
  }
  //run if loop every interval of milliseconds, read zigbee buffer here.
  if (currentMillis - previousZigbeeMillis >= zigbeeInterval)
  {
    previousZigbeeMillis = currentMillis; 

    //if xbee buffer is not empty.
    if(xbee.length() > 0)
    {
      //if zigbee buffer contains weather data.
      if(xbee.indexOf("w_") != -1)
      {
          //these vars and while loop process the string into the correct parts by the _ delimiter, it 
          //is better described in the slave light code.
          int count = 0;
          int part = 0;
          String myBuffer = "";
          
          String requestedId = "";
          String command = "";
          while (count < xbee.length()) // delimiter is the semicolon
          {
            char myChar = xbee.charAt(count); 
            if(myChar == '_')
            {
              switch(part)
              {
                case 1:
                  slavePhotocellReading = myBuffer.toInt();
                  break;
                case 2:
                  slaveARainVal = myBuffer.toInt();
                  break;
                case 3:
                  slaveRaining = myBuffer.toInt();
                  break;
                case 4:
                  slaveTemp = myBuffer.toFloat();
                  break;
                case 5:
                  slaveTemp2 = myBuffer.toFloat();
                  break;
                case 6:
                  slavePres = myBuffer.toFloat();
                  break;
                case 7:
                  slaveHumidity = myBuffer.toFloat();
                  break;
                
                default:
                  if(part == 0)
                  {
                  slaveId = myBuffer.substring(0, 6);
                  } 
                  break; 
              }
              part++;
              myBuffer = "";
            }
            else
            {
              myBuffer.concat(myChar);  
            }
            count++;
        }

        //weather data grabed successfully.
        if(slaveId.length() > 0)
        {
          //update light mode if light is set to weather active mode.
          if(lightMode == 2)
          {
            updateLight(2);  
          }

          //parse weather data into string for nexmo.
          slaveWeather = "";
          slaveWeather.concat(slaveId);
          slaveWeather.concat("_");
          slaveWeather.concat(parseWeatherData(slavePhotocellReading, slaveARainVal, slaveRaining, slaveTemp, slaveTemp2, slavePres, slaveHumidity));
          Serial.println("GOOD WEATHER");
          Serial.println(slaveWeather);
        }
        else
        {
          Serial.println("BAD WEATHER"); 
          slaveWeather = "";  
          slaveWeather.concat(slaveId);
          slaveWeather.concat("_");
          slaveWeather.concat(parseWeatherData(slavePhotocellReading, slaveARainVal, slaveRaining, slaveTemp, slaveTemp2, slavePres, slaveHumidity));
          Serial.println(slaveWeather);
          
          slaveWeather = "";
        }
      }
      //check if slave light is sending mode data, not used currently.
      else if(xbee.indexOf(conSlaveId + "m_") != -1)
      {
        Serial.println("UA");  
      }
      else
      {
//        Serial.print("BAD ");
      }
      xbee = "";
      serialFlush();
    }
  }
  
  //run if loop every interval of milliseconds, loop through getting weather data, sending light status to
  //slave, and sending master weather data to slave.
  if (currentMillis - previousWeatherMillis >= weatherInterval)
  {
    previousWeatherMillis = currentMillis; 
    
    if(printFlop == 0)
    {
        getWeatherData();
        printFlop = 1;
    }
    else if(printFlop == 1)
    {
       getStatus();
        printFlop = 2;
    }
    else
    {
        String essage = lightId;
        essage.concat("_");
        essage.concat(parseWeatherData(photocellReading, aRainVal, raining, T, (DHT.temperature * 1.8 + 32), P, DHT.humidity));
        Serial.println(essage);
        
        printFlop = 0;
    }
  }

  //CHECK FOR DATA FROM ZIGBEE/////////////////////////////////////////////
  while(Serial.available()) // if there is incoming serial data
  {
      xbee.concat(char(Serial.read()));//push to buffer.
  }
  
  //GET GPRS MESSAGES/////////////////////////////////////////////////////////
  while(gprsSerial.available())
  {  //get all of the gprs data in a packet, ie a string instead of by char.
    String smsPacket = gprsSerial.readString();

    //let command log know whats up.
    Serial.println("SMS");
    Serial.println(smsPacket);
 
    String message = "";
    message = smsPacket.substring(smsPacket.indexOf("aaa")); //clean the string by removing the break in the string.
   
    message.remove(message.indexOf("$"));

    //this loop is same as used in the xbee processing above.
    int count = 0;
    int part = 0;
    String myBuffer = "";
    
    String requestedId = "";
    String command = "";
    while (count < message.length()) // delimiter is the semicolon
    {
      char myChar = message.charAt(count); 
      if(myChar == '_')
      {
        switch(part)
        {
          case 1:
            command = myBuffer;
            break;
          case 2:
            redRequest = myBuffer;
            break;
          case 3:
            greenRequest = myBuffer;
            break;
          case 4:
            blueRequest = myBuffer;
            break;
          case 5:
            whiteRequest = myBuffer;
            break;
          
          default:
            requestedId = myBuffer; 
            break; 
        }
        part++;
        myBuffer = "";
      }
      else
      {
        myBuffer.concat(myChar);  
      }
      count++;   
    }
   
//    Serial.print("FOUND MESSAGE: ");
//    Serial.print(message.length());
//    Serial.println("");
//    Serial.print(smsPacket);
//    Serial.print("message: ");
//    Serial.println(message);
//    Serial.print("requested ID: ");
//    Serial.println(requestedId);
//    Serial.print("command: ");
//    Serial.println(command);

    //if light command is manual mode.
    if(command == "6") 
    {

      //write manual color data to EEPROM and set mem vars.
      EEPROM.write(12, redRequest.toInt());
      EEPROM.write(13, greenRequest.toInt());
      EEPROM.write(14, blueRequest.toInt());
      EEPROM.write(15, whiteRequest.toInt());

      memRedRequest = redRequest.toInt();
      memGreenRequest = greenRequest.toInt();
      memBlueRequest = blueRequest.toInt();
      memWhiteRequest = whiteRequest.toInt();
    } 
    //if command is set slave light.
    else if(command == "7")
    {
     // Serial.println("ACCESS RADIO");
    }
    else
    {
        redRequest = "";
        blueRequest = "";
        greenRequest = "";
        whiteRequest = "";    
    }
    
    //check for AT command that new message was recieved.
    if(smsPacket.indexOf("CMTI:") != -1)
    {
      Serial.println("NEW MESSAGE RECIEVED");
      getMessages(); //get the new message(s)
    }
    //check for AT command that message was read by arduino.  this should run when getMessages() runs.
    else if(smsPacket.indexOf("CMGL:") != -1)
    {
      //Serial.println("MESSAGE READ");
//      Serial.println(smsPacket);
////        
//      Serial.print(requestedId);
//      Serial.print(command);
//       
//      Serial.println(incomingNum);
//      Serial.println(message);

      code = command.toInt();

      if(requestedId == lightId)
      {
        updateLight(code);
//        Serial.println("UPDATE THIS LIGHT");
        
        redRequest = "";
        blueRequest = "";
        greenRequest = "";
        whiteRequest = "";  
      }
      else if(requestedId == "all")
      {
        //updateLight(code);
        //slaveIndex = EEPROM.read(257);
//        Serial.println("UPDATE ALL LIGHTS");
      }
      else if(requestedId == conSlaveId)
      {
//         Serial.print(conSlaveId);
//         Serial.print("u_");
//         Serial.print(code);
      }
      else
      {
        redRequest = "";
        blueRequest = "";
        greenRequest = "";
        whiteRequest = "";  
      }

      deleteMessages(); //delete message after read.
    }
    
    //check for AT command that messages are being retreived from sim card.  this should also run when getMessages runs. 
    //(AT command same, one is requesting, other is listing. notice '=' and ':')
    else if(smsPacket.indexOf("CMGL=") != -1)
    { 
     // Serial.println(smsPacket);//print entire AT command.
       
    }
    //check for AT command that messages where deleted from sim card.
    else if(smsPacket.indexOf("CMGD") != -1)
    {
    //  Serial.println("MESSAGES DELETED");
    }
    //print out any AT command that was missed by if statements.
    else
    {
//      Serial.println("OTHER AT COMMAND..");
//      Serial.println(smsPacket);
      
      deleteMessages();
    }
  }
}

///////////////////////////////////////////////////////////////////////////////////////////
/////////////////////        OTHER FUNCTIONS       ////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////

//flush serial

void serialFlush(){
  while(Serial.available() > 0) {
    char t = Serial.read();
  }
}   

//ADD A SLAVE LIGHT TO INDEX///////////////////////////////////////////////////
void addLight(String light)
{
  EEPROM.write(0, light.toInt());
  
  slaveNum = EEPROM.read(0);
  conSlaveId = "";

  if(slaveNum > 0)
  {
    if(slaveNum < 10)
    {
      conSlaveId = "aaa00";
      conSlaveId.concat(slaveNum); 
    }  
    else if(slaveNum > 9 && slaveNum < 100)
    { 
      conSlaveId = "aaa0";
      conSlaveId.concat(slaveNum);
    }
    else if(slaveNum < 256)
    {
      conSlaveId = "aaa";
      conSlaveId.concat(slaveNum); 
    }
    else
    {
      conSlaveId = "NO LIGHT CONNECTED";  
    }

    conSlaveId="aaa002";
    updateLight(1);
  }


}

//UPDATE LIGHT AND SET STATUS////////////////////////////////////////////////////
void updateLight(int command)
{
  if(lightMode != command && command != 7)
  {   
    EEPROM.write(11, command);
    lightMode = command;
  }
  
  switch (command) {
    case 1:
   //   Serial.println("SET LIGHT TO FADE BLUE");
        Serial.println(conSlaveId + "m_blue");
      break;
    case 2:
      //put active code here.
      if(photocellReading < 350 || slavePhotocellReading < 350)
      {
       //Weather: ROAD ICE SCENARIO
       // Temperature, rain value or 

       if(
       //local temp freezing
       (DHT.temperature * 1.8 + 32) < 45 && aRainVal < 499 
       && (DHT.humidity > 80 || inHg > 30.2 || inHg < 29.8)
       //other light freezing
       ||
       slavePhotocellReading < 3500 && (slaveTemp2) < 45 && slaveARainVal < 499 
       && (slaveHumidity > 80 || (slavePres*0.0295333727) > 30.2 || (slavePres*0.0295333727) < 29.8)
       )
       {
        //NOTE: WILL NEED TO SET THE DIGITAL SIGNAL PINS FOR THE RELAYS HIGH RIGHT HERE
        //RELAY PIN: WHITE - 12, BLUE - 13, Green - 11, Red - 10 
//        RelayStatusOn(13);
//        RelayStatusOn(11);
//        RelayStatusOn(10);
//        
//        //Blue Light is pin 6, brightness of 250
//        analogWrite(6, 050);
//        //Green Light is pin 3, brightness of 255
//        analogWrite(5, 150);
//        //Red Light is pin 5, brightness of 255
//        analogWrite(3, 100);

        setLights(100, 150, 50, 0, true, true, true, false);
    
       }
       
       //Weather: HEAVY FOG
       // pressure > x , rain value , humidity
       else if(
        inHg > 29.53 && aRainVal < 500 &&
        DHT.humidity > 30 &&
        DHT.temperature * 1.8 + 32 > 90
        ||
       slavePhotocellReading < 3500 && (slavePres*0.0295333727) > 29.53 && slaveARainVal < 500 &&
        slaveHumidity > 30 &&
        slaveTemp2 > 90
        
        )
       {
//        RelayStatusOn(13);
//        RelayStatusOn(11);
//        RelayStatusOn(10);
//        
//        //Blue Light is pin 6, brightness of 100
//        analogWrite(6, 100);
//        //Green Light is pin 3, brightness of 030
//        analogWrite(5, 030);
//        //Red Light is pin 5, brightness of 255
//        analogWrite(3, 255);

          setLights(255, 30, 100, 0, true, true, true, false);
       }
    
        //Weather: HEAVY rain
       // rain value , humidity or pressure
       else if(
        aRainVal < 500 &&
        DHT.humidity > 40// &&
        //inHg < 29.80
        ||
       slavePhotocellReading < 3500 && 
        slaveARainVal < 500 &&
        slaveHumidity > 40// &&
       // (slavePres*0.0295333727) < 29.80
        
        )
       {
//        RelayStatusOn(11);
//        RelayStatusOn(10);
//        
//        //Green Light is pin 3, brightness of 100
//        analogWrite(5, 100);
//        //Red Light is pin 5, brightness of 45
//        analogWrite(3, 045);
//    
          setLights(45, 100, 0, 0, true, true, false, false);
       }

       
       // Basic Darkness: no weather
       else
       {
//        RelayStatusOn(13);
//        RelayStatusOn(12);
//    
//        //White Light is pin 9, brightness of 045
//        analogWrite(9, 045);
//        //Blue Light is pin 6, brightness of 120
//        analogWrite(6, 120);

          setLights(0, 0, 120, 45, false, false, true, true);
        }
      }
      else
      {
        setLights(0,0,0,0,false,false,false,false);
      }
      break;
    case 3:
      specialMode("accident", 1);
      break;
    case 4:
      specialMode("construction", 1);
      break;
    case 5:
      specialMode("evacuation", 1);
      break;
    case 6:
      specialMode("manualSetting", 0);
      break;
    case 7:
      addLight(redRequest);
      break;
    default: 
     // Serial.println("SET LIGHT TO FADE RED");
    break;
  }
//  Serial.println("updating light...");
//  Serial.println(EEPROM.read(11));
}

//Gets all messages by firing AT command to SIM900.///////////////////////////////
void getMessages()
{
  gprsSerial.println("AT+CMGL=\"ALL\"");
}

//Deletes all messages by firing AT command to SIM900.////////////////////////////
void deleteMessages()
{
  gprsSerial.println("AT+CMGD=1,4\r");
}

////SEND TEXT WITH DATA ENCAPSALATED/////////////////////////////////////////////
void sendWeatherMessage()
{
  String finalMessage = lightId;
  finalMessage.concat("_");
  finalMessage.concat(parseWeatherData(photocellReading, aRainVal, raining, T, (DHT.temperature  * 1.8 + 32), P, DHT.humidity));

  if(slaveId == conSlaveId)
  {
    
    finalMessage.concat("X");
    finalMessage.concat(slaveWeather);
    slaveId = "";
  }

  //Serial.println("SEND WEATHER DATA");
  Serial.println(finalMessage);
  
  gprsSerial.print("AT+CMGF=1\r"); // Set the shield to SMS mode
  delay(100);
  // set number to send.
  gprsSerial.println("AT+CMGS = \"+REMOVEDPRIVATEDAATA\"");
  //gprsSerial.println("AT+CMGS = \"+\"");
  delay(100);
  //set content of message
  gprsSerial.println(finalMessage);
  delay(100);
  //the ASCII code of the ctrl+z is 26
  gprsSerial.print((char)26);
  delay(100);
  //send message.
  gprsSerial.println();
  delay(200);

}


void SendTextMessage()
{
  gprsSerial.print("AT+CMGF=1\r"); // Set the shield to SMS mode
  delay(100);
  // set number to send.
  gprsSerial.println("AT+CMGS = \"+1REMOVEDPRIVATEDATA\"");
  delay(100);
  //set content of message
  gprsSerial.println("Hello from Arduino");
  delay(100);
  //the ASCII code of the ctrl+z is 26
  gprsSerial.print((char)26);
  delay(100);
  //send message.
  gprsSerial.println();
}

String parseWeatherData(int photocellPin, int aRainVal, boolean raining, double Temp, double Temp2, double Pressure, double Humidity)
{

  String weatherstring = "";

  weatherstring.concat(String(photocellPin));
  weatherstring.concat("_");
  weatherstring.concat(String(aRainVal));
  weatherstring.concat("_");
  weatherstring.concat(String(raining));
  weatherstring.concat("_");
  weatherstring.concat(String(Temp));
  weatherstring.concat("_");
  weatherstring.concat(String(Temp2));
  weatherstring.concat("_");
  weatherstring.concat(String(Pressure));
  weatherstring.concat("_");
  weatherstring.concat(String(Humidity));
  weatherstring.concat("_");

  return weatherstring;
}

void getStatus()
{
  switch (lightMode) {
    case 1:
        Serial.print(conSlaveId);
        Serial.println("m_blue");
      break;
    case 2:
        Serial.print(conSlaveId);
        Serial.println("m_active");
      break;
    case 3:
        Serial.print(conSlaveId);
        Serial.println("m_accident");
      break;
    case 4:
        Serial.print(conSlaveId);
        Serial.println("m_construction");
      break;
    case 5:
        Serial.print(conSlaveId);
        Serial.println("m_evacuation");
      break;
    case 6:
        Serial.print(conSlaveId);
        Serial.print("m_manual_");
         Serial.print(memRedRequest);
        Serial.print("_");
        Serial.print(memGreenRequest);
        Serial.print("_");
        Serial.print(memBlueRequest);
        Serial.print("_");
        Serial.print(memWhiteRequest);
        Serial.println("_");
        
  }
}

////GET ALL WEATHER DATA//////////////////////////////////////////////////////////
void getWeatherData()
{  
  
        Serial.print(conSlaveId);
        Serial.println("_weather");
       
  //PHOTOCELL MEASURING AMBIENT LIGHT HERE////////////////////////////////////////
  photocellReading = analogRead(photocellPin);  
  delay(10);
  ////////////////////////////////////////////////////////////////////////////////

  //RAIN SENSOR CODE HERE/////////////////////////////////////////////////////////
  aRainVal = analogRead(1);
  raining = !(digitalRead(2));
 
  //BAROMETER / THERMOMETER CODE HERE////////////////////////////////////////////
//  
//  Serial.print("working here");
//  status = pressure.startTemperature();
//  if (status != 0)
//  {
//      Serial.print("working here");
//    // Wait for the measurement to complete:
//    delay(status);
//    status = pressure.getTemperature(T);
//    if (status != 0)
//    {
//      Serial.print("working here");
//      // Print out the measurement:
//      //Serial.print(T,2); //temp in Celcius
//      
//      status = pressure.startPressure(3);
//      if (status != 0)
//      {
//        Serial.print("working here");
//        // Wait for the measurement to complete:
//        delay(status);
//        status = pressure.getPressure(P,T);
//        inHg =  P*0.0295333727;
//        if (status != 0)
//        { 
//          //  Serial.print(P,2); //pressure in millibars
//          p0 = pressure.sealevel(P,ALTITUDE); //sea level pressure
//          a = pressure.altitude(P,p0);//altitude in meters
//        }
//        else Serial.println("error retrieving pressure measurement\n");
//      }
//      else Serial.println("error starting pressure measurement\n");
//    }
//    else Serial.println("error retrieving temperature measurement\n");
//  }
//  else Serial.println("error starting temperature measurement\n");

  delay(10);  // Pause for 5 seconds.


  ///END BAROMOETER CODE/////////////////////////////////////////////////////////////////////////////

  //HUMIDITY CODE HERE/////////////////
  int chk = DHT.read11(DHT11_PIN);

  //Serial.println(parseWeatherData(photocellPin, aRainVal, raining, T, inHg, DHT.humidity));

  if(lightMode == 2)
  {
    //Serial.println("UPDATING LIGHT");
    updateLight(2);
  }
}


void specialMode(String mode, int broadcast)
{
  if(broadcast == 1)
  {
    Serial.print(conSlaveId + "m_");
    Serial.print(mode);  
  }
  if(mode == "construction")
  {
      setLights(0, 50, 0, 0, true, true, false, false);
  }
  else if(mode == "evacuation")
  {
      setLights(0, 0, 0, 255, true, false, false, true);
  }
  else if(mode == "accident")
  {
      setLights(0, 0, 0, 255, false, false, true, true);
  }
  else if(mode == "manualSetting")
  {
      
      float redCal = 0.00;
      float greenCal = 0.00;
      float blueCal = 0.00;
      float whiteCal = 0.00;

      bool redz = true;
      bool bluez = true;
      bool greenz = true;
      bool whitez = true;
      
      if(memRedRequest < 30)
      {
        redz = false;
        redCal = 0;
      }
      else
      {
      redCal = 255 - (((memRedRequest-30)/70)*255);
       }
      
      if(memBlueRequest < 30)
      {
        bluez = false;
        blueCal = 0;
      }
      else
      {
      blueCal = 255 - (((memBlueRequest-30)/70)*255);
     }
      
      if(memGreenRequest < 30)
      {
        greenz = false;
        greenCal = 0;
      }
      else
      {
      greenCal = (255 - (((memGreenRequest-30)/70)*255));
      
      }
      
      if(memWhiteRequest < 50)
      {
        whitez = false;
      }
      else if(memWhiteRequest > 90)
      {
        whiteCal = 0;  
      }
      else
      {
      whiteCal = 255 - (((memWhiteRequest-50)/70)*255);
      }
      

//      Serial.print("SET LIGHTS: ");
//      Serial.print(redCal);
//      Serial.print(" ");
//      Serial.print(greenCal);
//      Serial.print(" ");
//      Serial.print(blueCal);
//      Serial.print(" ");
//      Serial.print(whiteCal);
//      Serial.print(" ");
      
      setLights(redCal, greenCal, blueCal, whiteCal, redz, greenz, bluez, whitez);
  }
}

void setLights(int redDimmerVal, int greenDimmerVal, int blueDimmerVal, int whiteDimmerVal, boolean redRelayVal, boolean greenRelayVal, boolean blueRelayVal, boolean whiteRelayVal)
{
  analogWrite(redDimmer, redDimmerVal);
  analogWrite(greenDimmer, greenDimmerVal);
  analogWrite(blueDimmer, blueDimmerVal);
  analogWrite(whiteDimmer, whiteDimmerVal);

  digitalWrite(redRelay, !redRelayVal);
  digitalWrite(greenRelay, !greenRelayVal);
  digitalWrite(blueRelay, !blueRelayVal);
  digitalWrite(whiteRelay, !whiteRelayVal);
}

void updateFaders()
{
  if(lightMode == 0)
  {
    redFader = redFader + fadeAmount;
 
    // reverse the direction of the fading at the ends of the fade:
    if (redFader <= 0 || redFader >= 255) {
      fadeAmount = -fadeAmount;
    } 
    setLights(redFader, 0, 0, 0, true, false, false, false);
  }
  else if(lightMode == 1)
  {
    blueFader = blueFader + fadeAmount;
 
    // reverse the direction of the fading at the ends of the fade:
    if (blueFader <= 0 || blueFader >= 255) {
      fadeAmount = -fadeAmount;
    } 
    setLights(0, 0, blueFader, 0, false, false, true, false);
    
  }
  
}
