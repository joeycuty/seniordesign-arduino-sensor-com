#include <SoftwareSerial.h> 
#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>
//set light id in code.
const String lightId = "aaa002";
int lightMode = 0;

//how EEPROM IS LAID OUT////////////////
//registers 0 - 10 : slave ids
//11 : light status code
//257 : last slave index.
////////////////////////////////////////

//libraries for barometer
#include <SFE_BMP180.h>   // Barometer
#include <Wire.h>

//libraries for humidity sensor
#include <dht.h>


String masterWeather = "";
String masterId = "";
int masterPhotocellReading = 8000;
int masterARainVal = 0;
boolean masterRaining = false;
double masterTemp = 0.00;
double masterTemp2 = 0.00;
double masterPres = 0.00;
double masterHumidity = 0.00;

String redRequest = "";
String greenRequest = "";
String blueRequest = "";
String whiteRequest = "";

float memRedRequest = 0;
float memGreenRequest = 0;
float memBlueRequest = 0;
float memWhiteRequest = 0;


//PHOTOCELL VARS HERE///////////////////////////////////////////////////////////////////
int photocellPin = 0;     // Light sensor the cell and 10K pulldown are connected to a0
int photocellReading;     // the analog reading from the analog resistor divider

//RAIN SENSOR VARS HERE/////////////////////////////////////////////////////////////////
int aRainVal;
boolean raining = false;

//BAROMETER / THERMOMETER VARS HERE/////////////////////////////////////////////////////
SFE_BMP180 pressure;
#define ALTITUDE 102.0 // Altitude of Starkville in METERS

//HUMIDTY / THERMOMETER VARS HERE///////////////////////////////////////////////////////
dht DHT;
#define DHT11_PIN 4

  
  char status;
  double T,P,p0,a, inHg;

int code;

String xbee = "";

//amouunt of milliseconds since start of arduino
unsigned long previousZigbeeMillis = 0; 

unsigned long previousFaderMillis = 0;        
//amouunt of milliseconds since start of arduino 
//amount of time to run fader code from milliseconds.
const long faderInterval = 1000;     

//amouunt of milliseconds since start of arduino
unsigned long previousMillis = 0;
unsigned long Interval = 500;

unsigned long previousWeatherMillis = 0;
unsigned long weatherInterval = 700;
      
int color = 9;
int relay = 6; // the PWM pin the LED is attached to
int redFader = 10;
bool redBool = false;
int blueFader = 0;
int fadeAmount = 5;

#define NUMPIXELS  1

// When we setup the NeoPixel library, we tell it how many pixels, and which pin to use to send signals.
// Note that for older NeoPixel strips you might need to change the third parameter--see the strandtest
// example for more information on possible values.
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, color, NEO_GRB + NEO_KHZ800);


void setup() {

  EEPROM.write(11, 2);
  // put your setup code here, to run once:
  Serial.begin(9600);

  
  //set digital pin to input for rain sensor boolean
  pinMode(2,INPUT); 
  pinMode(relay,OUTPUT);

  analogWrite(6, 255);

  pixels.begin(); // This initializes the NeoPixel library.

  //initalize barometer
  if (pressure.begin())
    Serial.println("BMP180 init success");
  else
  
  {
    Serial.println("BMP180 init fail\n\n");
    while(1); // Pause forever.
  }

  //EEPROM.write(11, 0);
  
  lightMode = EEPROM.read(11);

  memRedRequest = EEPROM.read(12);
  memGreenRequest = EEPROM.read(13);
  memBlueRequest = EEPROM.read(14);
  memWhiteRequest = EEPROM.read(15);

  updateLight(lightMode);
}

void loop() {

  
  //get current milliseconds to compare
  unsigned long currentMillis = millis();

  if(currentMillis - previousWeatherMillis >= weatherInterval)
  {
    previousWeatherMillis = currentMillis;
  
      getWeatherData();

  }

  
  //run if loop every interval of milliseconds, send weather data here.
  if (currentMillis - previousFaderMillis >= faderInterval)
  {
    previousFaderMillis = currentMillis; 
    //update fader if light is in fading mode
    updateFaders();
  }
  
  //run if loop every interval of milliseconds, send weather data here.
  if (currentMillis - previousMillis >= Interval)
  {
    previousMillis = currentMillis; 
    //print weather data
    //sendWeatherMessage();
    if(xbee != "")
    {
      if(xbee.indexOf("_weather") != -1)
      {
        String fire = lightId + "w_";
        fire.concat(parseWeatherData(photocellReading, aRainVal, raining, DHT.temperature, (DHT.temperature * 1.8 + 32), inHg, DHT.humidity));
        Serial.println(fire); 
      }
      else if(xbee.indexOf("aaa001_") != -1)
      {
        
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
                  masterPhotocellReading = myBuffer.toInt();
                  break;
                case 2:
                  masterARainVal = myBuffer.toInt();
                  break;
                case 3:
                  masterRaining = myBuffer.toInt();
                  break;
                case 4:
                  masterTemp = myBuffer.toFloat();
                  break;
                case 5:
                  masterTemp2 = myBuffer.toFloat();
                  break;
                case 6:
                  masterPres = myBuffer.toFloat();
                  break;
                case 7:
                  masterHumidity = myBuffer.toFloat();
                  break;
                
                default:
                  if(part == 0)
                  {
                  masterId = myBuffer.substring(0, 6);
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
          

        if(masterId.length() > 0)
        {
          
          //Serial.println("UW");
          if(lightMode == 2)
          {
            updateLight(2);  
          }
          
          masterWeather = "";
          masterWeather.concat(masterId);
          masterWeather.concat("_");
          masterWeather.concat(parseWeatherData(masterPhotocellReading, masterARainVal, masterRaining, masterTemp, masterTemp2, masterPres, masterHumidity));
        }
        else
        {
          //Serial.println("BAD WEATHER");  
        }
      }
      else if(xbee.indexOf("m_") != -1)
      { 
        //Serial.println("MODE");
       // Serial.println(xbee);
        if(xbee.indexOf("test") != -1)
        {
         // Serial.println("TEST MODE");  
        }
        else if(xbee.indexOf("blue") != -1)
        {
         // Serial.println("BLUE MODE");
          updateLight(1);
        }
        else if(xbee.indexOf("active") != -1)
        {
          //Serial.println("ACTIVE MODE");
          updateLight(2);
        }
        else if(xbee.indexOf("construction") != -1)
        {
         // Serial.println("CONSTRUCTION MODE");
          updateLight(4);
        }
        else if(xbee.indexOf("accident") != -1)
        {
          
         // Serial.println("ACCIDENT MODE");
          updateLight(3);
        }
        else if(xbee.indexOf("evacuation") != -1)
        {
          
         // Serial.println("EVACUATION MODE");
          updateLight(5);
        }
        else if(xbee.indexOf("manual") != -1)
        {
          
         // Serial.println("MANUAL MODE");
         // Serial.println(xbee);
          xbee.substring(xbee.indexOf(lightId + "m_manual"));
          
//          redRequest = getValue(xbee, '_', 2);
//          greenRequest = getValue(xbee, '_', 3);
//          blueRequest = getValue(xbee, '_', 4);
//          whiteRequest = getValue(xbee, '_', 5);

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


      if(memRedRequest != redRequest.toInt() || memGreenRequest != greenRequest.toInt() || memBlueRequest != blueRequest.toInt() || memWhiteRequest != whiteRequest.toInt()) 
      {
            EEPROM.write(12, redRequest.toInt());
            EEPROM.write(13, greenRequest.toInt());
            EEPROM.write(14, blueRequest.toInt());
            EEPROM.write(15, whiteRequest.toInt());
      }

            
//          Serial.print("COLORS: ");
//          Serial.print(EEPROM.read(12));
//          Serial.print(" ");
//          Serial.print(EEPROM.read(13));
//          Serial.print(" ");
//          Serial.print(EEPROM.read(14));
//          Serial.print(" ");
//          Serial.print(EEPROM.read(15));
//          Serial.println(" ");

            updateLight(6);          
        }
      }
      else
      {
//Serial.println("IGNORE");
//        Serial.println(xbee);   
      }
      
        xbee = "";
        serialFlush();
    }
  }
  
  // put your main code here, to run repeatedly:
//CHECK FOR DATA FROM ZIGBEE/////////////////////////////////////////////
  if (Serial.available()) // if there is incoming serial data
  {
        xbee.concat(char(Serial.read()));
  
  }

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
////GET ALL WEATHER DATA//////////////////////////////////////////////////////////
void getWeatherData()
{  
  //PHOTOCELL MEASURING AMBIENT LIGHT HERE////////////////////////////////////////
  photocellReading = analogRead(photocellPin);  
  delay(10);
  ////////////////////////////////////////////////////////////////////////////////

  //RAIN SENSOR CODE HERE/////////////////////////////////////////////////////////
  aRainVal = analogRead(1);
  raining = !(digitalRead(2));
 
  //BAROMETER / THERMOMETER CODE HERE////////////////////////////////////////////
  
  status = pressure.startTemperature();
  if (status != 0)
  {
    // Wait for the measurement to complete:
    delay(status);
    status = pressure.getTemperature(T);
    if (status != 0)
    {
      // Print out the measurement:
      //Serial.print(T,2); //temp in Celcius
      
      status = pressure.startPressure(3);
      if (status != 0)
      {
        // Wait for the measurement to complete:
        delay(status);
        status = pressure.getPressure(P,T);
        inHg =  P*0.0295333727;
        if (status != 0)
        { 
          //  Serial.print(P,2); //pressure in millibars
          p0 = pressure.sealevel(P,ALTITUDE); //sea level pressure
          a = pressure.altitude(P,p0);//altitude in meters
        }
        else Serial.println("error retrieving pressure measurement\n");
      }
      else Serial.println("error starting pressure measurement\n");
    }
    else Serial.println("error retrieving temperature measurement\n");
  }
  else Serial.println("error starting temperature measurement\n");

  delay(10);  // Pause for 5 seconds.


  ///END BAROMOETER CODE/////////////////////////////////////////////////////////////////////////////

  //HUMIDITY CODE HERE/////////////////
  int chk = DHT.read11(DHT11_PIN);


  if(lightMode == 2)
  {
    updateLight(2);
  }
}

void serialFlush(){
  while(Serial.available() > 0) {
    char t = Serial.read();
  }
}   


//used to break strings by comma delimitter.//////////////////////////////////////
String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }

  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void setLights(int red, int green, int blue)
{

   
    pixels.setPixelColor(0, pixels.Color(green, red, blue)); // Moderately bright green color.

    pixels.show(); // This sends the updated pixel color to the hardware.

    delay(100); // Delay for a period of time (in milliseconds).

    if(green == 0 && red == 0 && blue == 0)
    {
      analogWrite(relay, 0);
    }
    else
    {
      analogWrite(relay, 255);  
    }
}


//UPDATE LIGHT AND SET STATUS////////////////////////////////////////////////////
void updateLight(int command)
{
  if(lightMode != command)
  {   
    EEPROM.write(11, command);
    lightMode = command;
  }
  
  switch (lightMode) {
    case 1:
      setLights(0, 0, 255);
   //   Serial.println("SET LIGHT TO FADE BLUE");
      break;
    case 2:
      //put active code here.
      if(photocellReading < 350 || masterPhotocellReading < 350)
      {
       //Weather: ROAD ICE SCENARIO
       // Temperature, rain value or humidity
      
      if(
       //local temp freezing
       (DHT.temperature * 1.8 + 32) < 45 && aRainVal < 499 
       //&& (DHT.humidity > 80 || inHg > 30.2 || inHg < 29.8)
       //other light freezing
       ||
       masterPhotocellReading < 3500 &&
       (masterTemp2) < 45 && masterARainVal < 499 
       //&& (masterHumidity > 80 || (masterPres) > 30.2 || (masterPres) < 29.8) &&(masterPres) > 1
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

        setLights(150, 105, 205);
    
       }
       
       //Weather: HEAVY FOG
       // pressure > x , rain value , humidity
       else if(
        //inHg > 29.53 &&
        aRainVal < 500 &&
        //DHT.humidity > 30 &&
        DHT.temperature * 1.8 + 32 > 86
        ||
        masterPhotocellReading < 3500 &&
        //(masterPres) > 29.53 && 
        masterARainVal < 500 &&
       // masterHumidity > 30 &&
        masterTemp2 > 86
        
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

          setLights(10, 225, 155);
       }
    
        //Weather: HEAVY rain
       // rain value , humidity or pressure
       else if(
        aRainVal < 500 // &&
       // DHT.humidity > 40 &&
        //inHg < 29.80
        ||
        masterPhotocellReading < 3500 &&
        masterARainVal < 500 //&&
        //masterHumidity > 40 &&
        //(masterPres) < 29.80
        
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
          setLights(205, 150, 0);
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
          setLights(255, 255, 255);
        }
      }
      else
      {
        
        setLights(0,0,0);
      }
      break;
    case 3:
      specialMode("accident", 0);
      break;
    case 4:
      specialMode("construction", 1);
      break;
    case 5:
      specialMode("evacuation", 0);
      break;
    case 6:
      specialMode("manualSetting", 0);
      break;
    default: 
    
      setLights(255, 0, 0);
     // Serial.println("SET LIGHT TO FADE RED");
    break;
  }
//  Serial.println("updating light...");
//  Serial.println(EEPROM.read(11));
}


void specialMode(String mode, int broadcast)
{
//  if(broadcast == 1)
//  {
//    Serial.print(lightId + "m_");
//    Serial.print(mode);  
//  }
  if(mode == "construction")
  {
      setLights(255, 100, 0);
  }
  else if(mode == "evacuation")
  {
      setLights(255, 100, 100);
  }
  else if(mode == "accident")
  {

      setLights(50, 50, 255);
  }
  else if(mode == "manualSetting")
  {
      memRedRequest = EEPROM.read(12);
      memGreenRequest = EEPROM.read(13);
      memBlueRequest = EEPROM.read(14);
      memWhiteRequest = EEPROM.read(15);
      
      float redCal = 0;
      float greenCal = 0;
      float blueCal = 0;
      float whiteCal = 0;

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
      redCal = (((memRedRequest-30)/70)*255);


      }
      
      if(memBlueRequest < 30)
      {
        bluez = false;
        blueCal = 0;
      }
      else
      {
      blueCal = (((memBlueRequest-30)/70)*255);
      }
      
      if(memGreenRequest < 30)
      {
        greenz = false;
        greenCal = 0;
      }
      else
      {
      greenCal = (((memGreenRequest-30)/70)*255);
      }
      
      if(memWhiteRequest < 50)
      {
        whitez = false;
        whiteCal = 0;
      }
      else if(memWhiteRequest > 90)
      {
        whiteCal = 0;  
      }
      else
      {
      whiteCal = (((memWhiteRequest-50)/70)*255);
      }

//      Serial.print("set lights :");
//      Serial.print(redCal); 
//      Serial.print(" "); 
//      Serial.print(greenCal); 
//      Serial.print(" "); 
//      Serial.print(blueCal); 
//      Serial.println(" ");  
      setLights(redCal, greenCal, blueCal);
  }
}


void updateFaders()
{
  if(lightMode == 0)
  {
 
    // reverse the direction of the fading at the ends of the fade:
    if (redFader <= 0 || redFader >= 255) {
      fadeAmount = -fadeAmount;
    } 
    
    redFader = redFader + fadeAmount;
    
  }
  else if(lightMode == 1)
  {
    blueFader = blueFader + fadeAmount;
 
    // reverse the direction of the fading at the ends of the fade:
    if (blueFader <= 0 || blueFader >= 255) {
      fadeAmount = -fadeAmount;
    } 
   // analogWrite(relay, blueFader);
    
  }
  
}
