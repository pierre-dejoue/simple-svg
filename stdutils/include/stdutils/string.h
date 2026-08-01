// Copyright (c) 2021 Pierre DEJOUE
// This code is distributed under the terms of the MIT License
#pragma once

#include <stdutils/utf8.h>

#include <cstdlib>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace stdutils {

namespace ascii {

/**
 * ASCII characters manipulation
 *
 * Contrary to the std::isalpha, etc. functions, the locale is ignored.
 * Only the ASCII characters are considered.
 */
bool isalpha(char c) noexcept;
bool isalnum(char c) noexcept;
bool isprint(char c) noexcept;
bool isspace(char c) noexcept;
bool isupper(char c) noexcept;
bool islower(char c) noexcept;
char tolower(char c) noexcept;
char toupper(char c) noexcept;

// Escape sequence, e.g. \x0A for '\t'
struct HexEscape
{
    char c;
};
std::ostream& operator<<(std::ostream& out, const HexEscape& hex_escape);

} // namespace ascii

/**
 * String length with a boundary size
 *
 * Note: If strnlen returns max_len, the null-termination character wasn't found
 */
constexpr std::size_t DEFAULT_MAX_LEN = 1048576u;       // 2^20
std::size_t strnlen(const char* str, std::size_t max_len = DEFAULT_MAX_LEN);

namespace string {

/**
 * String conversion functions
 */
std::string tolower(std::string_view in);
std::string toupper(std::string_view in);
std::string capitalize(std::string_view in);
std::string remove_all_spaces(std::string_view in);

/**
 * Validate that a string is null terminated
 */
bool is_null_terminated(const char* str, std::size_t max_len = DEFAULT_MAX_LEN);

/**
 * Validate that a string is pure ASCII (1-127), and null terminated
 */
bool is_pure_ascii(const char* str, std::size_t max_len = DEFAULT_MAX_LEN);
bool is_pure_ascii(const std::string& str);

/**
 * Validate that a string is made of printable ASCII chars in the strict sense (32-127), and null terminated
 */
bool is_strictly_print_ascii(const char* str, std::size_t max_len = DEFAULT_MAX_LEN);
bool is_strictly_print_ascii(const std::string& str);

/**
 * Validate that a string is made of printable ASCII chars (32-127) and whitespaces (including '\n'), and null terminated
 */
bool is_printable_ascii(const char* str, std::size_t max_len = DEFAULT_MAX_LEN);
bool is_printable_ascii(const std::string& str);

/**
 * A string identifier
 *
 * It shall only contain the characters: 'a-z', 'A-Z', '0-9', '#', '.', '-', '_'
 * It shall not be empty and begin with an alphabetical character
 */
using Id = const char*;

bool is_valid_id(Id id);

/**
 * UTF8 utility functions:
 *  - Validate the string is well-formed
 *  - Number of characters
 */
bool is_well_formed_utf8(u8_c_str u8_str, std::size_t* error_loc = nullptr, std::size_t max_len = DEFAULT_MAX_LEN);
bool is_well_formed_utf8(const u8_string& u8_str, std::size_t* error_loc = nullptr);
std::size_t count_utf8_chars(u8_c_str u8_str, std::size_t max_len = DEFAULT_MAX_LEN);
std::size_t count_utf8_chars(const u8_string& u8_str);

/**
 * Split strings
 *
 * l_split: left split.  Split on the first delimiter from the left.  For example: l_split("a,b,c", ',') returns { "a", "b,c" };
 * r_split: right split. Split on the first delimiter from the right. For example: r_split("a,b,c", ',') returns { "a,b", "c" };
 * split: split on all delimiters. A few examples:
 *                  split("a,b,c",   ',') returns { "a", "b", "c" }
 *                  split("a,,b,c",  ',') returns { "a", "", "b", "c" }      // Empty strings between delimiters are kept
 *                  split(",a,b,c,", ',') returns { "", "a", "b", "c", "" }  // Delimiters at the beginning and end matter
 *
 * split_skip_empty is a variant of skip that will leave out the empty strings. It will return { "a", "b", "c" } in all the examples above.
 *
 * If we compare the implementation to Python:
 *  - l_split is equivalent to str.split(delim, maxsplit=1)
 *  - r_split is equivalent to str.rsplit(delim, maxsplit=1)
 *  - split is equivalent to str.split(delim)
 */
std::pair<std::string_view, std::string_view> l_split(std::string_view in_str, char delim);
std::pair<std::string_view, std::string_view> r_split(std::string_view in_str, char delim);
std::vector<std::string_view> split(std::string_view in_str, char delim);
std::vector<std::string_view> split_skip_empty(std::string_view in_str, char delim);

/**
 * String replacement. Output boolean indicates if the replacement took place as expected.
 */
std::string replace_first(std::string_view src, std::string_view from, std::string_view to, bool& replaced);
bool replace_first(std::ostream& out, std::string_view src, std::string_view from, std::string_view to);
bool replace_first_in_place(std::string& str, std::string_view from, std::string_view to);
std::string replace_all(std::string_view src, std::string_view from, std::string_view to);
void replace_all(std::ostream& out, std::string_view src, std::string_view from, std::string_view to);

/**
 * Indent: Utility class to easily output indentation to a stream
 *
 * Example usage:
 *
 *      const Indent indent(4);     // My indentation is 4 spaces
 *
 *      out << indent;              // Output 1 indentation
 *      out << indent(2);           // Output 2 indentations
 */
template <typename CharT>
class BasicIndent
{
public:
    class Multi;

    BasicIndent(std::size_t count, CharT ch = ' ') : m_str(count, ch) {}

    Multi operator()(std::size_t factor) const { return Multi(*this, factor); }

    bool empty() const noexcept { return m_str.empty(); }

    friend std::basic_ostream<CharT>& operator<<(std::basic_ostream<CharT>& out, const BasicIndent<CharT>& indent)
    {
        return out << indent.m_str;
    }

private:
    std::basic_string<CharT> m_str;
};

template <typename CharT>
class BasicIndent<CharT>::Multi
{
public:
    Multi(const BasicIndent& indent, std::size_t factor) : m_indent(indent), m_factor(factor) {}

    // NB: Read https://isocpp.org/wiki/faq/templates#template-friends regarding templated friend functions
    friend std::basic_ostream<CharT>& operator<<(std::basic_ostream<CharT>& out, const Multi& multi_indent)
    {
        for (auto c = 0u; c < multi_indent.m_factor; c++)
            out << multi_indent.m_indent;
        return out;
    }
private:
    const BasicIndent<CharT>& m_indent;
    std::size_t m_factor;
};

using Indent = BasicIndent<char>;

} // namespace string

} // namespace stdutils
