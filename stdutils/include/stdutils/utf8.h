// Copyright (c) 2024 Pierre DEJOUE
// This code is distributed under the terms of the MIT License
#pragma once

#include <string>

/**
 * Support for UTF8 strings
 *
 * The proposal in stdutils is to only add a syntactic sugar around regular std::strings
 * (and const char*) to convey the message that they should be UTF8 encoded.
 * We deliberately do not use char8_t available in C++20.
 *
 * Read:
 *  - https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p1423r2.html (char8_t backward compatibility remediation)
 */
namespace stdutils {

using u8_char_t = char;
using u8_string = std::string;
using u8_c_str  = const char*;                   // i.e. A regular null terminated C string

// Conversion from u8"" literals
inline u8_c_str to_u8_c_str(const char* u8_literal)
{
    return u8_literal;
}
inline u8_string to_u8_string(const char* u8_literal)
{
    return u8_string(u8_literal);
}
#ifdef __cpp_char8_t
    // C++20 adds the char8_t type which is similar to an unsigned char
    inline u8_c_str to_u8_c_str(const char8_t* u8_literal)
    {
        return reinterpret_cast<const u8_char_t*>(u8_literal);
    }
    inline u8_string to_u8_string(const char8_t* u8_literal)
    {
        return u8_string(reinterpret_cast<const u8_char_t*>(u8_literal));
    }
#endif

} // namespace stdutils
