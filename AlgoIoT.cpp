// algoiot.cpp
// v20240424-1
// Comments updated 20250905

// Work in progress	
// TODO:
// submitTransactionToAlgorand():
//  check for network errors separately and return appropriate error code
// Max number of attempts connecting to WiFi
// SHA512/256
// Mnemonics checksum (requires SHA512/256)

// By Fernando Carello for GT50
/* Copyright 2023 GT50 S.r.l.
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


#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <Crypto.h>
#include <base64.hpp>    
#include <Ed25519.h>
#include "base32decode.h" // Base32 decoding for Algorand addresses
#include "bip39enwords.h" // BIP39 english words to convert Algorand private key from mnemonics
#include "AlgoIoT.h"

#define LIB_DEBUGMODE
#define DEBUG_SERIAL Serial


// Class AlgoIoT

///////////////////////////////
// Public methods (exported)
///////////////////////////////

// Constructor
AlgoIoT::AlgoIoT(const char* sAppName, const char* nodeAccountMnemonics)
{
  int iErr = 0;

  if (sAppName == NULL)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.println("\n Error: NULL AppName passed to constructor\n");
    #endif
    return;
  }
  if (strlen(sAppName) > DAPP_NAME_MAX_LEN)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.println("\n Error: app name too long\n");
    #endif
    return;
  }
  strcpy(m_appName, sAppName);

  if (nodeAccountMnemonics == NULL)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.println("\n Error: NULL mnemonic words passed to constructor\n");
    #endif
    return;
  }

    // Configure HTTP client
  m_httpClient.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);

  // Decode private key from mnemonics
  iErr = decodePrivateKeyFromMnemonics(nodeAccountMnemonics, m_privateKey);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n Error %d decoding Algorand private key from mnemonic words\n", iErr);
    #endif
    return;
  }

  // Derive public key = sender address ( = this node address) from private key
  Ed25519::derivePublicKey(m_senderAddressBytes, m_privateKey);

  // By default, use current (sender) address as destination address (transaction to self)
  // User may set a different address later, with appropriate setter
  m_receiverAddressBytes = (uint8_t*)malloc(ALGORAND_ADDRESS_BYTES);
  if (!m_receiverAddressBytes)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n Memory error creating receiver address\n", iErr);
    #endif
    return;
  }
  memcpy((void*)(&(m_receiverAddressBytes[0])), m_senderAddressBytes, ALGORAND_ADDRESS_BYTES);
}


int AlgoIoT::setDestinationAddress(const char* algorandAddress)
{
  int iErr = 0;
  
  if (algorandAddress == NULL)
  {
    return ALGOIOT_NULL_POINTER_ERROR;
  }
  if (strlen(algorandAddress) != ALGORAND_ADDRESS_BYTES)
  {
    return ALGOIOT_BAD_PARAM;
  }
  iErr = decodeAlgorandAddress(algorandAddress, m_receiverAddressBytes);
  {
    return ALGOIOT_BAD_PARAM;
  }

  return ALGOIOT_NO_ERROR;
}


int AlgoIoT::setAlgorandNetwork(const uint8_t networkType)
{
  if ( (networkType != ALGORAND_TESTNET) && (networkType != ALGORAND_MAINNET) )
  {
    return ALGOIOT_BAD_PARAM;
  }

  m_networkType = networkType;
  if (m_networkType == ALGORAND_TESTNET)
  {
    m_httpBaseURL = ALGORAND_TESTNET_API_ENDPOINT;
  }
  else
  {
    m_httpBaseURL = ALGORAND_MAINNET_API_ENDPOINT;
  }

  return ALGOIOT_NO_ERROR;
}


const char* AlgoIoT::getTransactionID()
{
  return m_transactionID;
}


// Public methods to add values to be written in the blockchain
// Strongly typed; this helps towards adding ARC-2/MessagePack in the future

int AlgoIoT::dataAddInt8Field(const char* label, const int8_t value)
{
  int len = 0;

  if (label == NULL)
  {
    return ALGOIOT_NULL_POINTER_ERROR;
  }
  if (strlen(label) > NOTE_LABEL_MAX_LEN)
  {
    return ALGOIOT_BAD_PARAM;
  }

  m_noteJDoc[label] = value;
  
  // It is not trivial to anticipate how many chars we are going to add,
  // so we check JSON length after the fact
  len = m_noteOffset + measureJson(m_noteJDoc);
  if (len >= ALGORAND_MAX_NOTES_SIZE)
  {
    return ALGOIOT_DATA_STRUCTURE_TOO_LONG;
  }

  // Update note len
  m_noteLen = len;

  return ALGOIOT_NO_ERROR;
}

int AlgoIoT::dataAddUInt8Field(const char* label, const uint8_t value)
{
  int len = 0;

  if (label == NULL)
  {
    return ALGOIOT_NULL_POINTER_ERROR;
  }
  if (strlen(label) > NOTE_LABEL_MAX_LEN)
  {
    return ALGOIOT_BAD_PARAM;
  }

  m_noteJDoc[label] = value;
  
  // It is not trivial to anticipate how many chars we are going to add,
  // so we check JSON length after the fact
  len = m_noteOffset + measureJson(m_noteJDoc);
  if (len >= ALGORAND_MAX_NOTES_SIZE)
  {
    return ALGOIOT_DATA_STRUCTURE_TOO_LONG;
  }

  // Update note len
  m_noteLen = len;

  return ALGOIOT_NO_ERROR;
}

int AlgoIoT::dataAddInt16Field(const char* label, const int16_t value)
{
  int len = 0;

  if (label == NULL)
  {
    return ALGOIOT_NULL_POINTER_ERROR;
  }
  if (strlen(label) > NOTE_LABEL_MAX_LEN)
  {
    return ALGOIOT_BAD_PARAM;
  }

  m_noteJDoc[label] = value;
  
  // It is not trivial to anticipate how many chars we are going to add,
  // so we check JSON length after the fact
  len = m_noteOffset + measureJson(m_noteJDoc);
  if (len >= ALGORAND_MAX_NOTES_SIZE)
  {
    return ALGOIOT_DATA_STRUCTURE_TOO_LONG;
  }

  // Update note len
  m_noteLen = len;

  return ALGOIOT_NO_ERROR;
}

int AlgoIoT::dataAddUInt16Field(const char* label, const uint16_t value)
{
  int len = 0;

  if (label == NULL)
  {
    return ALGOIOT_NULL_POINTER_ERROR;
  }
  if (strlen(label) > NOTE_LABEL_MAX_LEN)
  {
    return ALGOIOT_BAD_PARAM;
  }

  m_noteJDoc[label] = value;
  
  // It is not trivial to anticipate how many chars we are going to add,
  // so we check JSON length after the fact
  len = m_noteOffset + measureJson(m_noteJDoc);
  if (len >= ALGORAND_MAX_NOTES_SIZE)
  {
    return ALGOIOT_DATA_STRUCTURE_TOO_LONG;
  }

  // Update note len
  m_noteLen = len;

  return ALGOIOT_NO_ERROR;
}

int AlgoIoT::dataAddInt32Field(const char* label, const int32_t value)
{
  int len = 0;

  if (label == NULL)
  {
    return ALGOIOT_NULL_POINTER_ERROR;
  }
  if (strlen(label) > NOTE_LABEL_MAX_LEN)
  {
    return ALGOIOT_BAD_PARAM;
  }

  m_noteJDoc[label] = value;
  
  // It is not trivial to anticipate how many chars we are going to add,
  // so we check JSON length after the fact
  len = m_noteOffset + measureJson(m_noteJDoc);
  if (len >= ALGORAND_MAX_NOTES_SIZE)
  {
    return ALGOIOT_DATA_STRUCTURE_TOO_LONG;
  }

  // Update note len
  m_noteLen = len;

  return ALGOIOT_NO_ERROR;
}

int AlgoIoT::dataAddUInt32Field(const char* label, const uint32_t value)
{
  int len = 0;

  if (label == NULL)
  {
    return ALGOIOT_NULL_POINTER_ERROR;
  }
  if (strlen(label) > NOTE_LABEL_MAX_LEN)
  {
    return ALGOIOT_BAD_PARAM;
  }

  m_noteJDoc[label] = value;
  
  // It is not trivial to anticipate how many chars we are going to add,
  // so we check JSON length after the fact
  len = m_noteOffset + measureJson(m_noteJDoc);
  if (len >= ALGORAND_MAX_NOTES_SIZE)
  {
    return ALGOIOT_DATA_STRUCTURE_TOO_LONG;
  }

  // Update note len
  m_noteLen = len;

  return ALGOIOT_NO_ERROR;
}

int AlgoIoT::dataAddFloatField(const char* label, const float value)
{ 
  int len = 0;

  if (label == NULL)
  {
    return ALGOIOT_NULL_POINTER_ERROR;
  }
  if (strlen(label) > NOTE_LABEL_MAX_LEN)
  {
    return ALGOIOT_BAD_PARAM;
  }

  m_noteJDoc[label] = value;
  
  // It is not trivial to anticipate how many chars we are going to add,
  // so we check JSON length after the fact
  len = m_noteOffset + measureJson(m_noteJDoc);
  if (len >= ALGORAND_MAX_NOTES_SIZE)
  {
    return ALGOIOT_DATA_STRUCTURE_TOO_LONG;
  }

  // Update note len
  m_noteLen = len;

  return ALGOIOT_NO_ERROR;
}

int AlgoIoT::dataAddShortStringField(const char* label, char* shortCString)
{
  int len = 0;

  if ( (label == NULL)||(shortCString == NULL) )
  {
    return ALGOIOT_NULL_POINTER_ERROR;
  }
  if ( (strlen(label) > NOTE_LABEL_MAX_LEN)||(strlen(shortCString) > NOTE_LABEL_MAX_LEN) )
  {
    return ALGOIOT_BAD_PARAM;
  }

  m_noteJDoc[label] = shortCString;
  
  // It is not trivial to anticipate how many chars we are going to add,
  // so we check JSON length after the fact
  len = m_noteOffset + measureJson(m_noteJDoc);
  if (len >= ALGORAND_MAX_NOTES_SIZE)
  {
    return ALGOIOT_DATA_STRUCTURE_TOO_LONG;
  }

  // Update note len
  m_noteLen = len;

  return ALGOIOT_NO_ERROR;
}

// Submit transaction to Algorand network
// Return: error code (0 = OK)
// We have the Note field ready, in ARC-2 JSON format
int AlgoIoT::submitTransactionToAlgorand()
{
  uint32_t fv = 0;
  uint16_t fee = 0;
  int iErr = 0;
  uint8_t signature[ALGORAND_SIG_BYTES];
  char notes[ALGORAND_MAX_NOTES_SIZE + 1] = "";
  uint8_t transactionMessagePackBuffer[ALGORAND_MAX_TX_MSGPACK_SIZE];
  char transactionID[ALGORAND_TRANSACTIONID_SIZE + 1];
  msgPack msgPackTx = NULL;

  
  // Add preamble to ARC-2 note field
  // Write app name and format specifier for ARC-2 (we use the JSON flavour of ARC-2)
  memcpy((void*)&(notes[0]), (void*)m_appName, strlen(m_appName));
  m_noteOffset = strlen(m_appName);
  notes[m_noteOffset++] = ':';
  notes[m_noteOffset++] = 'j';
  m_noteLen += m_noteOffset;

  // Serialize Note field to binary buffer after "<app-name>:j"
  int jlen = serializeJson(m_noteJDoc, (char*) (notes + m_noteOffset), ALGORAND_MAX_NOTES_SIZE);
  if (jlen < 1)
  {
    return ALGOIOT_JSON_ERROR;
  }
  int notesLen = jlen + m_noteOffset;

  // Get current Algorand parameters
  int httpResCode = getAlgorandTxParams(&fv, &fee);
  if (httpResCode != 200)
  {
    return ALGOIOT_NETWORK_ERROR;
  }

  // Prepare transaction structure as MessagePack
  msgPackTx = msgpackInit(&(transactionMessagePackBuffer[0]), ALGORAND_MAX_TX_MSGPACK_SIZE);
  if (msgPackTx == NULL)  
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.println("\n Error initializing transaction MessagePack\n");
    #endif
    return ALGOIOT_MESSAGEPACK_ERROR;
  }  
  iErr = prepareTransactionMessagePack(msgPackTx, fv, fee, PAYMENT_AMOUNT_MICROALGOS, notes, (uint16_t)notesLen);
  if (iErr)
  {
    return ALGOIOT_MESSAGEPACK_ERROR;
  }

  // Payment transaction correctly assembled. Now sign it
  iErr = signMessagePackAddingPrefix(msgPackTx, &(signature[0]));
  if (iErr)
  {
    return ALGOIOT_SIGNATURE_ERROR;
  }

  // Signed OK: now compose payload
  iErr = createSignedBinaryTransaction(msgPackTx, signature);
  if (iErr)
  {
    return ALGOIOT_INTERNAL_GENERIC_ERROR;
  }

  // Payload ready. Now we can submit it via algod REST API
  #ifdef LIB_DEBUGMODE
  DEBUG_SERIAL.println("\nReady to submit transaction to Algorand network");
  DEBUG_SERIAL.println();
  #endif
  iErr = submitTransaction(msgPackTx); // Returns HTTP code
  if (iErr != 200)  // 200 = HTTP OK
  { // Something went wrong
    return ALGOIOT_TRANSACTION_ERROR;
  }
  // OK: our transaction, carrying sensor data in the Note field, 
  // was successfully submitted to the Algorand blockchain
  #ifdef LIB_DEBUGMODE
  DEBUG_SERIAL.print("\t Transaction successfully submitted with ID=");
  DEBUG_SERIAL.println(getTransactionID());
  #endif
  
  return ALGOIOT_NO_ERROR;
}


///////////////////////////
//
// End exported functions
//
///////////////////////////


// Private methods

// Decodes Base64 Algorand network hash to 32-byte binary buffer suitable for our functions
// outBinaryHash has to be freed by caller
// Returns error code (0 = OK)
int AlgoIoT::decodeAlgorandNetHash(const char* hashB64, uint8_t*& outBinaryHash)
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


int AlgoIoT::decodeAlgorandAddress(const char* addressB32, uint8_t*& outBinaryAddress)
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


int AlgoIoT::decodePrivateKeyFromMnemonics(const char* inMnemonicWords, uint8_t privateKey[ALGORAND_KEY_BYTES])
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
  memcpy((void*)&(privateKey[0]), (void*)decodedBytes, ALGORAND_KEY_BYTES);

  return 0;
}


// Retrieves current Algorand transaction parameters
// Returns HTTP error code
// TODO: On error codes 5xx (server error), maybe we should retry after 5s?
int AlgoIoT::getAlgorandTxParams(uint32_t* round, uint16_t* minFee)
{
  String httpRequest = m_httpBaseURL + GET_TRANSACTION_PARAMS;
          
  *round = 0;
  *minFee = 0;

  // configure server and url               
  m_httpClient.begin(httpRequest);
    
  int httpResponseCode = m_httpClient.GET();

      
  // httpResponseCode will be negative on error
  if (httpResponseCode < 0)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.print("HTTP GET failed, error: "); DEBUG_SERIAL.println(m_httpClient.errorToString(httpResponseCode).c_str());
    #endif
    return ALGOIOT_INTERNAL_GENERIC_ERROR;
  }
  else
  {
    switch (httpResponseCode)
    {
      case 200:
      {   // No error: let's get the response
        String payload = m_httpClient.getString();
        StaticJsonDocument<ALGORAND_MAX_RESPONSE_LEN> JSONResDoc;
                        
        #ifdef LIB_DEBUGMODE
        DEBUG_SERIAL.println("GetParams server response:");
        DEBUG_SERIAL.println(payload);
        #endif

        DeserializationError error = deserializeJson(JSONResDoc, payload);                
        if (error) 
        {
          #ifdef LIB_DEBUGMODE
          DEBUG_SERIAL.println("GetParams: JSON response parsing failed!");
          #endif
          return ALGOIOT_INTERNAL_GENERIC_ERROR;
        }
        else
        { // Fetch interesting fields
          *minFee = JSONResDoc["min-fee"];
          *round = JSONResDoc["last-round"];

          #ifdef LIB_DEBUGMODE
          DEBUG_SERIAL.println("Algorand transaction parameters received:");
          DEBUG_SERIAL.print("min-fee = "); DEBUG_SERIAL.print(*minFee); DEBUG_SERIAL.println(" microAlgo");
          DEBUG_SERIAL.print("last-round = "); DEBUG_SERIAL.println(*round);                  
          #endif                  
        }
      }
      break;
      case 204:
      {   // No error, but no data available from server
        #ifdef LIB_DEBUGMODE
        DEBUG_SERIAL.println("Server returned no data");
        #endif
        return ALGOIOT_NETWORK_ERROR;
      }
      break;
      default:
      {
        #ifdef LIB_DEBUGMODE
        DEBUG_SERIAL.print("Unmanaged HTTP response code "); DEBUG_SERIAL.println(httpResponseCode);
        #endif
        return ALGOIOT_INTERNAL_GENERIC_ERROR;
      }
      break;
    }
  }        
  
  m_httpClient.end();

  return httpResponseCode;
}



// To be called AFTER getAlgorandTxParams(), because we need current "min-fee" and "last-round" values from algod
// Returns error code (0 = OK)
int AlgoIoT::prepareTransactionMessagePack(msgPack msgPackTx,
                                  const uint32_t lastRound, 
                                  const uint16_t fee, 
                                  const uint32_t paymentAmountMicroAlgos,
                                  const char* notes,
                                  const uint16_t notesLen)
{ 
  int iErr = 0;
  char gen[ALGORAND_NETWORK_ID_CHARS + 1] = "";
  uint32_t lv = lastRound + ALGORAND_MAX_WAIT_ROUNDS;
  const char type[] = "pay";
  uint8_t nFields = ALGORAND_PAYMENT_TRANSACTION_MIN_FIELDS;

  if (msgPackTx == NULL)
    return ALGOIOT_NULL_POINTER_ERROR;
  if (msgPackTx->msgBuffer == NULL)
    return ALGOIOT_INTERNAL_GENERIC_ERROR;
  if ((lastRound == 0) || (fee == 0) || (paymentAmountMicroAlgos < ALGORAND_MIN_PAYMENT_MICROALGOS))
  {
    return ALGOIOT_INTERNAL_GENERIC_ERROR;
  }
  
  if ( (notes != NULL) && (notesLen > 0) )
    nFields++;  // We have 9 fields without Note, 10 with Note

  if (m_networkType == ALGORAND_TESTNET)
  { // TestNet
    strncpy(gen, ALGORAND_TESTNET_ID, ALGORAND_NETWORK_ID_CHARS);
    // Decode Algorand network hash
    iErr = decodeAlgorandNetHash(ALGORAND_TESTNET_HASH, m_netHash);
    if (iErr)
    {
      #ifdef LIB_DEBUGMODE
      DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d decoding Algorand network hash\n\n", iErr);
      #endif
      return ALGOIOT_INTERNAL_GENERIC_ERROR;
    }
  }
  else
  { // MainNet
    strncpy(gen, ALGORAND_MAINNET_ID, ALGORAND_NETWORK_ID_CHARS);
    iErr = decodeAlgorandNetHash(ALGORAND_MAINNET_HASH, m_netHash);
    if (iErr)
    {
      #ifdef LIB_DEBUGMODE
      DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d decoding Algorand network hash\n\n", iErr);
      #endif
      return ALGOIOT_INTERNAL_GENERIC_ERROR;
    }
  }
  gen[ALGORAND_NETWORK_ID_CHARS] = '\0';


  // We leave a blank space header so we can add:
  // - "TX" prefix before signing
  // - m_signature field and "txn" node field after signing
  iErr = msgPackModifyCurrentPosition(msgPackTx, BLANK_MSGPACK_HEADER);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d from msgPackModifyCurrentPosition()\n\n");
    #endif

    return 5;
  }

  // Add root map
  iErr = msgpackAddShortMap(msgPackTx, nFields); 
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding root map\n\n");
    #endif

    return 5;
  }

  // Fields must follow alphabetical order

  // "amt" label
  iErr = msgpackAddShortString(msgPackTx, "amt");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding amt label\n\n");
    #endif

    return 5;
  }
  // amt value
  // Note: encoding depends on payment amount
  if (paymentAmountMicroAlgos < 128)
  {
    iErr = msgpackAddUInt7(msgPackTx, (uint8_t)paymentAmountMicroAlgos);
  }
  else
  {
    if (paymentAmountMicroAlgos < 256)
    {
      iErr = msgpackAddUInt8(msgPackTx, (uint8_t)paymentAmountMicroAlgos);
    }
    else
    {
      if (paymentAmountMicroAlgos < 65536)
      {
        iErr = msgpackAddUInt16(msgPackTx, (uint16_t)paymentAmountMicroAlgos);
      }
      else    
      {
        iErr = msgpackAddUInt32(msgPackTx, paymentAmountMicroAlgos);
      }
    }
  }
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding amt value\n\n");
    #endif

    return 5;
  }

  // "fee" label
  iErr = msgpackAddShortString(msgPackTx, "fee");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding fee label\n\n");
    #endif

    return 5;
  }
  // fee value
  iErr = msgpackAddUInt16(msgPackTx, fee);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding fee value\n\n");
    #endif

    return 5;
  }

  // "fv" label
  iErr = msgpackAddShortString(msgPackTx, "fv");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding fv label\n\n");
    #endif

    return 5;
  }
  // fv value
  iErr = msgpackAddUInt32(msgPackTx, lastRound);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding fv value\n\n");
    #endif

    return 5;
  }

  // "gen" label
  iErr = msgpackAddShortString(msgPackTx, "gen");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding gen label\n\n");
    #endif

    return 5;
  }
  // gen string
  iErr = msgpackAddShortString(msgPackTx, gen);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding gen string\n\n");
    #endif

    return 5;
  }

  // "gh" label
  iErr = msgpackAddShortString(msgPackTx, "gh");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding gh label\n\n");
    #endif

    return 5;
  }
  // gh value (binary buffer)
  iErr = msgpackAddShortByteArray(msgPackTx, (const uint8_t*)&(m_netHash[0]), (const uint8_t)ALGORAND_NET_HASH_BYTES);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding gh value\n\n");
    #endif

    return 5;
  }

  // "lv" label
  iErr = msgpackAddShortString(msgPackTx, "lv");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding lv label\n\n");
    #endif

    return 5;
  }
  // lv value
  iErr = msgpackAddUInt32(msgPackTx, lv);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding lv value\n\n");
    #endif

    return 5;
  }

  if ((notes != NULL) && (notesLen > 0))
  {
    // Add "note" label
    iErr = msgpackAddShortString(msgPackTx, "note");
    if (iErr)
    {
      #ifdef LIB_DEBUGMODE
      DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding note label\n\n");
      #endif

      return 5;
    }
    // Add note content as binary buffer
    // WARNING: if note len is < 256, we have to encode Bin 8 so msgpackAddShortByteArray
    // Otherwise, m_signature does not pass verification
    if (notesLen < 256)
      iErr = msgpackAddShortByteArray(msgPackTx, (const uint8_t*)notes, (const uint8_t)notesLen);    
    else
      iErr = msgpackAddByteArray(msgPackTx, (const uint8_t*)notes, (const uint16_t)notesLen);
    if (iErr)
    {
      #ifdef LIB_DEBUGMODE
      DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding notes content\n\n");
      #endif

      return 5;
    }
  }

  // "rcv" label
  iErr = msgpackAddShortString(msgPackTx, "rcv");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding rcv label\n\n");
    #endif

    return 5;
  }
  // rcv value (binary buffer)
  iErr = msgpackAddShortByteArray(msgPackTx, (const uint8_t*)&(m_receiverAddressBytes[0]), (const uint8_t)ALGORAND_ADDRESS_BYTES);  
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding rcv value\n\n");
    #endif

    return 5;
  }

  // "snd" label
  iErr = msgpackAddShortString(msgPackTx, "snd");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding snd label\n\n");
    #endif

    return 5;
  }
  // snd value (binary buffer)
  iErr = msgpackAddShortByteArray(msgPackTx, (const uint8_t*)&(m_senderAddressBytes[0]), (const uint8_t)ALGORAND_ADDRESS_BYTES);  
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding snd value\n\n");
    #endif

    return 5;
  }

  // "type" label
  iErr = msgpackAddShortString(msgPackTx, "type");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding type label\n\n");
    #endif

    return 5;
  }
  // type string
  iErr = msgpackAddShortString(msgPackTx, "pay");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n prepareTransactionMessagePack(): ERROR %d adding type string\n\n");
    #endif

    return 5;
  }

  // End of messagepack

  return 0;
}



// Obtains Ed25519 signature of passed MessagePack, adding "TX" prefix; fills "signature" return buffer
// To be called AFTER convertToMessagePack()
// Returns error code (0 = OK)
// Caller passes a 64-byte array in "signature", to be filled
int AlgoIoT::signMessagePackAddingPrefix(msgPack msgPackTx, uint8_t signature[ALGORAND_SIG_BYTES])
{
  uint8_t* payloadPointer = NULL;
  uint32_t payloadBytes = 0;

  if (msgPackTx == NULL)
    return 1;
  if (msgPackTx->msgBuffer == NULL)
    return 2;
  if (msgPackTx->currentMsgLen == 0)
    return 2;

  // We sign from prefix (included), leaving out the rest of the blank header
  payloadPointer = msgPackTx->msgBuffer + BLANK_MSGPACK_HEADER - ALGORAND_TRANSACTION_PREFIX_BYTES;
  payloadBytes = msgPackTx->currentMsgLen + ALGORAND_TRANSACTION_PREFIX_BYTES;

  // Add prefix to messagepack; we purposedly left a blank header, with length BLANK_MSGPACK_HEADER
  payloadPointer[0] = 'T';
  payloadPointer[1] = 'X';

  // Sign pack+prefix
  Ed25519::sign(signature, m_privateKey, m_senderAddressBytes, payloadPointer, payloadBytes);

  return 0;
}


// Add signature to MessagePack. We reserved a blank space header for this purpose
// To be called AFTER signMessagePackAddingPrefix()
// Returns error code (0 = OK)
int AlgoIoT::createSignedBinaryTransaction(msgPack mPack, const uint8_t signature[ALGORAND_SIG_BYTES])
{
  int iErr = 0;
  // When adding the m_signature "sig" field, the messagepack has to be changed into a 2-level structure,
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

  // We reset internal msgpack pointer, since we now start from the beginning of the messagepack (filling the blank space)
  iErr = msgPackModifyCurrentPosition(mPack, 0);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n createSignedBinaryTransaction(): ERROR %d resetting position\n\n");
    #endif

    return 5;
  }

  // Add a Map holding 2 fields (sig and txn)
  iErr = msgpackAddShortMap(mPack, 2);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n createSignedBinaryTransaction(): ERROR %d adding map\n\n");
    #endif

    return 5;
  }
  
  // Add "sig" label
  iErr = msgpackAddShortString(mPack, "sig");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n createSignedBinaryTransaction(): ERROR %d adding sign label\n\n");
    #endif

    return 5;
  }

  // Add m_signature, which is a 64-bytes byte array 
  iErr = msgpackAddShortByteArray(mPack, signature, (const uint8_t)ALGORAND_SIG_BYTES);
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n createSignedBinaryTransaction(): ERROR %d adding m_signature\n\n");
    #endif

    return 5;
  }

  // Add "txn" label
  iErr = msgpackAddShortString(mPack, "txn");
  if (iErr)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.printf("\n createSignedBinaryTransaction(): ERROR %d adding txn label\n\n");
    #endif

    return 5;
  }

  // The rest stays as it is

  return 0;
}


// Submits transaction messagepack to algod
// Last method to be called, after all the others
// Returns http response code (200 = OK) or AlgoIoT error code
// TODO: On error codes 5xx (server error), maybe we should retry after 5s?
int AlgoIoT::submitTransaction(msgPack msgPackTx)
{
  String httpRequest = m_httpBaseURL + POST_TRANSACTION;
          
  // Configure server and url               
  m_httpClient.begin(httpRequest);
  
  // Configure MIME type
  m_httpClient.addHeader("Content-Type", ALGORAND_POST_MIME_TYPE);

  int httpResponseCode = m_httpClient.POST(msgPackTx->msgBuffer, msgPackTx->currentMsgLen);
      
  // httpResponseCode will be negative on error
  if (httpResponseCode < 0)
  {
    #ifdef LIB_DEBUGMODE
    DEBUG_SERIAL.print("\n[HTTP] POST failed, error: "); DEBUG_SERIAL.println(m_httpClient.errorToString(httpResponseCode).c_str());
    #endif
  }
  else
  {
    switch (httpResponseCode)
    {
      case 200:
      {   // No error: let's get the response for debug purposes
        String payload = m_httpClient.getString();
        StaticJsonDocument<ALGORAND_MAX_RESPONSE_LEN> JSONResDoc;
                        
        DeserializationError error = deserializeJson(JSONResDoc, payload);                
        if (error) 
        {
          #ifdef LIB_DEBUGMODE
          DEBUG_SERIAL.println("JSON response parsing failed!");
          #endif
          return ALGOIOT_INTERNAL_GENERIC_ERROR;
        }
        else
        { // Fetch interesting fields                  
          strncpy(m_transactionID, JSONResDoc["txId"], 64);
        }
      }
      break;
      case 204:
      {   // No error, but no data available from server
        #ifdef LIB_DEBUGMODE
        DEBUG_SERIAL.println("\nServer returned no data");
        #endif
        return ALGOIOT_NETWORK_ERROR;
      }
      break;
      case 400:
      {   // Malformed request
        #ifdef LIB_DEBUGMODE
        DEBUG_SERIAL.println("\nTransaction format error");
        DEBUG_SERIAL.println("Server response:");
        String payload = m_httpClient.getString();
        DEBUG_SERIAL.println(payload);
        #endif
        return ALGOIOT_TRANSACTION_ERROR;
      }
      break;
      default:
      {
        #ifdef LIB_DEBUGMODE
        DEBUG_SERIAL.print("\nUnmanaged HTTP response code "); DEBUG_SERIAL.println(httpResponseCode);
        #endif
        return ALGOIOT_INTERNAL_GENERIC_ERROR;
      }
      break;
    }
  }        
  
  m_httpClient.end();

  return httpResponseCode;
}
