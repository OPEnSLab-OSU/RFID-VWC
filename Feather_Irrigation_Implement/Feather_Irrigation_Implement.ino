/*
  Reading multiple RFID tags, simultaneously!
  By: Nathan Seidle @ SparkFun Electronics
  Appended By: Matt Guo
  Date: March 4th, 2020
  https://github.com/sparkfun/Simultaneous_RFID_Tag_Reader

  Constantly reads and outputs any tags heard

  If using the Simultaneous RFID Tag Reader (SRTR) shield, make sure the serial slide
  switch is in the 'SW-UART' position
*/

//TODO: clean up relay_switch function so that toggling is not necessary for each iteration
//		  of state machine.
//TODO: test fertigate functionality

//#include <Loom.h>
#include <Adafruit_NeoPixel.h>        // Library for NeoPixel functionality
#include "wiring_private.h"           // pinPeripheral() function
#include "SparkFun_UHF_RFID_Reader.h" // Library for controlling the M6E Nano module

#define NEOPIXEL A1
#define RELAY_SWITCH A0
#define UART_TX 10
#define UART_RX 11
#define FERTIGATE_SWITCH A2
#define RELAY_RSSI_THRESHOLD -80

#define NUMPIXELS 1
#define NUM_TAGS 10
#define RSSI_THRESHOLD -60
#define WET 16
#define DRY 21
#define MAX_VALUE 26
#define MIN_VALUE 17

bool _debug_serial = true;      //global bool for serial debug mode

enum state_enum {
   COMPARE_THRESHOLD,
   WET_CYCLE,
   DRY_CYCLE,
};

//struct for tag object, stores tag EPC, fertigates,
//moisture value, frequency of communication, etc
typedef struct{

   byte EPCHeaderName;
   byte fertigate;
   uint8_t threshold;
   uint8_t state = COMPARE_THRESHOLD;
   int rssi;
   long freq;
   long moisture;
   byte tagEPCBytes;
    
} tag;

tag tag_array[NUM_TAGS];      //global array for tag objects
int current_num_tags = 0;     //global counter for current number of tags
uint8_t fertigate_state = 0;	//global variable for the state of fertigate pass

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, NEOPIXEL, NEO_GRB + NEO_KHZ800);


//const char* json_config = 
//#include "config.h"
//;
//// Set enabled modules
//LoomFactory<
//  Enable::Internet::Disabled,
//  Enable::Sensors::Enabled,
//  Enable::Radios::Disabled,
//  Enable::Actuators::Disabled,
//  Enable::Max::Disabled
//> ModuleFactory{};
//
//LoomManager Loom{ &ModuleFactory };


//Tx ---> 10
//Rx ---> 11
Uart rfidSerial(&sercom1, UART_RX, UART_TX, SERCOM_RX_PAD_0, UART_TX_PAD_2 );

void SERCOM1_Handler() {
   rfidSerial.IrqHandler();
}

RFID nano; //Create instance
char color[10];
char EPCHeader_Hex[3];
int timeout_counter = 0;

int rssi;
long freq;
long moisture;
byte tagEPCBytes;
byte EPCHeader;
byte EPCFertigate;

void setup()
{

//  Loom.begin_serial(true);
//  Loom.parse_config(json_config);
//  Loom.print_config();
//
//  LPrintln("\n ** Setup Complete ** ");

	pinMode(RELAY_SWITCH, OUTPUT);
	pinMode(FERTIGATE_SWITCH, INPUT_PULLUP);
  
	pixels.begin();
	setColor(-1, -1);
	Serial.begin(115200);
	//while (!Serial); //Wait for the serial port to come online
	
	rfidSerial.begin(115200);
	
	pinPeripheral(10, PIO_SERCOM);
	pinPeripheral(11, PIO_SERCOM);
	
	while(!rfidSerial);
	
	delay(1000);
	
	StartOver:
	
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
	
	   delay(10000);
	   goto StartOver;
	 }
	
	nano.setRegion(REGION_NORTHAMERICA); //Set to North America
	
	nano.setReadPower(2600); //5.00 dBm. Higher values may caues USB port to brown out
	//Max Read TX Power is 27.00 dBm and may cause temperature-limit throttling
	
	//Serial.println(F("Press a key to begin scanning for tags."));
	//while (!Serial.available()); //Wait for user to send a character
	//Serial.read(); //Throw away the user's character
	
	nano.startReading(); //Begin scanning for tags
}

//Gracefully handles a reader that is already configured and already reading continuously
//Because Stream does not have a .begin() we have to do this outside the library
boolean setupNano(long baudRate)
{

	delay(200);
	nano.enableDebugging();
	
	nano.begin(rfidSerial);
	
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

//main loop of the program, calls the state machine for individual
//tags upon successive tag responses
void loop()
{
	//readFertigateSwitch();
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
      rssi          = nano.getTagRSSINew(); 		//Get the RSSI for this tag read
      freq          = nano.getTagFreqNew(); 		//Get the frequency this tag was detected at
      tagEPCBytes   = nano.getTagEPCBytesNew(); //Get the number of bytes of EPC from response
      EPCHeader     = nano.getEPCHeader();		//Get the EPC header for identification
      EPCFertigate  = nano.getFertigateTag();	//Get the EPC fertigate code
      moisture      = nano.getMoistureData();	//Get the moisture data of the current read

      if(freq < 1000000 && nano.getAntennaeIDNew() == 17 && tagEPCBytes <= 20){

//      	if(_debug_serial){
//      		Serial.print("Fertigate State: ");
//      		Serial.println(fertigate_state);	
//      	}

			//checking the fertigate pass
			//means that current sweep is not fertigate sweep
      	//if(!fertigate_state){

      		 for(int i = 0; i <= current_num_tags; i++){
		          if(tag_array[i].EPCHeaderName == EPCHeader){
		             //run state machine at i
		             updateData(i);
		             state_machine(i);  
		          }
		          //no tag found, add in this new tag
		          else if(i == current_num_tags){
		            if(current_num_tags < NUM_TAGS){
		              addNewTag(i);
		              current_num_tags++;
		            }
		          }
	        }
      		
      	//}
      	//we are in the fertigate sweep, do not call state machine on non-fertigate tags
//      	else{
//
//      			for(int i = 0; i <= current_num_tags; i++){
//      				if(tag_array[i].EPCHeaderName == EPCHeader && tag_array[i].fertigate == 0xFF){
//      					updateData(i);
//      					state_machine(i);	
//      				}
//      				//no tag found, add in this new tag
//      				else if(i == current_num_tags){
//      					if(current_num_tags < NUM_TAGS){
//      						addNewTag(i);
//      						current_num_tags++;	
//      					}	
//      				}
//      			}
//      		
//      	}

        //reset timeout
        timeout_counter = 0;

        delay(100);

        setColor(moisture, rssi);


        itoa(EPCHeader, EPCHeader_Hex, 16); 

//        Loom.measure();
//        Loom.package();
//        Loom.display_data();
//
//        Loom.add_data("RFID", "RSSI", rssi);
//        Loom.add_data("Frequency", "Frequency", freq);
//        Loom.add_data("Moisture", "Moisture", moisture);
//        Loom.add_data("EPC", "EPC", EPCHeader_Hex);
//        Loom.add_data("Color", "Color", color);
//        Loom.SDCARD().log();
//        Loom.pause();

      }
      else{
      	//implement timeout function
        timeout_counter++;
        if(timeout_counter > 10){
          setColor(-1, -1);
        }
      }

    }
    else if (responseType == ERROR_CORRUPT_RESPONSE)
    {
      Serial.println("Bad CRC");
    }
    else
    {
      //Unknown response
      Serial.println("Scanning...");
    }
  }
}

//Takes inputs of moisture and rssi values, sets the color on the NeoPixel
//accordingly.
//NOTE: DEBUG FUNCTION ONLY
void setColor(int moisture, int rssi){
  if(moisture >= 0 && rssi >= RSSI_THRESHOLD){
      if(moisture <= WET){
           for(int i = 0; i < NUMPIXELS; i++){
              pixels.setPixelColor(i, pixels.Color(0, 0, 150));
              pixels.show();
              strcpy(color, "blue");  
           }
      }
      else if(moisture < DRY){
           for(int i = 0; i < NUMPIXELS; i++){
              pixels.setPixelColor(i, pixels.Color(0, 150, 0));
              pixels.show();
              strcpy(color, "green");  
           }
      }
      else{
          for(int i = 0; i < NUMPIXELS; i++){
              pixels.setPixelColor(i, pixels.Color(150, 0, 0));
              pixels.show();
              strcpy(color, "red");;  
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

//Create a new struct tag object with a default initialization
//setup, then store the information to the tag array
void addNewTag(int i){
   i++;
   tag newTag;
   newTag.EPCHeaderName = EPCHeader;
   newTag.fertigate     = EPCFertigate;
   newTag.threshold     = MIN_VALUE;
   newTag.state         = COMPARE_THRESHOLD;
   
   tag_array[i] = newTag;
}

//Update the current tag object array to the most current values
//from the irrigation sweep
void updateData(int i){
   tag_array[i].fertigate   = EPCFertigate;
   tag_array[i].rssi        = rssi;
   tag_array[i].moisture    = moisture;
   tag_array[i].freq        = freq;
   tag_array[i].tagEPCBytes = tagEPCBytes;
}

//function that turns on the relay switch by pulsing high on
//the GPIO
void turnOnRelay(){
	digitalWrite(RELAY_SWITCH, HIGH);
}

void turnOffRelay(){
	digitalWrite(RELAY_SWITCH, LOW);
	
}

//void readFertigateSwitch(){
//	if(digitalRead(FERTIGATE_SWITCH) == HIGH){
//		fertigate_state = 1;	
//	}
//	else{
//		fertigate_state = 0;	
//	}
//}

//Irrigation state machine with three states: compare the threshold
//wet cycle (for continuous watering until low threshold is met), and dry cycle
//(for continuous drying until high threshold is met).
void state_machine(int index){

  switch(tag_array[index].state){

    case COMPARE_THRESHOLD:
    
       if(_debug_serial){
          Serial.print("EPC Tag: ");
          Serial.print(tag_array[index].EPCHeaderName, HEX);
          Serial.println(" COMPARE_THRESHOLD state");
        
       }

       if(tag_array[index].threshold == MAX_VALUE){
          if(_debug_serial){
             Serial.println("Transitioning to: DRY_CYCLE");
             Serial.println();
          }
          tag_array[index].state = DRY_CYCLE;
       }
       else if(tag_array[index].threshold == MIN_VALUE){
          if(_debug_serial){
             Serial.println("Transitioning to: WET_CYCLE");
             Serial.println();
          }
          tag_array[index].state = WET_CYCLE;
       }

       break;

     case WET_CYCLE:

      if(_debug_serial){
         Serial.print("EPC Tag: ");
         Serial.print(tag_array[index].EPCHeaderName, HEX);
         Serial.print(" Current RSSI: ");
         Serial.print(tag_array[index].rssi);
         Serial.print(" WET_CYCLE state ");
         Serial.print("Current moisture: ");
         Serial.print(tag_array[index].moisture);
         Serial.print(" Threshold moisture: ");
         Serial.println(tag_array[index].threshold);
         Serial.println(); 
      }
     
       if(tag_array[index].moisture > tag_array[index].threshold){
       	 if(tag_array[index].rssi > RELAY_RSSI_THRESHOLD){
          	turnOnRelay();
          	tag_array[index].state = COMPARE_THRESHOLD;
       	 }
       	 else
       	 	turnOffRelay();
       }
       else if(tag_array[index].moisture <= tag_array[index].threshold){
          tag_array[index].threshold = MAX_VALUE;
          tag_array[index].state = COMPARE_THRESHOLD;
          turnOffRelay();
       }

       break;

     case DRY_CYCLE:

        if(_debug_serial){
           Serial.print("EPC Tag: ");
           Serial.print(tag_array[index].EPCHeaderName, HEX);
           Serial.print(" Current RSSI: ");
           Serial.print(tag_array[index].rssi);
           Serial.print(" DRY_CYCLE state ");
           Serial.print("Current moisture: ");
           Serial.print(tag_array[index].moisture);
           Serial.print(" Threshold moisture: ");
           Serial.println(tag_array[index].threshold);
           Serial.println(); 
        }
     
        if(tag_array[index].moisture > tag_array[index].threshold){
        	  if(tag_array[index].rssi > RELAY_RSSI_THRESHOLD){
        	  		tag_array[index].threshold = MIN_VALUE;
          		turnOnRelay();
          		tag_array[index].state = COMPARE_THRESHOLD;
        	  	
        	  }
          
        }
        else if(tag_array[index].moisture <= tag_array[index].threshold){
          tag_array[index].state = COMPARE_THRESHOLD;
          turnOffRelay();
        }
  }
}
