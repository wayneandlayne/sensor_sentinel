#include <SoftwareSerial.h>
#include <String.h>
#include <CapacitiveSensor.h> //Get from http://playground.arduino.cc/Main/CapacitiveSensor

/* Sensor Sentinel 
   by Adam Wolf, Wayne and Layne
   MAKE Weekend Project
   
   For more details, including part numbers and step-by-step instructions on using this code, see http://makeTKTKTK.TK
/*

/* Potential improvements */

/*
  - You can change the sensors to be just about anything, like a humidity sensor, a barometric pressure sensor, a temperature sensors, a compass, an accelerometer, a Hall effect or reed switch, a vibration sensor, a flex sensor, an ultrasonic rangefinder, a tilt sensor, an IR sensor, or even just a plain old button!  Initialize the sensor in setup(). If you want the sensor to trigger a message, then check for the triggering condition in loop(), otherwise just read the sensor in annotateMessage(), put some text in the message variable and you're set!
  
  - The code for interfacing with the cellular module is based upon code from Seeed Studio.  It has a lot of delays.  It is possible to write a function to watch for OK in the cellular module's output.  This would likely reduce those delays a bit, and make it a little more robust.

  - There are Arduino libraries for interfacing with the module on the Seeed Studio GPRS Shield.  They simplify the interaction (especially when doing complicated things), but we chose not to use them in order to make using this sketch easier.

  - Ask for the network time at start, and then keep track of it internally.  There are time libraries that can help with this.

  - Hook up a microphone to the MIC jack on the shield, and have the shield call you for 1 minute and then hang up.  You could listen in remotely!

  - If we try to send a text before the minimum delay is up, save it and send it later if there are no newer outgoing texts.

  - Have the cellular module check for incoming texts, and if you text it from your trusted number, have it reply back with sensor readings (or call you back and listen in!) Once you have this, you could attach servos, buzzers, anything, and trigger them from anywhere you have cell signal!
  
  - Depending upon where you live, you may be able to text the Twitter shortcode and post to Twitter through this shield, by changing the SMS_NUMBER.
  
  - Set this up for low power draw!  Normal Arduinos aren't really meant for low power/long battery life, so we didn't make power optimizations in this sketch.  However, you could rig this up to "sleep" most of the time, and last a good amount of time on a battery, especially if you're only sending outgoing texts.  You could even turn off the cellular module and only turn it on when you need to send a text.  
*/

/* Settings */
const char SMS_NUMBER[] = "+15558675309"; //remember to include + and country code! 
const unsigned long MIN_MS_BETWEEN_SMS = 60000;
const boolean SMS_ENABLED = false;
const boolean SEND_SMS_AT_STARTUP = true;
const boolean CAP_SENSOR_ENABLED = true; 

const long CAP_LOW_THRESHOLD = 8000; //if the capacitive sensor is lower than this, it will not count as a press.
const long CAP_HIGH_THRESHOLD = 12000; //if the capacitive sensor is higher than this, it will count as a press.  Put a gap between these numbers to prevent it repeatedly triggering when it is clsoe to the limit.

/* Pin Settings */
#define LIGHT_SENSOR A0
#define PIR_POWER 3
#define PIR_SENSOR 2
#define PIR_GROUND 4
#define CONTACT_SENSOR 10
#define CAPACITIVE_SEND 12
#define CAPACITIVE_RECEIVE 11

/* Global Variables */
String message = String(""); //make a new string and set it to empty
unsigned long last_sms_time = 0;
boolean pir_reading = LOW;
boolean contact_reading;
boolean capacitive_reading = false;
long raw_capacitive_reading;
CapacitiveSensor cap_sensor = CapacitiveSensor(CAPACITIVE_RECEIVE, CAPACITIVE_SEND);

SoftwareSerial cellularSerial(7, 8);

void setup()
{
  //This function runs once every time the Arduino is powered up.
  
  Serial.begin(115200);   // Set your Serial Monitor baud rate to this to see output from this sketch on your computer when plugged in via USB.
  Serial.println(F("Hello world.")); //prints to the computer via Serial Monitor (when plugged in via USB)
  
  pinMode(LIGHT_SENSOR, INPUT_PULLUP);
  pinMode(PIR_GROUND, OUTPUT);
  pinMode(PIR_POWER, OUTPUT);
  pinMode(PIR_SENSOR, INPUT);
  digitalWrite(PIR_GROUND, LOW);
  digitalWrite(PIR_POWER, HIGH);
  
  pinMode(CONTACT_SENSOR, INPUT_PULLUP);
  contact_reading = digitalRead(CONTACT_SENSOR);
  
  //The PIR needs about 8 seconds to stabilize, so we start powering it before we start powering the cell module.
  
  cellularSerial.begin(19200);  // the default baud rate of the cellular module
  
  turnOnCellModule();
  
  enableNetworkTime(); //Ask the module to get the time from the network.

  if (CAP_SENSOR_ENABLED)
  {  
    raw_capacitive_reading = cap_sensor.capacitiveSensor(30);
    if (raw_capacitive_reading < CAP_LOW_THRESHOLD)
    {
      capacitive_reading = false;
    } else if (raw_capacitive_reading > CAP_HIGH_THRESHOLD)
    {
      capacitive_reading = true;
    }
  }
  
  //Make sure we've waited 10 seconds since bootup, so the PIR and network time is ready.
  while (millis() < 10000)
  {
    //do nothing
  }
  
  if (SEND_SMS_AT_STARTUP && SMS_ENABLED)
  {
    startTextMessage();
    cellularSerial.println(F("The Sensor Sentinel is ready."));
    endTextMessage();
  }
  Serial.println(F("The Sensor Sentinel is ready."));
}

void loop()
{
  //This function runs over and over after setup().
  drainSoftwareSerial(true);
  
  if (digitalRead(PIR_SENSOR) != pir_reading)
  {
    pir_reading = !pir_reading;
    if (pir_reading)
    {
      Serial.println(F("PIR just activated."));
      message = String("PIR activated:");
      sendTextMessage();
    } else
    {
      Serial.println(F("PIR deactivated."));
    }
  }
  
  if (digitalRead(CONTACT_SENSOR) != contact_reading)
  {
    contact_reading = !contact_reading;
    if (!contact_reading)
    {
      Serial.println(F("Contact switch just pressed."));
      message = String("Contact pressed:");
      sendTextMessage();
    } else
    {
      Serial.println(F("Contact switch just released."));
      message = String("Contact released:");
      sendTextMessage();    
    }
  }
  
  if (CAP_SENSOR_ENABLED)
  {
    raw_capacitive_reading = cap_sensor.capacitiveSensor(30);
    Serial.print("Raw capacitive reading: ");
    Serial.println(raw_capacitive_reading);
    
    if (capacitive_reading && raw_capacitive_reading < CAP_LOW_THRESHOLD)
    {
      capacitive_reading = !capacitive_reading;
      Serial.println("Cap sensor deactivated.");
      message = String("Cap sensor deactivated.");
      sendTextMessage();
    } else if (!capacitive_reading && raw_capacitive_reading > CAP_HIGH_THRESHOLD)
    {
      capacitive_reading = !capacitive_reading;
      Serial.println(F("Cap sensor activated."));
      message = String("Cap sensor activated.");
      sendTextMessage();
    }
  }
    
  delay(100);
}

void drainSoftwareSerial(boolean printToSerial)
{
  //This function reads the available data from the software serial port, and stops when there is no more incoming data.  If printToSerial is true, it prints the software serial data out to the hardware serial port, so you can see it the Serial Monitor.
  char b;
  while (cellularSerial.available())
  {
    b = cellularSerial.read();
    if (printToSerial)
    {
      Serial.print(b);
    }
  }
}

boolean toggleAndCheck()
{
  //This function toggles the power of the cell module, and tries to send a simple AT command to it.  If it replies, we know it is powered on.
  
  togglePower(); //the cell module may be on or off, we don't know.
  drainSoftwareSerial(true); //throw away all the data we have so far, until it stops sending.
  delay(50); //wait for it to turn on
  cellularSerial.print(F("AT+GMI\r")); //Ask the cellular module about the manufacturer
  delay(125); //wait for it to start responding
  if (cellularSerial.available())     //it responded! it's on!
  {
    drainSoftwareSerial(false);
    delay(200);
    return false; 
  }
  return true;
}

void turnOnCellModule()
{
  //This function will turn on the cellular module if it is off, but it will turn it off and back on if it is on.
  //Think of it as a fresh start!
  //If this takes more than twice, something's wrong.
  
  while (toggleAndCheck()) //keep toggling and checking until it returns false, which means we're good.  
  {
    Serial.println(F("Trying again."));
  }
}

void togglePower() //from Seeed Studio wiki
{
  Serial.println(F("Toggling cell module power."));
  pinMode(9, OUTPUT); 
  digitalWrite(9, LOW);
  delay(1000);
  digitalWrite(9, HIGH);
  delay(2000);
  digitalWrite(9, LOW);
  delay(3000);
}


void enableNetworkTime()
{
  //This function tells the module to ask for the network time.  It took about 5 seconds in our tests for it to come in.  After that, you can read it with getTime.
  cellularSerial.print(F("AT+CLTS=1\r"));
  delay(100);
  drainSoftwareSerial(true);
}


void getTime()
{
  //This function gets the time and puts it into message, but if the network hasn't sent the time over, then it's the module time, which starts over every time this gets rebooted.

  drainSoftwareSerial(true); //we want the serial buffer to be empty

  cellularSerial.print(F("AT+CCLK?\r"));
  delay(100);
  //start reading
  char b;
  boolean inside_quotes = false;
  while (cellularSerial.available())
  {
    b = cellularSerial.read(); //get one character from the module
    
    if (b == '"')
    {
      inside_quotes = !inside_quotes;  //if the character is a ", then toggle the variable inside_quotes
    }
    
    if (inside_quotes && b != '"') //if the inside_quotes variable is set and the character isn't a quote, then add it to message, and print it out too.
    {
      message += b;
    } 
  } 
}

void startTextMessage()
{
  //This function sends the beginning of the AT commands for an outgoing text to the cell module.
  
  cellularSerial.print(F("AT+CMGF=1\r"));    //Because we want to send the SMS in text mode
  delay(100);
  cellularSerial.print(F("AT + CMGS = \""));
  cellularSerial.print(SMS_NUMBER);
  cellularSerial.println(F("\""));
  delay(100);
}

void endTextMessage()
{
  //This function ends the commands to the module for sending a single text.
  delay(100);
  cellularSerial.println((char)26); //control-z
  delay(100);
  cellularSerial.println();
}
 
void sendTextMessage()
{
  //This function checks if it should send a text, based on SMS_ENABLED and MIN_MS_BETWEEN_SMS, and then does so.
  
  boolean send_sms = false;
  if (!SMS_ENABLED)
  {
    Serial.println(F("SMS disabled due to setting."));
  } else
  {
    if (last_sms_time > 0 && millis() < last_sms_time + MIN_MS_BETWEEN_SMS)
    {
      Serial.println(F("SMS blocked since we already sent one recently."));
    } else
      send_sms = true;
  }
  
  annotateMessage(); //fill out the rest of the text message
  
  if (send_sms)
  {
    startTextMessage();
    cellularSerial.print(message);
    endTextMessage();
    
    last_sms_time = millis();
    Serial.print(F("Actually sent the following SMS to "));
  } else
  {
    Serial.print(F("Didn't actually send this SMS to "));
  }
      Serial.print(SMS_NUMBER);
      Serial.print(F(":"));
      Serial.println(message);
}

void annotateMessage()
{
  //This function appends some sensor readings to the current text.
  
  message += " Light: ";
  message += analogRead(LIGHT_SENSOR);
  
  message += " Contact: ";
  if (digitalRead(CONTACT_SENSOR))
  {
    message += "released";
  } else
  {
    message += "pressed";
  }
  
  if (CAP_SENSOR_ENABLED)
  {
    message += " Cap: ";
    message += cap_sensor.capacitiveSensor(30);
  }
  
  message += " (sent at ";
  getTime();
  message += ")";
}
