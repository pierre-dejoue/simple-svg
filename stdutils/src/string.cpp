// Copyright (c) 2021 Pierre DEJOUE
// This code is distributed under the terms of the MIT License

#include <stdutils/macros.h>
#include <stdutils/string.h>

#include <algorithm>
#include <cassert>
#include <sstream>

namespace stdutils {

namespace ascii {

bool isalpha(char c) noexcept { return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z'); }

bool isalnum(char c) noexcept { return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9'); }

bool isprint(char c) noexcept { return char{32} <= c && c < char{127}; }                    // All printable characters (Note the absence of '\t' = \x9)

bool isspace(char c) noexcept { return (char(9) <= c && c <= char{13}) || c == ' '; }       // Namely '\t', '\n', '\v', '\f', '\r' and the regular space

bool isupper(char c) noexcept { return ('A' <= c && c <= 'Z'); }

bool islower(char c) noexcept { return ('a' <= c && c <= 'z'); }

char tolower(char c) noexcept { return ('A' <= c && c <= 'Z') ? c + char{0x20} : c; }

char toupper(char c) noexcept { return ('a' <= c && c <= 'z') ? c - char{0x20} : c; }

std::ostream& operator<<(std::ostream& out, const HexEscape& hex_escape)
{
    char a = '0' + ((hex_escape.c & 0xF0) >> 4);
    char b = '0' + (hex_escape.c & 0x0F);
    if (a > 0x39) { a += 7; }               // For ABCDEF
    if (b > 0x39) { b += 7; }
    return out << "\\x" << a << b;
}

} // namespace ascii

std::size_t strnlen(const char* str, std::size_t max_len)
{
    if (str == nullptr)
        return 0;
    std::size_t len = 0;
    while(*str++ && len < max_len) { len++; }
    return len;
}

namespace string {

std::string tolower(std::string_view in)
{
    std::string out(in);
    std::transform(in.cbegin(), in.cend(), out.begin(), [](char c) -> char { return ('A' <= c && c <= 'Z') ? c + char{0x20} : c; });
    return out;
}

std::string toupper(std::string_view in)
{
    std::string out(in);
    std::transform(in.cbegin(), in.cend(), out.begin(), [](char c) -> char { return ('a' <= c && c <= 'z') ? c - char{0x20} : c; });
    return out;
}

std::string capitalize(std::string_view in)
{
    std::string out = tolower(in);
    if (!out.empty()) { out.front() = ::stdutils::ascii::toupper(out.front()); }
    return out;
}

std::string remove_all_spaces(std::string_view in)
{
    std::string out;
    std::copy_if(std::cbegin(in), std::cend(in), std::back_inserter(out), [](const char c) { return !ascii::isspace(c); });
    return out;
}

bool is_null_terminated(const char* str, std::size_t max_len)
{
    return str != nullptr && strnlen(str, max_len) != max_len;
}

namespace {

struct PredicatePureASCII
{
    static bool eval(const char& c) { return c > 0; }
};

struct PredicateStrictlyPrintASCII
{
    static bool eval(const char& c) { return char{32} <= c && c < char{127}; }
};

struct PredicatePrintableASCII
{
    static bool eval(const char& c) { return (char(9) <= c && c <= char{13}) || (char{32} <= c && c < char{127}); }
};

template <typename PREDICATE>
bool is_cond_ascii(const char* str, std::size_t max_len)
{
    if (str == nullptr)
        return false;
    std::size_t len = 0;
    while(*str && len < max_len)
    {
        const char& c = *str++;
        if (!PREDICATE::eval(c)) { return false; }
        len++;
    }
    return len < max_len;
}

} // namespace

bool is_pure_ascii(const char* str, std::size_t max_len)
{
    return is_cond_ascii<PredicatePureASCII>(str, max_len);
}

bool is_pure_ascii(const std::string& str)
{
    return std::all_of(str.cbegin(), str.cend(), PredicatePureASCII::eval);
}

bool is_strictly_print_ascii(const char* str, std::size_t max_len)
{
     return is_cond_ascii<PredicateStrictlyPrintASCII>(str, max_len);
}

bool is_strictly_print_ascii(const std::string& str)
{
    return std::all_of(str.cbegin(), str.cend(), PredicateStrictlyPrintASCII::eval);
}

bool is_printable_ascii(const char* str, std::size_t max_len)
{
    return is_cond_ascii<PredicatePrintableASCII>(str, max_len);
}

bool is_printable_ascii(const std::string& str)
{
    return std::all_of(str.cbegin(), str.cend(), PredicatePrintableASCII::eval);
}

bool is_valid_id(Id id)
{
    constexpr unsigned int MAX_LEN = 65535;
    if (id == nullptr)
        return false;
    unsigned int len = 0;
    const char* c_ptr = id;
    if (!stdutils::ascii::isalpha(*c_ptr))      // If empty string or not starting with an alphabetical character
        return false;
    while (*c_ptr && len < MAX_LEN)
    {
        const char& c = *c_ptr++;
        if (!('a' <= c && c <= 'z') &&
            !('A' <= c && c <= 'Z') &&
            !('0' <= c && c <= '9') &&
            c != '#' &&
            c != '.' &&
            c != '-' &&
            c != '_')
        {
            return false;
        }
        len++;
    }
    return len < MAX_LEN;
}

bool is_well_formed_utf8(u8_c_str u8_str, std::size_t* error_loc, std::size_t max_len)
{
    if (error_loc) { *error_loc = 0; }
    if (u8_str == nullptr)
        return false;
    bool well_formed = true;
    unsigned int continuation_chars = 0;
    std::size_t len = 0;
    while (well_formed && *u8_str && len < max_len)
    {
        const stdutils::u8_char_t c = *u8_str++;
        if ((c & 0xC0) == 0x80)
        {
            // 10xx xxxx
            // Continuation_character
            well_formed = (continuation_chars > 0);
            continuation_chars--;
        }
        else
        {
            // New character
            well_formed = (continuation_chars == 0);
            if ((c & 0x80) == 0)
            {
                // 0xxx xxxx
                // Do nothing
            }
            else if ((c & 0xE0) == 0xC0)
            {
                // 110x xxxx
                continuation_chars = 1;
            }
            else if ((c & 0xF0) == 0xE0)
            {
                // 1110 xxxx
                continuation_chars = 2;
            }
            else if ((c & 0xF8) == 0xF0)
            {
                // 1111 0xxx
                continuation_chars = 3;
            }
            else
            {
                // Unexpected pattern in a UTF8 string
                well_formed = false;
            }
        }
        if (!well_formed && error_loc) { *error_loc = len; }
        len++;
    }
    if (!well_formed)
        return false;
    well_formed &= (continuation_chars == 0);
    well_formed &= (len < max_len);
    if (!well_formed && error_loc) { *error_loc = len; }
    return well_formed;
}

bool is_well_formed_utf8(const u8_string& u8_str, std::size_t* error_loc)
{
    return is_well_formed_utf8(u8_str.data(), error_loc, u8_str.size() + 1);
}

std::size_t count_utf8_chars(u8_c_str u8_str, std::size_t max_len)
{
    if (u8_str == nullptr)
        return 0;
    std::size_t len = 0;
    std::size_t count = 0;
    while (*u8_str && len < max_len)
    {
        const stdutils::u8_char_t c = *u8_str++;
        len++;
        // Increment the counter unless this is a continuation byte (in binary: 10xx xxxx)
        if ((c & 0xC0) != 0x80) { count++; }
    }
    return count;
}

std::size_t count_utf8_chars(const u8_string& u8_str)
{
    return static_cast<std::size_t>(std::count_if(u8_str.cbegin(), u8_str.cend(), [](const auto& c) { return (c & 0xC0) != 0x80; }));
}

std::pair<std::string_view, std::string_view> l_split(std::string_view in_str, char delim)
{
    const auto delim_pos = in_str.find(delim);
    assert(delim_pos == std::string_view::npos || delim_pos + 1 <= in_str.size());
    return delim_pos == std::string_view::npos
        ? std::make_pair(in_str, std::string_view())
        : std::make_pair(std::string_view(in_str.data(), delim_pos), std::string_view(in_str.data() + delim_pos + 1u, in_str.size() - delim_pos - 1));
}

std::pair<std::string_view, std::string_view> r_split(std::string_view in_str, char delim)
{
    const auto delim_pos = in_str.rfind(delim);
    assert(delim_pos == std::string_view::npos || delim_pos + 1 <= in_str.size());
    return delim_pos == std::string_view::npos
        ? std::make_pair(std::string_view(), in_str)
        : std::make_pair(std::string_view(in_str.data(), delim_pos), std::string_view(in_str.data() + delim_pos + 1u, in_str.size() - delim_pos - 1));
}

namespace {

template <typename UnaryPred>
std::vector<std::string_view> split_and_filter(std::string_view in_str, char delim, UnaryPred pred)
{
    std::vector<std::string_view> parts;
    std::string_view head, tail;
    auto& remainder = in_str;
    bool cont = true;
    while (cont)
    {
        std::tie(head, tail) = l_split(remainder, delim);
        //assert(tail.size() < remainder.size());
        cont = (head.size() + tail.size() < remainder.size());
        remainder = tail;
        if (pred(head)) { parts.emplace_back(head); }
    }
    return parts;
}

} // namespace

std::vector<std::string_view> split(std::string_view in_str, char delim)
{
    return split_and_filter(in_str, delim, [](const std::string_view&) -> bool { return true; });
}

std::vector<std::string_view> split_skip_empty(std::string_view in_str, char delim)
{
    return split_and_filter(in_str, delim, [](const std::string_view& elt) -> bool { return !elt.empty(); });
}

std::string replace_first(std::string_view src, std::string_view from, std::string_view to, bool& replaced)
{
    std::stringstream out;
    replaced = replace_first(out, src, from, to);
    return out.str();
}

bool replace_first(std::ostream& out, std::string_view src, std::string_view from, std::string_view to)
{
    const auto lookup_pos = (!from.empty()) ? src.find(from) : std::string::npos;
    out << src.substr(0, lookup_pos);
    if (lookup_pos == std::string::npos)
        return false;
    out << to;
    out << src.substr(lookup_pos + from.size());
    return true;
}

bool replace_first_in_place(std::string& str, std::string_view from, std::string_view to)
{
    const auto lookup_pos = (!from.empty()) ? str.find(from) : std::string::npos;
    if (lookup_pos == std::string::npos)
        return false;
    IGNORE_RETURN str.replace(lookup_pos, from.size(), to);
    return true;
}

std::string replace_all(std::string_view src, std::string_view from, std::string_view to)
{
    std::stringstream out;
    replace_all(out, src, from, to);
    return out.str();
}

void replace_all(std::ostream& out, std::string_view src, std::string_view from, std::string_view to)
{
    if (from.empty())
    {
        out << src;
        return;
    }
    std::size_t lookup_pos = 0;
    while (true)
    {
        const auto next_lookup_pos = src.find(from, lookup_pos);
        assert(next_lookup_pos >= lookup_pos);
        out << src.substr(lookup_pos, next_lookup_pos - lookup_pos);
        if (next_lookup_pos == std::string::npos)
            break;
        out << to;
        assert(next_lookup_pos + from.size() > lookup_pos);         // The 'from' string is not empty therefore lookup_pos is strictly increasing each loop
        lookup_pos = next_lookup_pos + from.size();
    }
}

} // namespace string

} // namespace stdutils
