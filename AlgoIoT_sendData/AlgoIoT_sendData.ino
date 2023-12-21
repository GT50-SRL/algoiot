/**
 *  AlgoIoT example for ESP32
 * 
 *  Example for "AlgoIoT", Algorand lightweight library for ESP32
 * 
 *  Last mod 20231221-1
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



#include <WiFi.h>
#include <WiFiMulti.h>
#include <AlgoIoT.h>


///////////////////////////
// USER-DEFINED SETTINGS
///////////////////////////
// Please edit accordingly
#define MYWIFI_SSID "My_WiFi_SSID" 	
#define MYWIFI_PWD "My_WiFi_Password" 

// Assign a name to your IoT app:
#define DAPP_NAME "AlgoIoT_MyTest1" // Keep it short; 31 chars = absolute max length
// Mnemonic words (25 BIP-39 words) representing device account. Beware, this is a PRIVATE KEY and should not be shared! 
// Please replace demo mnemonic words with your own:
#define NODE_ACCOUNT_MNEMONICS "shadow market lounge gauge battle small crash funny supreme regular obtain require control oil lend reward galaxy tuition elder owner flavor rural expose absent sniff"  
#define RECEIVER_ADDRESS "" 				// Leave "" to send to self (default, no fee to be paid) or insert a valid Algorand destination address
#define USE_TESTNET	                // Comment out to use Mainnet  *** BEWARE: Mainnet is the "real thing" and will cost you real Algos! ***

// Assign your node serial number (will be added to Note data):
#define NODE_SERIAL_NUMBER 1234567890UL

// Sample labels for your data:
#define SN_LABEL "NodeSerialNum"
#define T_LABEL "Temperature(°C)"
#define H_LABEL "RelHumidity(%)"
#define P_LABEL "Pressure(mbar)"
#define LAT_LABEL "Latitude"
#define LON_LABEL "Longitude"
#define ALT_LABEL "Elevation(m)"

// Comment out if a BME280 module is connected via I2C to the ESP32 board
#define FAKE_TPH_SENSOR

#ifdef FAKE_TPH_SENSOR
// Assign your fake data of choice here
const uint8_t FAKE_H_PCT = 55;
const float FAKE_T_C = 25.5f;
const uint16_t FAKE_P_MBAR = 1018;
#else
#define SDA_PIN 21	// Sensor module connection
#define SCL_PIN 22	// Edit accordingly to your board/wiring
#include <Bme280.h> // https://github.com/malokhvii-eduard/arduino-bme280
#endif

// You may optionally specify sensor position here, or comment out
#define POS_LAT_DEG 41.905344f
#define POS_LON_DEG 12.444122f
#define POS_ALT_M 21

#define DATA_SEND_INTERVAL_MINS 60
#define WIFI_RETRY_DELAY_MS 1000

// Uncomment to get debug prints on Serial Monitor
#define SERIAL_DEBUGMODE

//////////////////////////////////
// END OF USER-DEFINED SETTINGS
//////////////////////////////////


#define DEBUG_SERIAL Serial
#define DATA_SEND_INTERVAL (( DATA_SEND_INTERVAL_MINS ) * 60 * 1000UL) 


// Globals
AlgoIoT g_algoIoT(DAPP_NAME, NODE_ACCOUNT_MNEMONICS);
WiFiMulti g_wifiMulti;
#ifndef FAKE_TPH_SENSOR
Bme280TwoWire g_BMEsensor;
#endif
// End globals



//////////////////////////////////////////////
//
// Forward Declarations for local functions
//
//////////////////////////////////////////////

void waitForever();

void initializeBME280();

// Read sensors data (real of fake depending on #define in user_config.h)
// Returns error code (0 = OK)
int readSensors(float* temperature_C, uint8_t* relhum_Pct, uint16_t* pressure_mbar);

// Get coordinates. We do not actually use a GPS: position is (optionally) specified by user (see above among defines)
int readPosition(float* lat_deg, float* lon_deg, int16_t* alt_m);



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


  #ifndef FAKE_TPH_SENSOR
  // Initialize T/H/P sensor
  initializeBME280();
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
      float lat = 0.0f;
      float lon = 0.0f;
      int16_t alt = 0;

      uint8_t positionNotSpecified = readPosition(&lat, &lon, &alt);

      if (!positionNotSpecified)
      { // Add user-defined position
        iErr = g_algoIoT.dataAddFloatField(LAT_LABEL, lat);
        if (iErr)
        {
          #ifdef TINYPICO
          // Turn on error LED if on TinyPICO
          g_tp.DotStar_SetPixelColor(LED_COLOR_RED);
          #endif
          #ifdef SERIAL_DEBUGMODE
          DEBUG_SERIAL.printf("Error %d adding Latitude field\n", iErr);
          #endif
          waitForever();
        }
        iErr = g_algoIoT.dataAddFloatField(LON_LABEL, lon);
        if (iErr)
        {
          #ifdef TINYPICO
          // Turn on error LED if on TinyPICO
          g_tp.DotStar_SetPixelColor(LED_COLOR_RED);
          #endif
          #ifdef SERIAL_DEBUGMODE
          DEBUG_SERIAL.printf("Error %d adding Longitude field\n", iErr);
          #endif
          waitForever();
        }
        iErr = g_algoIoT.dataAddInt16Field(ALT_LABEL, alt);
        if (iErr)
        {
          #ifdef TINYPICO
          // Turn on error LED if on TinyPICO
          g_tp.DotStar_SetPixelColor(LED_COLOR_RED);
          #endif
          #ifdef SERIAL_DEBUGMODE
          DEBUG_SERIAL.printf("Error %d adding Elevation field\n", iErr);
          #endif
          waitForever();
        }
      }

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

      #ifdef SERIAL_DEBUGMODE
      DEBUG_SERIAL.println("Data encoded, ready to be submitted to the blockchain\n");
      #endif

      // Data added to structure. Now we can submit our transaction to the blockchain
      iErr = g_algoIoT.submitTransactionToAlgorand();
      if (iErr)
      {
        #ifdef SERIAL_DEBUGMODE
        DEBUG_SERIAL.printf("Error %d submitting transaction to Algorand blockchain\n", iErr);
        #endif
        waitForever();
      }
      else
      { // Properly submitted
        #ifdef SERIAL_DEBUGMODE
        DEBUG_SERIAL.printf("\t*** Algorand transaction successfully submitted with ID = %s ***\n\n", g_algoIoT.getTransactionID());
        #endif
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


#ifndef FAKE_TPH_SENSOR
void initializeBME280()
{
  Bme280Settings customBmeSettings = Bme280Settings::weatherMonitoring();
  customBmeSettings.temperatureOversampling = Bme280Oversampling::X4;
  customBmeSettings.pressureOversampling = Bme280Oversampling::X2;
  customBmeSettings.humidityOversampling = Bme280Oversampling::X4;
  customBmeSettings.filter = Bme280Filter::X4;
  Wire.begin(SDA_PIN, SCL_PIN);  
  g_BMEsensor.begin(Bme280TwoWireAddress::Primary);
  g_BMEsensor.setSettings(customBmeSettings);
}
#endif    


int readPosition(float* lat_deg, float* lon_deg, int16_t* alt_m)
{
  #if defined(POS_LAT_DEG) && defined(POS_LON_DEG) && defined(POS_ALT_M)
  *lat_deg = POS_LAT_DEG;
  *lon_deg = POS_LON_DEG;
  *alt_m = POS_ALT_M;

  return 0; // OK
  #endif

  // Position not specified (it is optional after all)
  return 1;
}


// User may adapt this demo function to collect data
int readSensors(float* temperature_C, uint8_t* relhum_Pct, uint16_t* pressure_mbar)
{
  uint8_t bmeChipID = 0;

  #ifdef FAKE_TPH_SENSOR
  *temperature_C = FAKE_T_C;  // Dummy values
  *relhum_Pct = FAKE_H_PCT;
  *pressure_mbar = FAKE_P_MBAR;
  #else
  bmeChipID = g_BMEsensor.getChipId();
  if (bmeChipID == 0)
  { // BME280 not connected or not working
    return 1;
  }
  // Forced mode has to be specified every time we read data. We play safe and re-apply all settings
  Bme280Settings customBmeSettings = Bme280Settings::weatherMonitoring();
  customBmeSettings.temperatureOversampling = Bme280Oversampling::X4;
  customBmeSettings.pressureOversampling = Bme280Oversampling::X2;
  customBmeSettings.humidityOversampling = Bme280Oversampling::X4;
  customBmeSettings.filter = Bme280Filter::X4;
  g_BMEsensor.setSettings(customBmeSettings);
  delay(100);
  *temperature_C = g_BMEsensor.getTemperature();  
  *pressure_mbar = (uint16_t) (0.01f * g_BMEsensor.getPressure());
  *relhum_Pct = (uint8_t)(g_BMEsensor.getHumidity() + 0.5f);  // Round to largest int (H is always > 0)
  if (*pressure_mbar == 0)  // Sensor not working
    return 1;
  #endif

  return 0;
}
