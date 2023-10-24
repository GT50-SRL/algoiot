#ifndef _USER_CONFIG_H
#define _USER_CONFIG_H

// User-configured parameters. Edit as needed

// This is an Algorand Testnet account, for demo purposes. Refinance with Dispenser if needed, or use an account of yours
#define NODE_ACCOUNT_MNEMONICS "shadow market lounge gauge battle small crash funny supreme regular obtain require control oil lend reward galaxy tuition elder owner flavor rural expose absent sniff"  
#define RECEIVER_ADDRESS "" 				// Specify "" to send to self
#define PAYMENT_AMOUNT_MICROALGOS 100000	// Please check vs. ALGORAND_MIN_PAYMENT_MICROALGOS in .ino
#define DATA_SEND_INTERVAL_MINS 60 			// 1 h

#define MYWIFI_SSID "MySSID" 	// Edit accordingly. Please note only 2.4 GHz WiFi is supported
#define MYWIFI_PWD "MyPassword"

// Comment out if a BME280 module is connected via I2C to the ESP32 board
// We use Bme280 library at: https://github.com/malokhvii-eduard/arduino-bme280
#define FAKE_SENSOR

#ifdef FAKE_SENSOR
// Assign your fake data of choice here
const uint8_t FAKE_H_PCT = 55;
const float FAKE_T_C = 25.5f;
const uint16_t FAKE_P_MBAR = 1018;
#else
#define SDA_PIN 21	// Sensor module connection
#define SCL_PIN 22	// Edit accordingly to your board/wiring
#endif

// Please leave uncommented: we did not actually implement GPS reading yet
#define FAKE_GPS

#ifdef FAKE_GPS
// Assign your fake position of choice here
const float FAKE_LAT = 41.905608f;
const float FAKE_LON = 12.443910f;
const int16_t FAKE_ALT_M = 25;
#else
#define UART_TX_PIN 33		// These are OK for UM TinyPICO
#define UART_RX_PIN 32		// Edit accordingly to your board/wiring
#define GPS_BAUD_RATE 9600	// For most GPS modules is OK. Check yours
#endif

// Uncomment to get debug prints on Serial Monitor
#define SERIAL_DEBUGMODE


#endif _USER_CONFIG_H
