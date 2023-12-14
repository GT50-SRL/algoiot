/*
Base32 Decode as in http://tools.ietf.org/html/rfc4648
Derived from the work of Vladimir Tarasow
Released into the public domain.

Last mod 20231003-1
*/

#include "base32decode.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


int Base32::fromBase32(uint8_t* in, const int length, uint8_t*& out)
{
  int result = 0; // Length of the array of decoded values.
  int buffer = 0;
  int bitsLeft = 0;
  uint8_t* temp = NULL;

  if (in == NULL)
	return 0;
  if (length < 1)
    return 0;

  temp = (uint8_t*)malloc(length); // Allocating temporary array
  if (temp == NULL)
    return 0;

  for (int i = 0; i < length; i++)
  {
    uint8_t ch = in[i];

    // ignoring some characters: ' ', '\t', '\r', '\n', '='
    if (ch == 0xA0 || ch == 0x09 || ch == 0x0A || ch == 0x0D || ch == 0x3D) continue;

    // recovering mistyped: '0' -> 'O', '1' -> 'L', '8' -> 'B'
    if (ch == 0x30) { ch = 0x4F; } else if (ch == 0x31) { ch = 0x4C; } else if (ch == 0x38) { ch = 0x42; }
    
    // look up one base32 symbols: from 'A' to 'Z' or from 'a' to 'z' or from '2' to '7'
    if ((ch >= 0x41 && ch <= 0x5A) || (ch >= 0x61 && ch <= 0x7A)) { ch = ((ch & 0x1F) - 1); }
    else if (ch >= 0x32 && ch <= 0x37) { ch -= (0x32 - 26); }
    else { free(temp); return 0; }

    buffer <<= 5;    
    buffer |= ch;
    bitsLeft += 5;
    if (bitsLeft >= 8)
    {
      temp[result] = (unsigned char)((unsigned int)(buffer >> (bitsLeft - 8)) & 0xFF);
      result++;
      bitsLeft -= 8;
    }
  }

  out = (uint8_t*)malloc(result);
  if (out == NULL)
    return 0;

  memcpy(out, temp, result);
  free(temp);

  return result;
}
