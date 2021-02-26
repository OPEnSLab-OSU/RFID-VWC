/*
  Reading multiple RFID tags, simultaneously!
  By: Nathan Seidle @ SparkFun Electronics
  Date: October 3rd, 2016
  https://github.com/sparkfun/Simultaneous_RFID_Tag_Reader

  Write a new EPC (Electronic Product Code) to a tag
  This is a good way to assign your own, easy to read ID to a tag.
  Most tags have 12 bytes available for EPC

  EPC is good for things like UPC (this is a gallon of milk)
  User data is a good place to write things like the milk's best by date
*/

#include <Arduino.h>

#include "wiring_private.h" // pinPeripheral() function


#include "SparkFun_UHF_RFID_Reader.h" //Library for controlling the M6E Nano module

//Tx ---> 10
//Rx ---> 11
Uart rfidSerial(&sercom1, 11, 10, SERCOM_RX_PAD_0, UART_TX_PAD_2 );

void SERCOM1_Handler() {
  rfidSerial.IrqHandler();
}


RFID nano; //Create instance

void setup()
{
  Serial.begin(115200);

  while (!Serial);
  Serial.println();
  Serial.println("Initializing...");

  rfidSerial.begin(115200);
  
  pinPeripheral(10, PIO_SERCOM);
  pinPeripheral(11, PIO_SERCOM);

  while(!rfidSerial);

  delay(1000);

  StartOver:

  if (setupNano(115200) == false) //Configure nano to run at 38400bps
  {
    Serial.println("Module failed to respond. Please check wiring.");
    delay(10000);
    goto StartOver;
  }

  nano.setRegion(REGION_NORTHAMERICA); //Set to North America

  nano.setReadPower(1500); //5.00 dBm. Higher values may cause USB port to brown out
  //Max Read TX Power is 27.00 dBm and may cause temperature-limit throttling

  nano.setWritePower(1700); //5.00 dBm. Higher values may cause USB port to brown out
  //Max Write TX Power is 27.00 dBm and may cause temperature-limit throttling
}

void loop()
{
  Serial.println(F("Get all tags out of the area. Press a key to write EPC to first detected tag."));
  while (!Serial.available()); //Wait for user to send a character
  Serial.read(); //Throw away the user's character

  //"Hello" Does not work. "Hell" will be recorded. You can only write even number of bytes
 // char stringEPC[] = "Hello!"; //You can only write even number of bytes
 // byte responseType = nano.writeTagEPC(stringEPC, sizeof(stringEPC) - 1); //The -1 shaves off the \0 found at the end of string

  char hexEPC[] = {0xAA, 0x2D, 0x03, 0x54}; //You can only write even number of bytes
  byte responseType = nano.writeTagEPC(hexEPC, sizeof(hexEPC));

  if (responseType == RESPONSE_SUCCESS)
    Serial.println("New EPC Written!");
  else
    Serial.println("Failed write");
}

//Gracefully handles a reader that is already configured and already reading continuously
//Because Stream does not have a .begin() we have to do this outside the library
boolean setupNano(long baudRate)
{
  
  nano.enableDebugging();
  nano.begin(rfidSerial); //Tell the library to communicate over software serial port

  //Test to see if we are already connected to a module
  //This would be the case if the Arduino has been reprogrammed and the module has stayed powered
  //rfidSerial.begin(baudRate); //For this test, assume module is already at our desired baud rate
  while(!rfidSerial); //Wait for port to open

  //About 200ms from power on the module will send its firmware version at 115200. We need to ignore this.
  while(rfidSerial.available()) rfidSerial.read();
  
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
    //rfidSerial.begin(115200); //Start software serial at 115200

    nano.setBaud(baudRate); //Tell the module to go to the chosen baud rate. Ignore the response msg

    //rfidSerial.begin(baudRate); //Start the software serial port, this time at user's chosen baud rate
  }

  //Test the connection
  nano.getVersion();
  if (nano.msg[0] != ALL_GOOD) return (false); //Something is not right

  //The M6E has these settings no matter what
  nano.setTagProtocol(); //Set protocol to GEN2

  nano.setAntennaPort(); //Set TX/RX antenna ports to 1

  nano.disableDebugging();

  return (true); //We are ready to rock
}
