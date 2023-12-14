/*
  Base32 decoding (http://tools.ietf.org/html/rfc4648)
  Derived from the work of Vladimir Tarasow
  Released into the public domain.
*/

#ifndef _Base32_h
#define _Base32_h

#include "stdint.h"

class Base32
{
  public:
    /// @brief Decodes from Base32 buffer
    /// @param in Base32 buffer
    /// @param length Base32 buffer length
    /// @param out Decoded buffer, allocated internally (to be freed by caller)
    /// @return length of decoded buffer (0 if error occurred)
    static int fromBase32(uint8_t* in, const int length, uint8_t*& out);
};

#endif
