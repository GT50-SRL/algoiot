// minmpk.cpp
// minimal messagepack builder straight from the specs at https://github.com/msgpack/msgpack/blob/master/spec.md
// W.I.P. use with care
// In C because we need it on C-only platforms too
// v20231012-1

// TODO test floats and signed ints

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
#include "minmpk.h"


msgPack msgpackInit(uint8_t* buffer, const uint32_t bufferLen)
{
  msgPack mPack = NULL;

  if ((buffer == NULL) || (bufferLen == 0))
    return NULL;

  mPack = (msgPack) malloc(sizeof(mpkStruct));
  if (!mPack)
  {
    return NULL;
  }

  mPack->msgBuffer = buffer;
  mPack->bufferLen = bufferLen;
  mPack->currentMsgLen = 0;
  mPack->currentPosition = 0;

  return mPack;
}


int msgPackFree(msgPack mPack)
{
  if (mPack == NULL)
    return MPK_ERR_NULL_MPACK;
  
  free(mPack);
  mPack = NULL;

  return MPK_NO_ERROR;
}


int msgPackModifyCurrentPosition(msgPack mPack, const uint32_t newPosition)
{
  if (mPack == NULL)
    return MPK_ERR_NULL_MPACK;
  if (newPosition >= mPack->bufferLen)
    return MPK_ERR_BAD_PARAM;

  mPack->currentPosition = newPosition;

  return 0;
}


uint8_t* msgPackGetBuffer(msgPack mPack)
{
  return mPack->msgBuffer;
}


uint32_t msgPackGetLen(msgPack mPack)
{
  if (mPack->msgBuffer == NULL)
    return 0;

  return mPack->currentMsgLen;
}


int msgpackAddShortMap(msgPack mPack, const uint8_t nFields)
{
  uint8_t specifier = 0;

  if (mPack == NULL)
  {
    return MPK_ERR_NULL_MPACK;
  }
  if (mPack->msgBuffer == NULL)
  {
    return MPK_ERR_NULL_INTERNAL_BUFFER;
  }
  if (nFields > 15)
  {
    return MPK_ERR_BAD_PARAM;
  }
  if (mPack->currentPosition + 1 >= mPack->bufferLen)
  {
    return MPK_ERR_BUFFER_TOO_SHORT;
  }

  // Our map will max 15 fields, so we can use a FixMap (https://github.com/msgpack/msgpack/blob/master/spec.md#map-format-family)
  // FixMap specifier = 1000xxxx where xxxx are 4 bits keeping the number of fields
  // So for example with N = 9 -> xxxx = 1001 -> specifier = 10001001 = 0x89 = 137
  specifier = 128 + nFields; // 10000000 + 4-bit totalFields
  mPack->msgBuffer[mPack->currentPosition++] = specifier;
  mPack->currentMsgLen++;
  
  return 0;
}


int msgpackAddShortString(msgPack mPack, const char* string)
{
  uint32_t len = 0;
  uint8_t pos = 0;
  uint8_t specifier = 0;

  if (mPack == NULL)
  {
    return MPK_ERR_NULL_MPACK;
  }
  if (mPack->msgBuffer == NULL)
  {
    return MPK_ERR_NULL_INTERNAL_BUFFER;
  }
  if (string == NULL)
  {
    return MPK_ERR_BAD_PARAM;
  }

  len = strlen(string);

  if (len > 31)
  {
    return MPK_ERR_BAD_PARAM;
  }
  if (mPack->currentPosition + len + 1 >= mPack->bufferLen)
  {
    return MPK_ERR_BUFFER_TOO_SHORT;
  }

  // We can use a Fixstr (https://github.com/msgpack/msgpack/blob/master/spec.md#str-format-family)
  // Fixstr specifier = 101XXXXX (5 bits of string len)
  // Ex. N = 3 -> 10100011 = 0xA3
  specifier = 160 + (uint8_t) len; // 10100000 + 5-bit len
  mPack->msgBuffer[mPack->currentPosition++] = specifier;
  for (pos = 0; pos < len; pos++)
  {
    mPack->msgBuffer[mPack->currentPosition++] = (uint8_t)string[pos];
  }
  mPack->currentMsgLen += len + 1;
  
  return 0;
}


int msgpackAddUInt7(msgPack mPack, const uint8_t value)
{
  if (mPack == NULL)
  {
    return MPK_ERR_NULL_MPACK;
  }
  if (mPack->msgBuffer == NULL)
  {
    return MPK_ERR_NULL_INTERNAL_BUFFER;
  }
  if (mPack->currentPosition + 1 >= mPack->bufferLen)
  {
    return MPK_ERR_BUFFER_TOO_SHORT;
  }

  // We use "positive fixint" encoding https://github.com/msgpack/msgpack/blob/master/spec.md#int-format-family
  mPack->msgBuffer[mPack->currentPosition++] = value & 0x7F;

  mPack->currentMsgLen += 1;

  return 0;
}


int msgpackAddInt8(msgPack mPack, const int8_t value)
{
  const uint8_t specifier = 0xD0;

  if (mPack == NULL)
  {
    return MPK_ERR_NULL_MPACK;
  }
  if (mPack->msgBuffer == NULL)
  {
    return MPK_ERR_NULL_INTERNAL_BUFFER;
  }
  if (mPack->currentPosition + 2 >= mPack->bufferLen)
  {
    return MPK_ERR_BUFFER_TOO_SHORT;
  }

  // We use "int 8" encoding https://github.com/msgpack/msgpack/blob/master/spec.md#int-format-family
  mPack->msgBuffer[mPack->currentPosition++] = specifier;
  mPack->msgBuffer[mPack->currentPosition++] = value;

  mPack->currentMsgLen += 2;

  return 0;
}


int msgpackAddUInt8(msgPack mPack, const uint8_t value)
{
  const uint8_t specifier = 0xCC;

  if (mPack == NULL)
  {
    return MPK_ERR_NULL_MPACK;
  }
  if (mPack->msgBuffer == NULL)
  {
    return MPK_ERR_NULL_INTERNAL_BUFFER;
  }
  if (mPack->currentPosition + 2 >= mPack->bufferLen)
  {
    return MPK_ERR_BUFFER_TOO_SHORT;
  }

  // We use "uint 8" encoding https://github.com/msgpack/msgpack/blob/master/spec.md#int-format-family
  mPack->msgBuffer[mPack->currentPosition++] = specifier;
  mPack->msgBuffer[mPack->currentPosition++] = value;

  mPack->currentMsgLen += 2;

  return 0;
}


int msgpackAddInt16(msgPack mPack, const int16_t value)
{
  const uint8_t specifier = 0xD1;

  if (mPack == NULL)
  {
    return MPK_ERR_NULL_MPACK;
  }
  if (mPack->msgBuffer == NULL)
  {
    return MPK_ERR_NULL_INTERNAL_BUFFER;
  }
  if (mPack->currentPosition + 3 >= mPack->bufferLen)
  {
    return MPK_ERR_BUFFER_TOO_SHORT;
  }

  // We use "int 16" encoding https://github.com/msgpack/msgpack/blob/master/spec.md#int-format-family
  mPack->msgBuffer[mPack->currentPosition++] = specifier;
  #ifdef IS_BIG_ENDIAN
  memcpy((void*) &(mPack->msgBuffer[mPack->currentPosition]), (void*)&value, 2);
  mPack->currentPosition += 2;
  #else
  mPack->msgBuffer[mPack->currentPosition++] = (uint8_t)((value & 0xFF00) >> 8);
  mPack->msgBuffer[mPack->currentPosition++] = (uint8_t)((value & 0x00FF));
  #endif

  mPack->currentMsgLen += 3;

  return 0;
}


int msgpackAddUInt16(msgPack mPack, const uint16_t value)
{
  const uint8_t specifier = 0xCD;

  if (mPack == NULL)
  {
    return MPK_ERR_NULL_MPACK;
  }
  if (mPack->msgBuffer == NULL)
  {
    return MPK_ERR_NULL_INTERNAL_BUFFER;
  }
  if (mPack->currentPosition + 3 >= mPack->bufferLen)
  {
    return MPK_ERR_BUFFER_TOO_SHORT;
  }

  // We use "uint 16" encoding https://github.com/msgpack/msgpack/blob/master/spec.md#int-format-family
  mPack->msgBuffer[mPack->currentPosition++] = specifier;
  #ifdef IS_BIG_ENDIAN
  memcpy((void*) &(mPack->msgBuffer[mPack->currentPosition]), (void*)&value, 2);
  mPack->currentPosition += 2;
  #else
  mPack->msgBuffer[mPack->currentPosition++] = (uint8_t)((value & 0xFF00) >> 8);
  mPack->msgBuffer[mPack->currentPosition++] = (uint8_t)((value & 0x00FF));
  #endif

  mPack->currentMsgLen += 3;

  return 0;
}


int msgpackAddInt32(msgPack mPack, const int32_t value)
{
  const uint8_t specifier = 0xD2;

  if (mPack == NULL)
  {
    return MPK_ERR_NULL_MPACK;
  }
  if (mPack->msgBuffer == NULL)
  {
    return MPK_ERR_NULL_INTERNAL_BUFFER;
  }
  if (mPack->currentPosition + 5 >= mPack->bufferLen)
  {
    return MPK_ERR_BUFFER_TOO_SHORT;
  }

  // We use "int 32" encoding https://github.com/msgpack/msgpack/blob/master/spec.md#int-format-family
  mPack->msgBuffer[mPack->currentPosition++] = specifier;
  #ifdef IS_BIG_ENDIAN
  memcpy((void*) &(mPack->msgBuffer[mPack->currentPosition]), (void*)&value, 4);
  mPack->currentPosition += 4;
  #else
  mPack->msgBuffer[mPack->currentPosition++] = (uint8_t)((value & 0xFF000000) >> 24);
  mPack->msgBuffer[mPack->currentPosition++] = (uint8_t)((value & 0x00FF0000) >> 16);
  mPack->msgBuffer[mPack->currentPosition++] = (uint8_t)((value & 0x0000FF00) >> 8);
  mPack->msgBuffer[mPack->currentPosition++] = (uint8_t)((value & 0x000000FF));
  #endif

  mPack->currentMsgLen += 5;

  return 0;
}


int msgpackAddUInt32(msgPack mPack, const uint32_t value)
{
  const uint8_t specifier = 0xCE;

  if (mPack == NULL)
  {
    return MPK_ERR_NULL_MPACK;
  }
  if (mPack->msgBuffer == NULL)
  {
    return MPK_ERR_NULL_INTERNAL_BUFFER;
  }
  if (mPack->currentPosition + 5 >= mPack->bufferLen)
  {
    return MPK_ERR_BUFFER_TOO_SHORT;
  }

  // We use "uint 32" encoding https://github.com/msgpack/msgpack/blob/master/spec.md#int-format-family
  mPack->msgBuffer[mPack->currentPosition++] = specifier;
  #ifdef IS_BIG_ENDIAN
  memcpy((void*) &(mPack->msgBuffer[mPack->currentPosition]), (void*)&value, 4);
  mPack->currentPosition += 4;
  #else
  mPack->msgBuffer[mPack->currentPosition++] = (uint8_t)((value & 0xFF000000) >> 24);
  mPack->msgBuffer[mPack->currentPosition++] = (uint8_t)((value & 0x00FF0000) >> 16);
  mPack->msgBuffer[mPack->currentPosition++] = (uint8_t)((value & 0x0000FF00) >> 8);
  mPack->msgBuffer[mPack->currentPosition++] = (uint8_t)((value & 0x000000FF));
  #endif

  mPack->currentMsgLen += 5;

  return 0;
}


int msgpackAddFloat(msgPack mPack, const float value)
{
  const uint8_t specifier = 0xCA;
  uint8_t floatBytes[4];

  if (mPack == NULL)
  {
    return MPK_ERR_NULL_MPACK;
  }
  if (mPack->msgBuffer == NULL)
  {
    return MPK_ERR_NULL_INTERNAL_BUFFER;
  }
  if (mPack->currentPosition + 5 >= mPack->bufferLen)
  {
    return MPK_ERR_BUFFER_TOO_SHORT;
  }

  // We use "float" encoding https://github.com/msgpack/msgpack/blob/master/spec.md#float-format-family
  mPack->msgBuffer[mPack->currentPosition++] = specifier;
  #ifdef IS_BIG_ENDIAN
  memcpy((void*) &(mPack->msgBuffer[mPack->currentPosition]), (void*)&value, 4);
  mPack->currentPosition += 4;
  #else
  memcpy((void*) &(floatBytes[0]), (void*)&value, 4);
  mPack->msgBuffer[mPack->currentPosition++] = floatBytes[3];
  mPack->msgBuffer[mPack->currentPosition++] = floatBytes[2];
  mPack->msgBuffer[mPack->currentPosition++] = floatBytes[1];
  mPack->msgBuffer[mPack->currentPosition++] = floatBytes[0];
  #endif

  mPack->currentMsgLen += 5;

  return 0;
}


// Max 255 bytes
int msgpackAddShortByteArray(msgPack mPack, const uint8_t* inputArray, const uint8_t inputBytes)
{
  const uint8_t specifier = 0xC4;

  if (mPack == NULL)
  {
    return MPK_ERR_NULL_MPACK;
  }
  if (mPack->msgBuffer == NULL)
  {
    return MPK_ERR_NULL_INTERNAL_BUFFER;
  }
  if (inputArray == NULL)
  {
    return MPK_ERR_BAD_PARAM;
  }
  if (mPack->currentPosition + inputBytes + 2 >= mPack->bufferLen)
  {
    return MPK_ERR_BUFFER_TOO_SHORT;
  }

  // It fits into "bin 8" encoding https://github.com/msgpack/msgpack/blob/master/spec.md#bin-format-family
  // Format specifier = 0xC4
  // First byte (len) = inputBytes
  mPack->msgBuffer[mPack->currentPosition++] = specifier;
  mPack->msgBuffer[mPack->currentPosition++] = inputBytes;
  memcpy((void*) &(mPack->msgBuffer[mPack->currentPosition]), (void*)inputArray, inputBytes);
  mPack->currentPosition += inputBytes;

  mPack->currentMsgLen += inputBytes + 2;

  return 0;
}


// Max 65535 bytes
int msgpackAddByteArray(msgPack mPack, const uint8_t* inputArray, const uint16_t inputBytes)
{ 
  const uint8_t specifier = 0xC5;

  if (mPack == NULL)
  {
    return MPK_ERR_NULL_MPACK;
  }
  if (mPack->msgBuffer == NULL)
  {
    return MPK_ERR_NULL_INTERNAL_BUFFER;
  }
  if (inputArray == NULL)
  {
    return MPK_ERR_BAD_PARAM;
  }
  if (mPack->currentPosition + inputBytes + 3 >= mPack->bufferLen)
  {
    return MPK_ERR_BUFFER_TOO_SHORT;
  }

  // It fits into "bin 16" encoding https://github.com/msgpack/msgpack/blob/master/spec.md#bin-format-family
  // Format specifier = 0xC5
  // Then 2 bytes = inputBytes, as big endian
  mPack->msgBuffer[mPack->currentPosition++] = specifier;
  #ifdef IS_BIG_ENDIAN
  memcpy((void*) &(mPack->msgBuffer[mPack->currentPosition]), (void*)&inputBytes, 2);
  mPack->currentPosition += 2;
  #else
  mPack->msgBuffer[mPack->currentPosition++] = (uint8_t)((inputBytes & 0xFF00) >> 8);
  mPack->msgBuffer[mPack->currentPosition++] = (uint8_t)((inputBytes & 0x00FF));
  #endif
  memcpy((void*) &(mPack->msgBuffer[mPack->currentPosition]), (void*)inputArray, inputBytes);
  mPack->currentPosition += inputBytes;

  mPack->currentMsgLen += inputBytes + 3;

  return 0;
}
