/**
 *  AlgoIoT example for ESP32
 * 
 *  Example for "AlgoIoT", Algorand lightweight library for ESP32
 * 
 *  Last mod 20231205-1
 *
 *  By Fernando Carello for GT50
 *  Released under Apache license
 *  Copyright 2023 GT50 S.r.l.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.*/



// TODO:
//

#include <WiFi.h>
#include <WiFiMulti.h>
#include <AlgoIoT.h>

// Please edit accordingly
#define MYWIFI_SSID "My_SSID" 
#define MYWIFI_PWD "My_Password"

#define DAPP_NAME "AlgoIoT_MyTest1" // Keep it short; 31 chars = absolute max length
#define NODE_ACCOUNT_MNEMONICS "shadow market lounge gauge battle small crash funny supreme regular obtain require control oil lend reward galaxy tuition elder owner flavor rural expose absent sniff"  
#define RECEIVER_ADDRESS "" 				// Leave "" to send to self or insert a valid Algorand destination address
#define USE_TESTNET	// Comment out to use Mainnet  *** BEWARE: Mainnet is the "real thing" and will cost you real Algos! ***

#define NODE_SERIAL_NUMBER 1234567890UL
#define SN_LABEL "NodeSerialNum"
#define LAT_LABEL "Lat"
#define LON_LABEL "Lon"
#define ALT_LABEL "Alt(m)"
#define T_LABEL "Temperature(°C)"
#define H_LABEL "RelHumidity(%)"
#define P_LABEL "Pressure(mbar)"

// Comment out if a BME280 module is connected via I2C to the ESP32 board
#define FAKE_SENSOR

#ifdef FAKE_SENSOR
// Assign your fake data of choice here
const uint8_t FAKE_H_PCT = 55;
const float FAKE_T_C = 25.5f;
const uint16_t FAKE_P_MBAR = 1018;
#else
#define SDA_PIN 21	// Sensor module connection
#define SCL_PIN 22	// Edit accordingly to your board/wiring
#include <Bme280.h> // https://github.com/malokhvii-eduard/arduino-bme280
#endif


// Uncomment to get debug prints on Serial Monitor
#define SERIAL_DEBUGMODE


#define DEBUG_SERIAL Serial

#define WIFI_RETRY_DELAY_MS 1000
#define DATA_SEND_INTERVAL (( DATA_SEND_INTERVAL_MINS ) * 60 * 1000UL) 


// Globals
AlgoIoT g_algoIoT(DAPP_NAME, NODE_ACCOUNT_MNEMONICS);
WiFiMulti g_wifiMulti;
#ifndef FAKE_SENSOR
Bme280TwoWire g_BMEsensor;
#endif
// End globals



/////////////////////////
//
// Forward Declarations
//
/////////////////////////

void waitForever();

// Read sensors data (real of fake depending on #define in user_config.h)
// Returns error code (0 = OK)
int readSensors(float* temperature_C, uint8_t* relhum_Pct, uint16_t* pressure_mbar);


// Algorand-related functions



//////////
// SETUP
//////////

void setup() 
{  
  int iErr = 0;

  #ifdef SERIAL_DEBUGMODE
  DEBUG_SERIAL.begin(115200);
  while (!DEBUG_SERIAL)
  {    
  }
  delay(1000);
  DEBUG_SERIAL.println();
  #endif
  
  delay(500);

  // Configure WiFi
  g_wifiMulti.addAP(MYWIFI_SSID, MYWIFI_PWD);

  // Initialize T/H/P sensor
  #ifndef FAKE_SENSOR
  Wire.begin(SDA_PIN, SCL_PIN);
  g_BMEsensor.begin(Bme280TwoWireAddress::Primary);
  g_BMEsensor.setSettings(Bme280Settings::indoor());
  #endif    

  // Change data receiver address and Algorand network type if needed
  if (RECEIVER_ADDRESS != "")
  {
    iErr = g_algoIoT.setDestinationAddress(RECEIVER_ADDRESS);
    if (iErr != ALGOIOT_NO_ERROR)
    {
      #ifdef SERIAL_DEBUGMODE
      DEBUG_SERIAL.printf("\n Error %d setting receiver address %s: please check it\n\n", iErr, RECEIVER_ADDRESS);
      #endif

      waitForever();
    }
  }

  #ifndef USE_TESTNET
  iErr = g_algoIoT.setAlgorandNetwork(ALGORAND_MAINNET);
  if (iErr != ALGOIOT_NO_ERROR)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.printf("\n Error %d setting Algorand network type to Mainnet: please report!\n\n", iErr);
    #endif

    waitForever();
  }
  #endif
}


/////////
// LOOP
/////////

void loop() 
{
  uint16_t notesLen = 0;
  uint32_t currentMillis = millis();
  uint8_t rhPct = 0;
  float tempC = 0.0f;
  uint16_t pmbar = 0;
  float latitude = 0.0f;
  float longitude = 0.0f;
  int16_t altitude_m = 0;

  // Check for WiFi connection
  #ifdef SERIAL_DEBUGMODE
  DEBUG_SERIAL.print("Trying to connect to WiFi network "); DEBUG_SERIAL.println(MYWIFI_SSID); DEBUG_SERIAL.println();
  #endif
  
  if((g_wifiMulti.run() == WL_CONNECTED)) 
  {
    int iErr = 0;

    #ifdef SERIAL_DEBUGMODE
      DEBUG_SERIAL.print("Connected to "); DEBUG_SERIAL.println(MYWIFI_SSID); DEBUG_SERIAL.println();
    #endif

    iErr = readSensors(&tempC, &rhPct, &pmbar);
    if (iErr)
    {
      #ifdef SERIAL_DEBUGMODE
      DEBUG_SERIAL.printf("Error %d reading sensors\n", iErr);
      #endif
    }
    else
    { // readsensors OK
      // Now we format our data in a format suitable to be submitted as a Note field
      // of an Algorand payment transaction. This way, our data will be written in the blockchain
      // The Note field of an Algorand transaction is quite short and there is some format overhead
      // (JSON), so we need to keep data and labels as short as possible

      #ifdef SERIAL_DEBUGMODE
      DEBUG_SERIAL.println("Data OK, ready to be encoded\n");
      #endif

      // Add node serial number
      iErr = g_algoIoT.dataAddUInt32Field(SN_LABEL, NODE_SERIAL_NUMBER);
      if (iErr)
      {
        #ifdef SERIAL_DEBUGMODE
        DEBUG_SERIAL.printf("Error %d adding Serial Number field\n", iErr);
        #endif
        waitForever();
      }

      // Add sensor data
      iErr = g_algoIoT.dataAddFloatField(T_LABEL, tempC);
      if (iErr)
      {
        #ifdef SERIAL_DEBUGMODE
        DEBUG_SERIAL.printf("Error %d adding Temperature field\n", iErr);
        #endif
        waitForever();
      }
      iErr = g_algoIoT.dataAddUInt8Field(H_LABEL, rhPct);
      if (iErr)
      {
        #ifdef SERIAL_DEBUGMODE
        DEBUG_SERIAL.printf("Error %d adding Humidity field\n", iErr);
        #endif
        waitForever();
      }
      iErr = g_algoIoT.dataAddInt16Field(P_LABEL, pmbar);
      if (iErr)
      {
        #ifdef SERIAL_DEBUGMODE
        DEBUG_SERIAL.printf("Error %d adding Pressure field\n", iErr);
        #endif
        waitForever();
      }

      // Data added to structure. Now we can submit our transaction to the blockchain
      iErr = g_algoIoT.submitTransactionToAlgorand();
      if (iErr)
      {
        #ifdef SERIAL_DEBUGMODE
        DEBUG_SERIAL.printf("Error %d submitting transaction to Algorand blockchain\n", iErr);
        #endif
        waitForever();
      }
    }
    // Wait for next data upload
    delay(DATA_SEND_INTERVAL);
  }
  // WiFi connection not established, wait a bit and retry
  delay(WIFI_RETRY_DELAY_MS);
}


////////////////////
//
// Implementations
//
////////////////////

void waitForever()
{
  while(1)
    delay(ULONG_MAX);
}


int readSensors(float* temperature_C, uint8_t* relhum_Pct, uint16_t* pressure_mbar)
{
  #ifdef FAKE_SENSOR
  *temperature_C = FAKE_T_C;  // Dummy values assigned in user_config.h
  *relhum_Pct = FAKE_H_PCT;
  *pressure_mbar = FAKE_P_MBAR;
  #else
  *temperature_C = g_BMEsensor.getTemperature();  
  *pressure_mbar = (uint16_t) (0.01f * g_BMEsensor.getPressure());
  *relhum_Pct = (uint8_t)(g_BMEsensor.getHumidity() + 0.5f);  // Round to largest int (H is always > 0)
  #endif

  return 0;
}
