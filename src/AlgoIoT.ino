/**
 *  "AlgoIoT"
 * 
 *  Algorand client for ESP32 (and other MCUs)
 * 
 *  Last mod 20231017-1
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



/////////////////////////////////////////////////////////////////////////////////
//     *******************************************************************
// ==> User-defined settings are in user_config.h: please edit accordingly <==
//     *******************************************************************
/////////////////////////////////////////////////////////////////////////////////



// TODO:
// Code refactoring
// Max number of attempts connecting to WiFi
// SHA512/256
// Mnemonics checksum (requires SHA512/256)
// Actual reading from BME280
// Sleep/wakeup

#include <Arduino.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>   // https://github.com/espressif/arduino-esp32/blob/master/libraries/HTTPClient/src/HTTPClient.h
#include <ArduinoJson.h>  // Algod sends a JSON with current Transaction parameters
#include <Crypto.h>
#include <Ed25519.h>
#include <base64.hpp>     // For Testnet Hash and debug
#include "minmpk.h"       // Our minimal messagepack builder
#include "base32decode.h" // Base32 decoding for Algorand addresses
#include "bip39enwords.h" // BIP39 english words to convert Algorand private key from mnemonics
#include "user_config.h"

#define DEBUG_SERIAL Serial

#define WIFI_RETRY_DELAY_MS 1000
#define HTTP_CONNECT_TIMEOUT_MS 5000UL
#define HTTP_QUERY_TIMEOUT_S 5
#define DATA_SEND_INTERVAL (( DATA_SEND_INTERVAL_MINS ) * 60 * 1000UL) 

#define BLANK_MSGPACK_HEADER 75  // We leave this space at the head of the buffer, so we can add the signature later
#define POST_MIME_TYPE "application/msgpack"
#define MAX_RESPONSE_LEN 512      // For Algorand transaction params. Max measured = 250
#define MAX_TX_MSGPACK_SIZE 1280  // 1253 max measured for payment transaction   
#define MAX_NOTES_SIZE 1000
#define TRANSACTION_PREFIX "TX"
#define TRANSACTION_PREFIX_BYTES 2
#define ALGORAND_PAYMENT_TRANSACTION_MIN_FIELDS 9 // without "note", otherwise 10 (not counting "sig" which is separate from txn Map)
#define ALGORAND_ADDRESS_BYTES 32
#define ALGORAND_KEY_BYTES 32
#define ALGORAND_SIG_BYTES 64
#define ALGORAND_NET_HASH_BYTES 32
#define ALGORAND_MNEMONICS_NUMBER 25
#define ALGORAND_MNEMONIC_MIN_LEN 3
#define ALGORAND_MNEMONIC_MAX_LEN 8
#define NODE_SERIAL_NUMBER 1234567890UL
#define ALGORAND_APP_NAME "AlgoIoT/v0.9"
#define ALGORAND_NOTES_SN_LABEL "SN"
#define ALGORAND_NOTES_T_LABEL "T(C)"
#define ALGORAND_NOTES_H_LABEL "H(%)"
#define ALGORAND_NOTES_P_LABEL "P(mbar)"
#ifndef RECEIVER_ADDRESS
  #define RECEIVER_ADDRESS ""
#endif  


// TODO derive binary address = public key from private key
#define THIS_NODE_ADDRESS "MT3WLS4GKJLZZ6CYFZPRUGR2PSSXXC5ABO72CB45AKNCGSUCSCVNWDKOUY" // Address of current IoT device
// #define SENDER_ADDRESS THIS_NODE_ADDRESS
#define HTTP_ENDPOINT "https://testnet-api.algonode.cloud"
#define GET_TRANSACTION_PARAMS "/v2/transactions/params"
#define POST_TRANSACTION "/v2/transactions"
#define ALGORAND_NETWORK_ID "testnet-v1.0"
#define ALGORAND_NETWORK_HASH "SGO1GKSzyE7IEPItTxCByw9x8FmnrCDexi9/cOUJOiI="
#define ALGORAND_MAX_WAIT_ROUNDS 1000
#define ALGORAND_MIN_PAYMENT_MICROALGOS 1 

#define FAKE_SENSOR



// Globals
WiFiMulti g_wifiMulti;
HTTPClient g_httpClient;        
StaticJsonDocument <MAX_RESPONSE_LEN>g_JDoc;
msgPack g_msgPackTx = NULL;
const String g_httpBaseURL = HTTP_ENDPOINT;
uint8_t g_privateKey[ALGORAND_KEY_BYTES];
uint8_t g_senderAddressBytes[ALGORAND_KEY_BYTES]; // = public key
uint8_t* g_pvtKey = NULL;
uint8_t* g_receiverAddressBytes = NULL;
uint8_t* g_testnetHash = NULL;
uint8_t g_Signature[ALGORAND_NET_HASH_BYTES];
uint8_t g_TransactionMessagePack[MAX_TX_MSGPACK_SIZE];
// fake values for temperature, humidity, pressure
const uint8_t H_PCT = 55;
const float T_C = 25.5f;
const uint16_t P_MBAR = 1018;
uint8_t g_notes[MAX_NOTES_SIZE];
#ifdef SERIAL_DEBUGMODE
char g_transactionID[65] = "";
#endif
// End globals



/////////////////////////
//
// Forward Declarations
//
/////////////////////////

void waitForever();

int readSensors(float* temperature_C, uint8_t* relhum_Pct, uint16_t* pressure_mbar);


// Algorand-related functions

// Decodes Base32 Algorand address to 32-byte binary address suitable for our functions
// outBinaryAddress allocated internally, has to be freed by caller
// Returns error code (0 = OK)
int decodeAlgorandAddress(const char* addressB32, uint8_t*& outBinaryAddress);


// Decodes Base64 Algorand network hash to 32-byte binary buffer suitable for our functions
// outBinaryHash allocated internally, has to be freed by caller
// Returns error code (0 = OK)
int decodeAlgorandNetHash(const char* hashB64, uint8_t*& outBinaryHash);


// Accepts a C string containing space-delimited mnemonic words (25 words)
// outPrivateKey allocated internally, has to be freed by caller
// Returns error code (0 = OK)
int decodePrivateKeyFromMnemonics(const char* mnemonicWords, uint8_t outPrivateKey[ALGORAND_KEY_BYTES]);



// Builds "notes" field, which carries data to be logged (ARC-2 JSON format)
// outBuffer passed by caller (not allocated internally)
int buildARC2JNotesField(uint8_t* outBuffer,
                         const uint16_t bufferLen,
                         const uint32_t serialNumber,
                         const float temperatureC,
                         const uint8_t Hpct,
                         const uint16_t Pmbar,
                         uint16_t* outputLen);


// Builds "notes" field, which carries data to be logged (ARC-2 MessagePack format)
// msgPack passed by caller (not allocated internally)
int buildARC2MPNotesField(msgPack mPack,
                          const uint32_t serialNumber,
                          const float temperatureC,
                          const uint8_t Hpct,
                          const uint16_t Pmbar);


// 1. Retrieves current Algorand transaction parameters
// Returns HTTP response code (200 = OK)
int getAlgorandTxParams(uint32_t* round, uint16_t* minFee);

// msgPack passed by caller (not allocated internally):

// 2. Fills Algorand transaction MessagePack
// Returns error code (0 = OK)
// "notes" max 1000 bytes
int prepareTransactionMessagePack(msgPack mPack, 
                                  const uint32_t lastRound, 
                                  const uint16_t fee, 
                                  const uint32_t paymentAmountMicroAlgos, 
                                  const uint8_t rcvAddressAsBytes[ALGORAND_ADDRESS_BYTES], 
                                  const uint8_t* notes, 
                                  const uint16_t notesLen);


// 4. Gets Ed25519 signature of binary pack (to which it internally prepends "TX" prefix)
// Caller passes a 64-bytes buffer in "signature"
// Returns error code (0 = OK)
int signMessagePackAddingPrefix(msgPack mPack, uint8_t* signature);


// 5. Adds signature to transaction and modifies messagepack accordingly
// Returns error code (0 = OK)
int createSignedBinaryTransaction(msgPack mPack, const uint8_t* signature);


// 6. Submits transaction to algod
// Last method to be called, after all the others
// Returns HTTP response code (200 = OK)
int submitTransaction(const msgPack mPack); 



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

  // Configure HTTP client
  g_httpClient.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);

  // Initialize Message Pack structure
  g_msgPackTx = msgpackInit(&(g_TransactionMessagePack[0]), MAX_TX_MSGPACK_SIZE);
  if (g_msgPackTx == NULL)  
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.println("\n Error initializing transaction MessagePack: aborting\n");
    #endif
    waitForever();
  }
  
  // Decode Algorand network hash
  iErr = decodeAlgorandNetHash(ALGORAND_NETWORK_HASH, g_testnetHash);
  if (iErr)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.printf("\n Error %d decoding Algorand network hash: aborting\n", ALGORAND_NETWORK_HASH, iErr);
    #endif
    waitForever();
  }

  // Decode private key from mnemonics
  iErr = decodePrivateKeyFromMnemonics(NODE_ACCOUNT_MNEMONICS, g_privateKey);
  if (iErr)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.printf("\n Error %d decoding Algorand private key: aborting\n", iErr);
    #endif
    waitForever();
  }

  // Derive public key = sender address ( = this node address) from private key
  Ed25519::derivePublicKey(g_senderAddressBytes, g_privateKey);

  // If RECEIVER_ADDRESS is empty, use sender address (transaction to self), else decode it
  if (strlen(RECEIVER_ADDRESS) == 0)
  { 
    g_receiverAddressBytes = (uint8_t*)malloc(ALGORAND_ADDRESS_BYTES);
    if (!g_receiverAddressBytes)
    {
      #ifdef SERIAL_DEBUGMODE
      DEBUG_SERIAL.printf("\n Memory error creating receiver address: aborting\n", iErr);
      #endif
      waitForever();
    }
    memcpy((void*)(&g_receiverAddressBytes[0]), g_senderAddressBytes, ALGORAND_ADDRESS_BYTES);
  }
  else
  {
    iErr = decodeAlgorandAddress(RECEIVER_ADDRESS, g_receiverAddressBytes);
    if (iErr)
    {
      #ifdef SERIAL_DEBUGMODE
      DEBUG_SERIAL.println("\n Error decoding Algorand receiver address: aborting\n");
      #endif
      waitForever();
    }  
  }
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
    uint32_t fv = 0;
    uint16_t fee = 0;
    uint8_t signature[ALGORAND_SIG_BYTES];
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

      // From now on, it's just a matter of calling in sequence:
      // buildARC2JNotesField()
      // getAlgorandTxParams()
      // prepareTransactionMessagePack()
      // signMessagePackAddingPrefix()
      // createSignedBinaryTransaction()
      // submitTransaction()

      // fill Note field in ARC-2
      iErr = buildARC2JNotesField(&(g_notes[0]), MAX_NOTES_SIZE, NODE_SERIAL_NUMBER, T_C, H_PCT, P_MBAR, &notesLen);
      if (iErr)
      {
        #ifdef SERIAL_DEBUGMODE
        DEBUG_SERIAL.printf("Error %d building note field\n", iErr);
        #endif
      }
      else
      { // Node field OK, proceed querying current Algorand network parameters
        int httpResCode = getAlgorandTxParams(&fv, &fee);
        if (httpResCode == 200)
        { // OK        
          #ifdef SERIAL_DEBUGMODE
          DEBUG_SERIAL.println("Transaction parameters correctly retrieved from algod service");
          #endif

        
          iErr = prepareTransactionMessagePack(g_msgPackTx, fv, fee, PAYMENT_AMOUNT_MICROALGOS, 
                    g_receiverAddressBytes, g_notes, notesLen);
          if (!iErr)
          { // Payment transaction correctly assembled. Now sign it
            iErr = signMessagePackAddingPrefix(g_msgPackTx, signature);
            if (!iErr)
            { // Sign OK, compose payload
              iErr = createSignedBinaryTransaction(g_msgPackTx, signature);
              if (!iErr)
              { // Payload ready. Now we can submit it via algod REST API
                #ifdef SERIAL_DEBUGMODE
                DEBUG_SERIAL.println("\nReady to submit transaction to Algorand network");
                DEBUG_SERIAL.println();
                #endif
                iErr = submitTransaction(g_msgPackTx); // Returns HTTP code
                if (iErr == 200)  // HTTP OK
                {
                  #ifdef SERIAL_DEBUGMODE
                  DEBUG_SERIAL.print("\t*** Algorand transaction successfully submitted with ID=");
                  DEBUG_SERIAL.print(g_transactionID);
                  DEBUG_SERIAL.println(" ***\n");
                  // DEBUG_SERIAL.printf("Total wakeup time = %u\n\n", millis() - currentMillis);
                  #endif
                }
                else
                {
                  #ifdef SERIAL_DEBUGMODE
                  DEBUG_SERIAL.print("\nError submitting Algorand transaction. HTTP code=");
                  DEBUG_SERIAL.println(iErr);
                  #endif
                }
              }
              else
              { // Error from createSignedBinaryTransaction()
                #ifdef SERIAL_DEBUGMODE
                DEBUG_SERIAL.printf("\nError %d creating signed binary transaction\n", iErr);
                #endif
              }
            }
            else
            { // Error from signMessagePackAddingPrefix()
              #ifdef SERIAL_DEBUGMODE
              DEBUG_SERIAL.printf("\nError %d signing MessagePack\n", iErr);
              #endif
            }
          }
          else
          { // Error from prepareTransactionMessagePack()
            #ifdef SERIAL_DEBUGMODE
            DEBUG_SERIAL.printf("\nError %d creating MessagePack\n", iErr);
            #endif
          }
        }
        else
        { // Error from getAlgorandTxParams()
          #ifdef SERIAL_DEBUGMODE
          DEBUG_SERIAL.println("Error retrieving transaction parameters from algod service");
          #endif
        }
      }
      // Wait for next data upload
    }
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
  *temperature_C = T_C;
  *relhum_Pct = H_PCT;
  *pressure_mbar = P_MBAR;
  #else
  // TODO read BME280 sensor
  return 1;
  #endif

  return 0;
}


//
// Algorand-related functions
//


// Decodes Base64 Algorand network hash to 32-byte binary buffer suitable for our functions
// outBinaryHash has to be freed by caller
// Returns error code (0 = OK)
int decodeAlgorandNetHash(const char* hashB64, uint8_t*& outBinaryHash)
{ 
  if (hashB64 == NULL)
    return 1;
  
  int inputLen = strlen(hashB64);
  if (inputLen > encode_base64_length(ALGORAND_NET_HASH_BYTES))
    return 2;
  
  outBinaryHash = (uint8_t*) malloc(ALGORAND_NET_HASH_BYTES);
  
  int hashLen = decode_base64((unsigned char*)hashB64, outBinaryHash);
  if (hashLen != ALGORAND_NET_HASH_BYTES)
  {
    free(outBinaryHash);
    return 3;
  }

  return 0;
}


int decodeAlgorandAddress(const char* addressB32, uint8_t*& outBinaryAddress)
{
  if (addressB32 == NULL)
    return 1;
  
  int iLen = Base32::fromBase32((uint8_t*)addressB32, strlen(addressB32), outBinaryAddress);
  if (iLen < ALGORAND_ADDRESS_BYTES + 4)  // Decoded address len from Base32 has to be exactly 36 bytes (but we use only the first 32 bytes)
  {
    free(outBinaryAddress);
    return 2;
  }

  return 0;
}


int decodePrivateKeyFromMnemonics(const char* inMnemonicWords, uint8_t outPrivateKey[ALGORAND_KEY_BYTES])
{ 
  uint16_t  indexes11bit[ALGORAND_MNEMONICS_NUMBER];
  uint8_t   decodedBytes[ALGORAND_KEY_BYTES + 3];
  // char      checksumWord[ALGORAND_MNEMONIC_MAX_LEN + 1] = "";  
  char*     mnWord = NULL;
  char*     mnemonicWords = NULL;

  if (inMnemonicWords == NULL)
    return 1;

  int inputLen = strlen(inMnemonicWords);

  // Early sanity check: mnemonicWords contains 25 space-delimited words, each composed by a minimum of 3 chars
  if (inputLen < ALGORAND_MNEMONICS_NUMBER * (ALGORAND_MNEMONIC_MIN_LEN + 1))
    return 2;

  // Copy input to work string
  mnemonicWords = (char*)malloc(inputLen + 1);
  if (!mnemonicWords)
    return 7;
  strcpy(mnemonicWords, inMnemonicWords);

  // Off-cycle
  mnWord = strtok(mnemonicWords, " ");
  if (mnWord == NULL)
  {
    free(mnemonicWords);
    return 3; // Invalid input, does not contain spaces
  }

  // Input parsing loop
  uint8_t index = 0;
  uint8_t found = 0;
  uint16_t pos = 0;
  while (mnWord != NULL) 
  {
    // Check word validity against BIP39 English words
    found = 0;
    pos = 0;
    while ((pos < BIP39_EN_WORDS_NUM) && (!found))
    {
      if (!strcmp(mnWord, BIP39_EN_Wordlist[pos]))
      {
        indexes11bit[index++] = pos;
        found = 1;
      }
      pos++;
    }
    if (!found)
    {
      free(mnemonicWords);
      return 4; // Wrong mnemonics: invalid word
    }
    if (index > ALGORAND_MNEMONICS_NUMBER)
    {
      free(mnemonicWords);
      return 5; // Wrong mnemonics: too many words
    }

    mnWord = strtok(NULL, " "); // strtok with NULL as first argument means it continues to parse the original string
  }

  if (index != ALGORAND_MNEMONICS_NUMBER)
  {
    free(mnemonicWords);
    return 6; // Wrong mnemonics: too few words (we already managed the too much words case)
  }
  
  // We now have an array of ALGORAND_MNEMONICS_NUMBER 16-bit unsigned values, which actually only use 11 bits (0..2047)
  // The last element is a checksum 

  // Save checksum word (not used ATM, see below)
  // strncpy(checksumWord, BIP39_EN_Wordlist[indexes[index-1]], ALGORAND_MNEMONIC_MAX_LEN);

  free(mnemonicWords);

  // We now build a byte array from the uint16_t array: 25 x 11-bits values become 34/35 x 8-bits values

  uint32_t tempInt = 0;
  uint16_t numBits = 0;
  uint16_t destIndex = 0;
  for (uint16_t i = 0; i < ALGORAND_MNEMONICS_NUMBER; i++)
  { 
    // For each 11-bit value, fill appropriate consecutive byte array elements
    tempInt |= (((uint32_t)indexes11bit[i]) << numBits);
    numBits += 11;
    while (numBits >= 8)
    {
      decodedBytes[destIndex] = (uint8_t)(tempInt & 0xff);
      destIndex++;
      tempInt = tempInt >> 8;
      numBits -= 8;
    }
  }
  if (numBits != 0)
  {
    decodedBytes[destIndex] = (uint8_t)(tempInt & 0xff);
  }

  // TODO we do not verify the checksum, because at the moment we miss a viable implementation of SHA512/256

  // Copy key to output array
  memcpy((void*)&(outPrivateKey[0]), (void*)decodedBytes, ALGORAND_KEY_BYTES);

  return 0;
}


// Retrieves current Algorand transaction parameters
// Returns HTTP error code
// TODO: On error codes 5xx (server error), maybe we should retry after 5s?
int getAlgorandTxParams(uint32_t* round, uint16_t* minFee)
{
  String httpRequest = g_httpBaseURL + GET_TRANSACTION_PARAMS;
          
  *round = 0;
  *minFee = 0;

  // configure server and url               
  g_httpClient.begin(httpRequest);
    
  int httpResponseCode = g_httpClient.GET();

      
  // httpResponseCode will be negative on error
  if (httpResponseCode < 0)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.print("HTTP GET failed, error: "); DEBUG_SERIAL.println(g_httpClient.errorToString(httpResponseCode).c_str());
    #endif
  }
  else
  {
    switch (httpResponseCode)
    {
      case 200:
      {   // No error: let's get the response
        String payload = g_httpClient.getString();
        StaticJsonDocument<MAX_RESPONSE_LEN> JSONResDoc;
                        
        #ifdef SERIAL_DEBUGMODE
        DEBUG_SERIAL.println("Server response:");
        DEBUG_SERIAL.println(payload);
        #endif

        DeserializationError error = deserializeJson(JSONResDoc, payload);                
        if (error) 
        {
          #ifdef SERIAL_DEBUGMODE
          DEBUG_SERIAL.println("JSON response parsing failed!");
          #endif
        }
        else
        { // Fetch interesting fields
          *minFee = JSONResDoc["min-fee"];
          *round = JSONResDoc["last-round"];

          #ifdef SERIAL_DEBUGMODE
          DEBUG_SERIAL.println("Algorand transaction parameters received:");
          DEBUG_SERIAL.print("min-fee = "); DEBUG_SERIAL.print(*minFee); DEBUG_SERIAL.println(" microAlgo");
          DEBUG_SERIAL.print("last-round = "); DEBUG_SERIAL.println(*round);                  
          #endif                  
        }
      }
      break;
      case 204:
      {   // No error, but no data available from server
        #ifdef SERIAL_DEBUGMODE
        DEBUG_SERIAL.println("Server returned no data");
        #endif
      }
      break;
      default:
      {
        #ifdef SERIAL_DEBUGMODE
        DEBUG_SERIAL.print("Unmanaged HTTP response code "); DEBUG_SERIAL.println(httpResponseCode);
        #endif
      }
      break;
    }
  }        
  
  g_httpClient.end();

  return httpResponseCode;
}


// Builds "notes" field, which carries data to be logged (ARC-2 JSON format)
// buffer passed by caller
int buildARC2JNotesField(uint8_t* outBuffer,
                         const uint16_t bufferLen,
                         const uint32_t serialNumber,
                         const float temperatureC,
                         const uint8_t Hpct,
                         const uint16_t Pmbar,
                         uint16_t* outputLen)
{
  // An ARC-2/JSON, "note" starts with the app name in clear text, followed by ":j" again in clear text
  // Then we have the JSON (root node and fields = "label":value)
  int iErr = 0;
  StaticJsonDocument <64>JDoc;
  uint16_t currentPos = 0;

  // Check output buffer is not empty
  if (outBuffer == NULL)
    return 1;

  currentPos = 0;
  // Write app name and format specifier
  memcpy((void*)outBuffer, (void*)ALGORAND_APP_NAME, strlen(ALGORAND_APP_NAME));
  currentPos += strlen(ALGORAND_APP_NAME);
  outBuffer[currentPos++] = ':';
  outBuffer[currentPos++] = 'j';

  // Build JSON
  JDoc[ALGORAND_NOTES_SN_LABEL] = serialNumber;
  JDoc[ALGORAND_NOTES_T_LABEL] = temperatureC;
  JDoc[ALGORAND_NOTES_H_LABEL] = Hpct;
  JDoc[ALGORAND_NOTES_P_LABEL] = Pmbar;

  // Serialize to buffer after "<app-name>:j"
  int jlen = serializeJson(JDoc, (char*) (outBuffer + currentPos), bufferLen);
  if (jlen < 1)
  {
    return 3;
  }

  *outputLen = jlen + currentPos;

  return 0;
}


// Builds "notes" field, which carries data to be logged (ARC-2 MessagePack format)
// msgPack passed by caller
// Not used at the moment: message pack-formatted ARC-2 notes are not correctly interpreted by explorers
int buildARC2MPNotesField(msgPack mPack,
                          const uint32_t serialNumber,
                          const float temperatureC,
                          const uint8_t Hpct,
                          const uint16_t Pmbar)
{
  // An ARC-2/Message Pack, "note" starts with the app name in clear text, followed by ":m" again in clear text
  // Then we have the Message Pack proper (root map and fields = "label":value)
  int iErr = 0;
  uint16_t totalLen = 0;
  const uint16_t clearTextHeaderLen = strlen(ALGORAND_APP_NAME) + 2; // App name + ':m'

  // Check mpack buffer is not empty
  if (mPack->msgBuffer == NULL)
    return 1;

  // Check buffer len is appropriate
  totalLen += clearTextHeaderLen;
  totalLen += 1;  // Root Map specifier
  totalLen += strlen(ALGORAND_NOTES_SN_LABEL) + strlen(ALGORAND_NOTES_T_LABEL) + strlen(ALGORAND_NOTES_H_LABEL) + strlen(ALGORAND_NOTES_P_LABEL) + 4; // Labels and specifiers
  totalLen += 4 + 1;  // Float value for T and specifier
  totalLen += 4 + 2;  // 2 x uint16 values for H and P, plus specifiers
  if (mPack->bufferLen < totalLen)
    return 2;

  // Leave space for app name and separator (clear text before Message Pack)
  iErr = msgPackModifyCurrentPosition(mPack, clearTextHeaderLen);
  if (iErr)
    return 3;

  // Root map
  iErr = msgpackAddShortMap(mPack, 4);
  if (iErr)
    return 3;

  // Serial number label
  iErr = msgpackAddShortString(mPack, ALGORAND_NOTES_SN_LABEL);
  if (iErr)
    return 3;
  // Serial number value
  iErr = msgpackAddUInt32(mPack, serialNumber);
  if (iErr)
    return 3;

  // Temperature label
  iErr = msgpackAddShortString(mPack, ALGORAND_NOTES_T_LABEL);
  if (iErr)
    return 3;
  // Temperature value
  iErr = msgpackAddFloat(mPack, temperatureC);
  if (iErr)
    return 3;

  // Relative humidity label
  iErr = msgpackAddShortString(mPack, ALGORAND_NOTES_H_LABEL);
  if (iErr)
    return 3;
  // RelHum value (%)
  iErr = msgpackAddUInt16(mPack, Hpct);
  if (iErr)
    return 3;

  // Atmospheric pressure label
  iErr = msgpackAddShortString(mPack, ALGORAND_NOTES_P_LABEL);
  if (iErr)
    return 3;
  // Pressure value (millibar)
  iErr = msgpackAddUInt16(mPack, Pmbar);
  if (iErr)
    return 3;

  // Now we add the clear text part as buffer header
  memcpy((void*)mPack->msgBuffer, (void*)ALGORAND_APP_NAME, clearTextHeaderLen - 2);
  mPack->msgBuffer[clearTextHeaderLen - 2] = ':';
  mPack->msgBuffer[clearTextHeaderLen - 1] = 'm';

  return 0;
}


// To be called AFTER getAlgorandTxParams(), because we need current "min-fee" and "last-round" values from algod
// Returns error code (0 = OK)
int prepareTransactionMessagePack(msgPack mPack, 
                                  const uint32_t lastRound, 
                                  const uint16_t fee, 
                                  const uint32_t paymentAmountMicroAlgos, 
                                  const uint8_t rcvAddressAsBytes[ALGORAND_ADDRESS_BYTES], 
                                  const uint8_t* notes, 
                                  const uint16_t notesLen)
{ 
  int iErr = 0;
  const char gen[] = ALGORAND_NETWORK_ID;
  // gh = g_testnetHash
  uint32_t lv = lastRound + ALGORAND_MAX_WAIT_ROUNDS;
  // rcv = g_receiverAddressBytes;
  // snd = g_senderAddressBytes;
  const char type[] = "pay";
  uint8_t nFields = ALGORAND_PAYMENT_TRANSACTION_MIN_FIELDS;

  if (mPack == NULL)
    return 1;
  if (mPack->msgBuffer == NULL)
    return 2;
  if ((lastRound == 0) || (fee == 0) || (paymentAmountMicroAlgos < ALGORAND_MIN_PAYMENT_MICROALGOS))
  {
    return 3;
  }
  
  if ((notes != NULL) && (notesLen > 0))
    nFields++;  // We have 9 fields without Note, 10 with Note

  // We leave a blank space header so we can add:
  // - "TX" prefix before signing
  // - signature field and "txn" node field after signing
  iErr = msgPackModifyCurrentPosition(mPack, BLANK_MSGPACK_HEADER);
  if (iErr)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d from msgPackModifyCurrentPosition()\n\n");
    #endif

    return 5;
  }

  // Add root map
  iErr = msgpackAddShortMap(mPack, nFields); 
  if (iErr)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding root map\n\n");
    #endif

    return 5;
  }

  // Fields must follow alphabetical order

  // "amt" label
  iErr = msgpackAddShortString(mPack, "amt");
  if (iErr)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding amt label\n\n");
    #endif

    return 5;
  }
  // amt value
  // Note: encoding depends on payment amount
  if (paymentAmountMicroAlgos < 128)
  {
    iErr = msgpackAddUInt7(mPack, (uint8_t)paymentAmountMicroAlgos);
  }
  else
  {
    if (paymentAmountMicroAlgos < 256)
    {
      iErr = msgpackAddUInt8(mPack, (uint8_t)paymentAmountMicroAlgos);
    }
    else
    {
      if (paymentAmountMicroAlgos < 65536)
      {
        iErr = msgpackAddUInt16(mPack, (uint16_t)paymentAmountMicroAlgos);
      }
      else    
      {
        iErr = msgpackAddUInt32(mPack, paymentAmountMicroAlgos);
      }
    }
  }
  if (iErr)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding amt value\n\n");
    #endif

    return 5;
  }

  // "fee" label
  iErr = msgpackAddShortString(mPack, "fee");
  if (iErr)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding fee label\n\n");
    #endif

    return 5;
  }
  // fee value
  iErr = msgpackAddUInt16(mPack, fee);
  if (iErr)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding fee value\n\n");
    #endif

    return 5;
  }

  // "fv" label
  iErr = msgpackAddShortString(mPack, "fv");
  if (iErr)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding fv label\n\n");
    #endif

    return 5;
  }
  // fv value
  iErr = msgpackAddUInt32(mPack, lastRound);
  if (iErr)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding fv value\n\n");
    #endif

    return 5;
  }

  // "gen" label
  iErr = msgpackAddShortString(mPack, "gen");
  if (iErr)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding gen label\n\n");
    #endif

    return 5;
  }
  // gen string
  iErr = msgpackAddShortString(mPack, ALGORAND_NETWORK_ID);
  if (iErr)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding gen string\n\n");
    #endif

    return 5;
  }

  // "gh" label
  iErr = msgpackAddShortString(mPack, "gh");
  if (iErr)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding gh label\n\n");
    #endif

    return 5;
  }
  // gh value (binary buffer)
  iErr = msgpackAddShortByteArray(mPack, (const uint8_t*)&(g_testnetHash[0]), (const uint8_t)ALGORAND_NET_HASH_BYTES);
  if (iErr)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding gh value\n\n");
    #endif

    return 5;
  }

  // "lv" label
  iErr = msgpackAddShortString(mPack, "lv");
  if (iErr)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding lv label\n\n");
    #endif

    return 5;
  }
  // lv value
  iErr = msgpackAddUInt32(mPack, lv);
  if (iErr)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding lv value\n\n");
    #endif

    return 5;
  }

  if ((notes != NULL) && (notesLen > 0))
  {
    // Add "note" label
    iErr = msgpackAddShortString(mPack, "note");
    if (iErr)
    {
      #ifdef SERIAL_DEBUGMODE
      DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding note label\n\n");
      #endif

      return 5;
    }
    // Add note content as binary buffer
    // WARNING: if note len is < 256, we have to encode Bin 8 so msgpackAddShortByteArray
    // Otherwise, signature does not pass verification
    if (notesLen < 256)
      iErr = msgpackAddShortByteArray(mPack, notes, (const uint8_t)notesLen);    
    else
      iErr = msgpackAddByteArray(mPack, notes, (const uint16_t)notesLen);
    if (iErr)
    {
      #ifdef SERIAL_DEBUGMODE
      DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding notes content\n\n");
      #endif

      return 5;
    }
  }

  // "rcv" label
  iErr = msgpackAddShortString(mPack, "rcv");
  if (iErr)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding rcv label\n\n");
    #endif

    return 5;
  }
  // rcv value (binary buffer)
  iErr = msgpackAddShortByteArray(mPack, (const uint8_t*)&(g_receiverAddressBytes[0]), (const uint8_t)ALGORAND_ADDRESS_BYTES);  
  if (iErr)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding rcv value\n\n");
    #endif

    return 5;
  }

  // "snd" label
  iErr = msgpackAddShortString(mPack, "snd");
  if (iErr)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding snd label\n\n");
    #endif

    return 5;
  }
  // snd value (binary buffer)
  iErr = msgpackAddShortByteArray(mPack, (const uint8_t*)&(g_senderAddressBytes[0]), (const uint8_t)ALGORAND_ADDRESS_BYTES);  
  if (iErr)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding snd value\n\n");
    #endif

    return 5;
  }

  // "type" label
  iErr = msgpackAddShortString(mPack, "type");
  if (iErr)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding type label\n\n");
    #endif

    return 5;
  }
  // type string
  iErr = msgpackAddShortString(mPack, "pay");
  if (iErr)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding type string\n\n");
    #endif

    return 5;
  }

  // End of messagepack

  return 0;
}



// Obtains Ed25519 signature of g_MessagePack, adding "TX" prefix; fills signature
// To be called AFTER convertToMessagePack()
// Returns error code (0 = OK)
// Caller passes a 64-byte array in "signature"
int signMessagePackAddingPrefix(msgPack mPack, uint8_t signature[64])
{
  uint8_t* payloadPointer = NULL;
  uint32_t payloadBytes = 0;

  if (mPack == NULL)
    return 1;
  if (mPack->msgBuffer == NULL)
    return 2;
  if (mPack->currentMsgLen == 0)
    return 2;

  // We sign from prefix (included), leaving out the rest of the blank header
  payloadPointer = mPack->msgBuffer + BLANK_MSGPACK_HEADER - TRANSACTION_PREFIX_BYTES;
  payloadBytes = mPack->currentMsgLen + TRANSACTION_PREFIX_BYTES;

  // Add prefix to messagepack; we purposedly left a blank header, with length BLANK_MSGPACK_HEADER
  payloadPointer[0] = 'T';
  payloadPointer[1] = 'X';

  // Sign pack+prefix
  Ed25519::sign(signature, g_privateKey, g_senderAddressBytes, payloadPointer, payloadBytes);

  return 0;
}


// Add Signature to MessagePack. We reserved a blank space header
// To be called AFTER signMessagePackAddingPrefix()
// Returns error code (0 = OK)
int createSignedBinaryTransaction(msgPack mPack, const uint8_t* signature)
{
  int iErr = 0;
  // When adding the Signature "sig" field, the messagepack has to be changed into a 2-level structure,
  // with a "txn" node holding existing fields and a new "sig" field directly in root
  // The JSON equivalent would be something like:
  /*
  {
    "sig": <array>,
    "txn": 
    {
     "amt": 10000000,
     "fee": 1000,
     "fv": 32752600,
     "gen": "testnet-v1.0",
      [...]
    }
  }
  */

  // We reset internal msgpack pointer, since we start from the beginning of the messagepack (filling the blank space)
  iErr = msgPackModifyCurrentPosition(mPack, 0);
  if (iErr)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.printf("\n createSignedBinaryTransaction(): ERROR %d resetting position\n\n");
    #endif

    return 5;
  }

  // Add a Map holding 2 fields (sig and txn)
  iErr = msgpackAddShortMap(mPack, 2);
  if (iErr)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.printf("\n createSignedBinaryTransaction(): ERROR %d adding map\n\n");
    #endif

    return 5;
  }
  
  // Add "sig" label
  iErr = msgpackAddShortString(mPack, "sig");
  if (iErr)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.printf("\n createSignedBinaryTransaction(): ERROR %d adding sign label\n\n");
    #endif

    return 5;
  }

  // Add signature, which is a 64-bytes byte array 
  iErr = msgpackAddShortByteArray(mPack, signature, (const uint8_t)ALGORAND_SIG_BYTES);
  if (iErr)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.printf("\n createSignedBinaryTransaction(): ERROR %d adding signature\n\n");
    #endif

    return 5;
  }

  // Add "txn" label
  iErr = msgpackAddShortString(mPack, "txn");
  if (iErr)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.printf("\n createSignedBinaryTransaction(): ERROR %d adding txn label\n\n");
    #endif

    return 5;
  }

  // The rest stays as it is

  return 0;
}


// Submits transaction messagepack to algod
// Last method to be called, after all the others
// Returns http response code (200 = OK)
// TODO: On error codes 5xx (server error), maybe we should retry after 5s?
int submitTransaction(const msgPack mPack)
{
  String httpRequest = g_httpBaseURL + POST_TRANSACTION;
          
  // Configure server and url               
  g_httpClient.begin(httpRequest);
  
  // Configure MIME type
  g_httpClient.addHeader("Content-Type", POST_MIME_TYPE);
  int httpResponseCode = g_httpClient.POST(mPack->msgBuffer, mPack->currentMsgLen);
      
  // httpResponseCode will be negative on error
  if (httpResponseCode < 0)
  {
    #ifdef SERIAL_DEBUGMODE
    DEBUG_SERIAL.print("\n[HTTP] POST failed, error: "); DEBUG_SERIAL.println(g_httpClient.errorToString(httpResponseCode).c_str());
    #endif
  }
  else
  {
    switch (httpResponseCode)
    {
      case 200:
      {   // No error: let's get the response for debug purposes
        #ifdef SERIAL_DEBUGMODE
        String payload = g_httpClient.getString();
        StaticJsonDocument<MAX_RESPONSE_LEN> JSONResDoc;
                        
        DeserializationError error = deserializeJson(JSONResDoc, payload);                
        if (error) 
        {
          DEBUG_SERIAL.println("JSON response parsing failed!");
        }
        else
        { // Fetch interesting fields                  
          strncpy(g_transactionID, JSONResDoc["txId"], 64);
        }
        #endif
      }
      break;
      case 204:
      {   // No error, but no data available from server
        #ifdef SERIAL_DEBUGMODE
        DEBUG_SERIAL.println("\nServer returned no data");
        #endif
      }
      break;
      case 400:
      {   // Malformed request
        #ifdef SERIAL_DEBUGMODE
        DEBUG_SERIAL.println("\nTransaction format error");
        DEBUG_SERIAL.println("Server response:");
        String payload = g_httpClient.getString();
        DEBUG_SERIAL.println(payload);
        #endif
      }
      break;
      default:
      {
        #ifdef SERIAL_DEBUGMODE
        DEBUG_SERIAL.print("\nUnmanaged HTTP response code "); DEBUG_SERIAL.println(httpResponseCode);
        #endif
      }
      break;
    }
  }        
  
  g_httpClient.end();

  return httpResponseCode;
}
