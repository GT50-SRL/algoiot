// minmpk.h
// header for minimal messagepack builder
// v20231012-2

// TODO:
//  Add more types

// By Fernando Carello for GT50
// Released under MIT license:

/* 
Copyright 2023 GT50 S.r.l.
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/


#ifndef __MINMPK_H
#define __MINMPK_H

#include <stdint.h>

// Error codes
#define MPK_NO_ERROR 0
#define MPK_ERR_NULL_MPACK 1
#define MPK_ERR_NULL_INTERNAL_BUFFER 2
#define MPK_ERR_BAD_PARAM 3
#define MPK_ERR_BUFFER_TOO_SHORT 4

// #define IS_BIG_ENDIAN  // Don't know of big-endian MCUs; in case, uncomment

// Typedefs
typedef struct mpkStruct
{
  uint8_t* msgBuffer;
  uint32_t bufferLen;
  uint32_t currentMsgLen;
  uint32_t currentPosition;
} mpkStruct;

typedef mpkStruct* msgPack;
// End typedefs


// To be called only once for each MessagePack
// Buffer (static or dynamic) has to be passed by caller, and then freed by caller if appropriate. Needs to be "large enough"
msgPack msgpackInit(uint8_t* buffer, const uint32_t bufferLen);

// Please note it does *not* free the buffer passed via msgpackInit()
// Returns error code (0 = OK)
int msgPackFree(msgPack mPack);

int msgPackModifyCurrentPosition(msgPack mPack, const uint32_t newPosition);

uint8_t* msgPackGetBuffer(msgPack mPack);

uint32_t msgPackGetLen(msgPack mPack);

// "fields" max value = 15
// Returns error code (0 = OK)
int msgpackAddShortMap(msgPack mPack, const uint8_t fields);

// Up to 31 single-byte chars (32 including trailing NULL, which will *not* be encoded)
// Returns error code (0 = OK)
int msgpackAddShortString(msgPack mPack, const char* string);

// Returns error code (0 = OK)
int msgpackAddUInt7(msgPack mPack, const uint8_t value);

// Returns error code (0 = OK)
int msgpackAddInt8(msgPack mPack, const int8_t value);

// Returns error code (0 = OK)
int msgpackAddUInt8(msgPack mPack, const uint8_t value);

// Returns error code (0 = OK)
int msgpackAddInt16(msgPack mPack, const int16_t value);

// Returns error code (0 = OK)
int msgpackAddUInt16(msgPack mPack, const uint16_t value);

// Returns error code (0 = OK)
int msgpackAddInt32(msgPack mPack, const int32_t value);

// Returns error code (0 = OK)
int msgpackAddUInt32(msgPack mPack, const uint32_t value);

// Returns error code (0 = OK)
int msgpackAddFloat(msgPack mPack, const float value);

// Max 255 bytes
// Returns error code (0 = OK)
int msgpackAddShortByteArray(msgPack mPack, const uint8_t* inputArray, const uint8_t inputBytes);

// Max 65535 bytes
// Returns error code (0 = OK)
int msgpackAddByteArray(msgPack mPack, const uint8_t* inputArray, const uint16_t inputBytes);



#endif
