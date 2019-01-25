/*
 * Using NeoPixel to illuminate different colors depending on the value that the SmarTrac reads.
 * BLUE = wet
 * RED = dry
 * WHITE = no response from RFID Reader
 * 
 * Hardware setup process:
 * Use jumper wires to connect the NeoPixel's Vcc, GND, and DIN pins
 * Connect the Vcc to Arduino's +5V pin
 * Connect the GND to Arduino's GND pin
 * Connect the DIN to Arduino's pin 6
 */
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

// Which pin on the Arduino is connected to the NeoPixels?
// On a Trinket or Gemma we suggest changing this to 1
#define PIN            6

// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS      1

//What is the Threshold for wet and dry?
#define THRESHOLD     19

//What length is the desired EPC array?
#define EPC_LENGTH    100

//How much to flash white during time a disconnect?
#define FLASHAMOUNT   10

// When we setup the NeoPixel library, we tell it how many pixels, and which pin to use to send signals.
// Note that for older NeoPixel strips you might need to change the third parameter--see the strandtest
// example for more information on possible values.
Adafruit_NeoPixel pixel = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

int delayval = 500; // delay for half a second

#include <SoftwareSerial.h> //used for transmitting to RFID board
SoftwareSerial softSerial(2,3); //RX, TX ; pins are perimenent

#include "SparkFun_UHF_RFID_Reader.h" //Library for controlling the M6E Nano module
RFID nano; //create instance 

void setup() {

  pixel.begin(); // This initializes the NeoPixel library.
  pixel.setPixelColor(0, 0, 0, 0);
  pixel.show();

//setup for hardware serial
  Serial.begin( 115200 ); //make sure serial monitor is set to correct baud rate
  while(!Serial); //wait until serial monitor is ready
  Serial.println();
  Serial.println("Initializing...");

//setup for RFID reader
  if(setupNano(38400) == false) //function defined below line 69
            //configure nano to run at 38400b/s
  {
    Serial.println("Module failed to respond. Please check wiring to RFID shield");

    for(int i = 0; i < FLASHAMOUNT; i++){
       pixel.setPixelColor(0, 0, 0, 0); // No color
       pixel.show(); // This sends the updated pixel color to the hardware.

       delay(150);

       pixel.setPixelColor(0, 50, 50, 50); // Moderately bright white color.
       pixel.show(); // This sends the updated pixel color to the hardware.

       delay(150);

    }
     
    while(1); //stop everything
  }
  
  nano.setRegion(REGION_NORTHAMERICA); //Set to North America
  nano.setReadPower(1500); //15.00 dBm. Higher values may cause USB port to brown out
  //Max Read TX Power is 27.00 dBm and may cause temperature-limit throttling

}

void loop() 
{  
  byte mySensorVal[2];  //sensor data will be 5 bits for 402h SmartTrac tags
  byte mySensorValLength;  
  byte responseTypeSensor = 0;  // 0 != RESPONSE_SUCCESS

  uint8_t epcStartIndex;
  uint8_t epcEndIndex;

  epcStartIndex = nano.getTagDataBytes() + 31;
  epcEndIndex = epcStartIndex + 12;

  //Read sensor value
  mySensorValLength = sizeof(mySensorVal); //length of sensor value may change each time .readTagSensorXXX is called
  responseTypeSensor = nano.readTagSensor402(mySensorVal, mySensorValLength, 200); //Scan for a new tag up to 1000ms

  if(responseTypeSensor == RESPONSE_SUCCESS)
  {
    //print sensor value
    Serial.println(F("sensor value: "));
    for (byte a = 1 ; a < 2 ; a++) 
    {
      byte epc[EPC_LENGTH];
      for(int i = 0; i < EPC_LENGTH; i++){
         epc[i] = nano.readMsgEPC(i);
      }
      Serial.println(mySensorVal[a], DEC);   //sensor value is read as decimal
      Serial.print("EPC: ");

      for(int i = 0; i < EPC_LENGTH; i++)
      {
         Serial.print(epc[i]);
         Serial.print(" ");
      }
      
      if(mySensorVal[a] > THRESHOLD) 
      {
      pixel.setPixelColor(0, 50, 0, 0); // Moderately bright red color.
      pixel.show(); // This sends the updated pixel color to the hardware.
      }
      else 
      {
      pixel.setPixelColor(0, 0, 50, 0); // Moderately bright green color.
      pixel.show(); // This sends the updated pixel color to the hardware.  
      }
    }
  }
  else
  {
    pixel.setPixelColor(0, 50, 50, 50);
    pixel.show(); 
    Serial.println(F("Searching for tag")); 
  }
}


//Gracefully handles a reader that is already configured and already reading continuously
//Because Stream does not have a .begin() we have to do this outside the library
boolean setupNano(long baudRate)
{
  nano.begin(softSerial); //Tell the library to communicate over software serial port

  //Test to see if we are already connected to a module
  //This would be the case if the Arduino has been reprogrammed and the module has stayed powered
  softSerial.begin(baudRate); //For this test, assume module is already at our desired baud rate
  while(!softSerial); //Wait for port to open

  //About 200ms from power on the module will send its firmware version at 115200. We need to ignore this.
  while(softSerial.available()) softSerial.read();
  
  nano.getVersion();

  if (nano.msg[0] == ERROR_WRONG_OPCODE_RESPONSE)
  {
    //This happens if the baud rate is correct but the module is doing a ccontinuous read
    nano.stopReading();

    Serial.println(F("Module continuously reading. Asking it to stop..."));

    delay(1500);
  }
  else
  {
    //The module did not respond so assume it's just been powered on and communicating at 115200bps
    softSerial.begin(115200); //Start software serial at 115200

    nano.setBaud(baudRate); //Tell the module to go to the chosen baud rate. Ignore the response msg

    softSerial.begin(baudRate); //Start the software serial port, this time at user's chosen baud rate
  }

  //Test the connection
  nano.getVersion();
  if (nano.msg[0] != ALL_GOOD) return (false); //Something is not right

  //The M6E has these settings no matter what
  nano.setTagProtocol(); //Set protocol to GEN2

  nano.setAntennaPort(); //Set TX/RX antenna ports to 1

  return (true); //We are ready to rock 
}
