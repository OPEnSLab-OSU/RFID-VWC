/*
  Reading multiple RFID tags, simultaneously!
  By: Nathan Seidle @ SparkFun Electronics
  Date: October 3rd, 2016
  https://github.com/sparkfun/Simultaneous_RFID_Tag_Reader

  Constantly reads and outputs any tags heard

  If using the Simultaneous RFID Tag Reader (SRTR) shield, make sure the serial slide
  switch is in the 'SW-UART' position
*/

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

#include <Arduino.h>   // required before wiring_private.h
#include "wiring_private.h" // pinPeripheral() function

// Which pin on the Arduino is connected to the NeoPixels?
// On a Trinket or Gemma we suggest changing this to 1
#define PIN            6

// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS      1

//What is the Threshold for dry?
#define THRESHOLD_DRY     15

//What is the Threshold for wet?
#define THRESHOLD_MEDIUM     10

//What length is the desired EPC array?
#define EPC_LENGTH    100

//How much to flash white during time a disconnect?
#define FLASHAMOUNT   10

// When we setup the NeoPixel library, we tell it how many pixels, and which pin to use to send signals.
// Note that for older NeoPixel strips you might need to change the third parameter--see the strandtest
// example for more information on possible values.
Adafruit_NeoPixel pixel = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

int delayval = 500; // delay for half a second

#include "SparkFun_UHF_RFID_Reader.h" //Library for controlling the M6E Nano module
RFID nano; //Create instance

void setup()
{
  delay(5000);
  pixel.begin(); // This initializes the NeoPixel library.
  pixel.setPixelColor(0, 0, 150, 0);
  pixel.show();
  delay(1000);
  
  Serial1.begin(38400);
  Serial.begin(115200);
  //while (!Serial); //Wait for the serial port to come online

  pixel.setPixelColor(0, 150, 0, 0);
  pixel.show();
  delay(1000);
  
  if (setupNano(38400) == false) //Configure nano to run at 38400bps
  {
    pixel.setPixelColor(0, 0, 0, 150);
    pixel.show();
    delay(100);
    Serial.println(("Module failed to respond. Please check wiring."));
    while (1); //Freeze!
  }
  
  pixel.setPixelColor(0, 150, 150, 0);
  pixel.show();  
  
  nano.setRegion(REGION_NORTHAMERICA); //Set to North America

  pixel.setPixelColor(0, 150, 0, 150);
  pixel.show();
  delay(1000);
  
  nano.setReadPower(1300); //5.00 dBm. Higher values may caues USB port to brown out
  //Max Read TX Power is 27.00 dBm and may cause temperature-limit throttling

  //Serial.println(F("Press a key to begin scanning for tags."));
  //while (!Serial.available()); //Wait for user to send a character
  //Serial.read(); //Throw away the user's character

  pixel.setPixelColor(0, 0, 150, 150);
  pixel.show();
  delay(1000);
  
  nano.startReading(); //Begin scanning for tags
}

void loop()
{


  if (nano.check() == true) //Check to see if any new data has come in from module
  {
    byte responseType = nano.parseResponse(); //Break response into tag ID, RSSI, frequency, and timestamp

    if (responseType == RESPONSE_IS_KEEPALIVE)
    {
      Serial.println(F("Scanning"));
    }
    else if (responseType == RESPONSE_IS_TAGFOUND)
    {
      //If we have a full record we can pull out the fun bits
      int rssi = nano.getTagRSSINew(); //Get the RSSI for this tag read

      long freq = nano.getTagFreqNew(); //Get the frequency this tag was detected at


      long moisture = nano.getMoistureData();
      //long timeStamp = nano.getTagTimestamp(); //Get the time this was read, (ms) since last keep-alive message

    
      byte tagEPCBytes = nano.getTagEPCBytesNew(); //Get the number of bytes of EPC from response

      if(freq < 1000000 && nano.getAntennaeIDNew() == 17){


          if(moisture >= THRESHOLD_DRY) 
      {
        pixel.setPixelColor(0, 150, 0, 0); // Moderately bright red color.
        pixel.show(); // This sends the updated pixel color to the hardware.
      }
      else if(moisture >= THRESHOLD_MEDIUM)
      {
        pixel.setPixelColor(0, 0, 150, 0); // Moderately bright green color.
        pixel.show(); // This sends the updated pixel color to the hardware.  
      }
      else
      {
        pixel.setPixelColor(0, 0, 0, 150); //Moderately bright blue color.
        pixel.show(); //This sends the updated pixel color to the hardware.
      }


         Serial.print(F(" rssi["));
        Serial.print(rssi);
        Serial.print(F("]"));

        Serial.print(F(" freq["));
        Serial.print(freq);
        Serial.print(F("]"));

        Serial.print(F(" Moisture["));
        Serial.print(moisture);
        Serial.print(F("]"));

      //Print EPC bytes, this is a subsection of bytes from the response/msg array
        Serial.print(F(" epc["));
        for (byte x = 0 ; x < tagEPCBytes ; x++)
        {
         if (nano.msg[31 + x] < 0x10) Serial.print(F("0")); //Pretty print
          Serial.print(nano.msg[37 + x], HEX);
          Serial.print(F(" "));
        }
        Serial.print(F("]"));

        Serial.println();
      }

     
    }
    else if (responseType == ERROR_CORRUPT_RESPONSE)
    {
      Serial.println("Bad CRC");
    }
    else
    {
      //Unknown response
      Serial.println("Unknown error");
    }
  }
}

//Gracefully handles a reader that is already configured and already reading continuously
//Because Stream does not have a .begin() we have to do this outside the library
boolean setupNano(long baudRate)
{
  nano.begin(Serial1); //Tell the library to communicate over hardware Serial1 port

  pixel.setPixelColor(0, 150, 0, 150);
  pixel.show();
  delay(1000);
  
  //Test to see if we are already connected to a module
  //This would be the case if the Arduino has been reprogrammed and the module has stayed powered
  Serial1.begin(baudRate); //For this test, assume module is already at our desired baud rate
  //while(!Serial1); //Wait for port to open

  //About 200ms from power on the module will send its firmware version at 115200. We need to ignore this.
  while(Serial1.available()) Serial1.read();

  pixel.setPixelColor(0, 0, 150, 150);
  pixel.show();
  delay(1000);
  
  nano.getVersion();

  pixel.setPixelColor(0, 150, 150, 150);
  pixel.show();
  delay(1000);

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
    Serial1.begin(115200); //Start Serial1 at 115200

    nano.setBaud(baudRate); //Tell the module to go to the chosen baud rate. Ignore the response msg

    Serial1.begin(baudRate); //Start the Serial1 port, this time at user's chosen baud rate
  }

  //Test the connection
  nano.getVersion();
  if (nano.msg[0] != ALL_GOOD) return (false); //Something is not right

  //The M6E has these settings no matter what
  nano.setTagProtocol(); //Set protocol to GEN2

  nano.setAntennaPort(); //Set TX/RX antenna ports to 1

  return (true); //We are ready to rock
}
