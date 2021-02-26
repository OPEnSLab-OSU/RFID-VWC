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
#include "wiring_private.h" // pinPeripheral() function
#include "SparkFun_UHF_RFID_Reader.h" //Library for controlling the M6E Nano module

#define NUMPIXELS 1
#define WET 16
#define DRY 21
#define RSSI_THRESHOLD -90
#define RSSI_THRESHOLD_RELAY -80

#define RELAY_SWITCH A0

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, A1, NEO_GRB + NEO_KHZ800);

//Tx ---> 10
//Rx ---> 11
Uart rfidSerial(&sercom1, 11, 10, SERCOM_RX_PAD_0, UART_TX_PAD_2 );

void SERCOM1_Handler() {
  rfidSerial.IrqHandler();
}

RFID nano; //Create instance
int counter = 0;

void setup()
{

  
  pinMode(5, OUTPUT);   // Enable control of 3.3V rail 
  pinMode(6, OUTPUT);   // Enable control of 5V rail 

  digitalWrite(5, LOW);  // Enable 3.3V rail
  digitalWrite(6, HIGH);  // Enable 5V rail
  
  StartOver:
  pixels.begin();
  setColor(-1, -1);
  pinMode(RELAY_SWITCH, OUTPUT);
  Serial.begin(115200);
  delay(500);
  //while (!Serial); //Wait for the serial port to come online

  rfidSerial.begin(115200);
  
  pinPeripheral(10, PIO_SERCOM);
  pinPeripheral(11, PIO_SERCOM);

  while(!rfidSerial);

  delay(1000);


  
    if (setupNano(115200) == false) //Configure nano to run at 38400bps
    {
      Serial.println(F("Module failed to respond. Please check wiring."));
      for(int j = 0; j < 5; j ++){
        for(int i = 0; i < NUMPIXELS; i++){
          pixels.setPixelColor(i, pixels.Color(150, 150, 150));
          pixels.show();  
        }
        delay(300);
        for(int i = 0; i < NUMPIXELS; i++){
          pixels.setPixelColor(i, pixels.Color(0, 0, 0));
          pixels.show();  
        }
        delay(300);
      }
      //while (1); //Freeze!

      delay(10000);
      goto StartOver;
    }
  //}
  
  nano.setRegion(REGION_NORTHAMERICA); //Set to North America

  nano.setReadPower(2000); //5.00 dBm. Higher values may caues USB port to brown out
  //Max Read TX Power is 27.00 dBm and may cause temperature-limit throttling

  //Serial.println(F("Press a key to begin scanning for tags."));
  //while (!Serial.available()); //Wait for user to send a character
  //Serial.read(); //Throw away the user's character

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
      setColor(-1, -1);
    }
    else if (responseType == RESPONSE_IS_TAGFOUND)
    {
      //If we have a full record we can pull out the fun bits
      int rssi          = nano.getTagRSSINew(); //Get the RSSI for this tag read
      long freq         = nano.getTagFreqNew(); //Get the frequency this tag was detected at
      long moisture     = nano.getMoistureData();
      byte tagEPCBytes  = nano.getTagEPCBytesNew(); //Get the number of bytes of EPC from response
      byte EPCHeader    = nano.getEPCHeader();
      byte EPCFertigate = nano.getFertigateTag();

      if(freq < 1000000 && nano.getAntennaeIDNew() == 17 && tagEPCBytes <= 20){

        counter = 0;

        setColor(moisture, rssi);

		  if(rssi > RSSI_THRESHOLD_RELAY)
        		water_on();
        else{
        		delay(300);
        		water_off();
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

        Serial.print(F(" epc["));
        for (byte x = 0 ; x < tagEPCBytes ; x++)
        {
         if (nano.msg[31 + x] < 0x10) Serial.print(F("0")); //Pretty print
          Serial.print(nano.msg[37 + x], HEX);
          Serial.print(F(" "));
          
        }
        Serial.println(F("]"));

      }
      else{
        counter++;
        if(counter > 10){
            setColor(-1, -1);
            water_off();
        }
      }

      //Print EPC bytes, this is a subsection of bytes from the response/msg array
      /*
      Serial.print(F(" epc["));
      for (byte x = 0 ; x < tagEPCBytes ; x++)
      {
        if (nano.msg[31 + x] < 0x10) Serial.print(F("0")); //Pretty print
        Serial.print(nano.msg[31 + x], HEX);
        Serial.print(F(" "));
      }*/
      //Serial.print(F("]"));

      //Serial.println();
    }
    else if (responseType == ERROR_CORRUPT_RESPONSE)
    {
      Serial.println("Bad CRC");
    }
    else
    {
      //Unknown response
      Serial.print("Unknown error");
      setColor(-1, -1);
    }
  }
}

//Gracefully handles a reader that is already configured and already reading continuously
//Because Stream does not have a .begin() we have to do this outside the library
boolean setupNano(long baudRate)
{

  delay(200);
  nano.enableDebugging();
  //rfidSerial.end();//begin(baudRate); //For this test, assume module is already at our desired baud rate
  //rfidSerial.begin(baudRate);

  nano.begin(rfidSerial);
//  if(!nano.begin(rfidSerial)) //Tell the library to communicate over Uart
//    Serial.println("failed to init");
//  else{
//    Serial.println("System initialized");
//  }

  //Test to see if we are already connected to a module
  //This would be the case if the Arduino has been reprogrammed and the module has stayed powered
  while(!rfidSerial); //Wait for port to open

  //About 200ms from power on the module will send its firmware version at 115200. We need to ignore this.
  
   while(rfidSerial.available()) rfidSerial.read();
   delay(100);
  
  Serial.println("Getting Version...");
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
    //rfidSerial.begin(115200);
    //while(!rfidSerial);
    Serial.println("Setting Baud...");
    nano.setBaud(baudRate); //Tell the module to go to the chosen baud rate. Ignore the response msg

   // rfidSerial.begin(baudRate); //Start the software serial port, this time at user's chosen baud rate
  }

  //Test the connection
  Serial.println("Getting Version...");
  nano.getVersion();

  delay(200);

  nano.getVersion();
  
  if (nano.msg[0] != ALL_GOOD){
    Serial.println(nano.msg[0], HEX);
    return (false); //Something is not right
  }

  Serial.println(nano.msg[0], HEX);
 
  Serial.println("Setting Tag Protocol...");
  //The M6E has these settings no matter what
  nano.setTagProtocol(); //Set protocol to GEN2

  Serial.println("Setting Antenna Port...");
  nano.setAntennaPort(); //Set TX/RX antenna ports to 1

  nano.disableDebugging();

  return (true); //We are ready to rock
}

void setColor(int moisture, int rssi){

  if(moisture >= 0 && rssi >= RSSI_THRESHOLD){
  
      if(moisture <= WET){
           for(int i = 0; i < NUMPIXELS; i++){
              pixels.setPixelColor(i, pixels.Color(0, 0, 150));
              pixels.show();  
           }
      
      }
      else if(moisture < DRY){
        
           for(int i = 0; i < NUMPIXELS; i++){
              pixels.setPixelColor(i, pixels.Color(0, 150, 0));
              pixels.show();  
           }
        
      }
      else{
          for(int i = 0; i < NUMPIXELS; i++){
              pixels.setPixelColor(i, pixels.Color(150, 0, 0));
              pixels.show();  
           }  
        
      }
  }
  else{

       for(int i = 0; i < NUMPIXELS; i++){
          pixels.setPixelColor(i, pixels.Color(0, 0, 0));
          pixels.show();  
       }
    
  }
  
  
}

void water_on(){

	digitalWrite(RELAY_SWITCH, HIGH);
	
}

void water_off(){
  	digitalWrite(RELAY_SWITCH, LOW);
}
