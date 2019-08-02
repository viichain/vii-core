#pragma once

#ifndef BASEN_HPP
#define BASEN_HPP

#include <algorithm>
#include <cctype>
#include <cassert>
#include <cstring>

namespace bn
{

template<class Iter1, class Iter2>
void encode_b16(Iter1 start, Iter1 end, Iter2 out);

template<class Iter1, class Iter2>
void encode_b32(Iter1 start, Iter1 end, Iter2 out);

template<class Iter1, class Iter2>
void encode_b64(Iter1 start, Iter1 end, Iter2 out);

template<class Iter1, class Iter2>
void decode_b16(Iter1 start, Iter1 end, Iter2 out);

template<class Iter1, class Iter2>
void decode_b32(Iter1 start, Iter1 end, Iter2 out);

template<class Iter1, class Iter2>
void decode_b64(Iter1 start, Iter1 end, Iter2 out);

namespace impl
{

const unsigned char Error = std::numeric_limits<unsigned char>::max();

namespace {

unsigned char extract_partial_bits(unsigned char value, size_t start_bit, size_t bits_count)
{
    assert(start_bit + bits_count < 8);
        unsigned char t1 = value >> (8 - bits_count - start_bit);
        unsigned char t2 = t1 & ~(0xff << bits_count);
    return t2;
}

unsigned char extract_overlapping_bits(unsigned char previous, unsigned char next, size_t start_bit, size_t bits_count)
{
    assert(start_bit + bits_count < 16);
    size_t bits_count_in_previous = 8 - start_bit;
    size_t bits_count_in_next = bits_count - bits_count_in_previous;
    unsigned char t1 = previous << bits_count_in_next;
    unsigned char t2 = next >> (8 - bits_count_in_next) & ~(0xff << bits_count_in_next) ;
    return (t1 | t2) & ~(0xff << bits_count);
}

}

struct b16_conversion_traits
{
    static size_t group_length()
    {
       return 4;
    }

    static unsigned char encode(unsigned int index)
    {
        static const char dictionary[17] = "0123456789ABCDEF";
        assert(index < 16);
        return dictionary[index];
    }

    static unsigned char decode(unsigned char c)
    {
        if (c >= '0' && c <= '9') {
            return c - '0';
        } else if (c >= 'A' && c <= 'F') {
            return c - 'A' + 10;
        }
        return Error;
    }
};

struct b32_conversion_traits
{
    static size_t group_length()
    {
       return 5;
    }

    static unsigned char encode(unsigned int index)
    {
        static const char dictionary[33] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
        assert(index < 32);
        return dictionary[index];
    }

    static unsigned char decode(unsigned char c)
    {
        if (c >= 'A' && c <= 'Z') {
            return c - 'A';
        } else if (c >= '2' && c <= '7') {
            return c - '2' + 26;
        }
        return Error;
    }
};

struct b64_conversion_traits
{
    static size_t group_length()
    {
       return 6;
    }

    static unsigned char encode(unsigned int index)
    {
        static const char dictionary[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        assert(index < 64);
        return dictionary[index];
    }

    static unsigned char decode(unsigned char c)
    {
        const int alph_len = 26;
        if (c >= 'A' && c <= 'Z') {
            return c - 'A';
        } else if (c >= 'a' && c <= 'z') {
            return c - 'a' + alph_len * 1;
        } else if (c >= '0' && c <= '9') {
            return c - '0' + alph_len * 2;
        } else if (c == '+') {
            return c - '+' + alph_len * 2 + 10;
        } else if (c == '/') {
            return c - '/' + alph_len * 2 + 11;
        }
        return Error;
    }
};

template<class ConversionTraits, class Iter1, class Iter2>
void decode(Iter1 start, Iter1 end, Iter2 out)
{
    Iter1 iter = start;
    size_t output_current_bit = 0;
    unsigned char buffer = 0;

    while (iter != end) {
        if (std::isspace(*iter)) {
            ++iter;
            continue;
        }
        unsigned char value = ConversionTraits::decode(*iter);
        if (value == Error) {
                        ++iter;
            continue;
        }
        size_t bits_in_current_byte = std::min<size_t>(output_current_bit + ConversionTraits::group_length(), 8) - output_current_bit;
        if (bits_in_current_byte == ConversionTraits::group_length()) {
                        buffer |= value << (8 - output_current_bit - ConversionTraits::group_length());
            output_current_bit += ConversionTraits::group_length();
                        if (output_current_bit == 8) {
                *out++ = buffer;
                buffer = 0;
                output_current_bit = 0;
            }
        } else {
                        size_t bits_in_next_byte = ConversionTraits::group_length() - bits_in_current_byte;
                        buffer |= value >> bits_in_next_byte;
            *out++ = buffer;
            buffer = 0;
                                    buffer |= value << (8 - bits_in_next_byte);
            output_current_bit = bits_in_next_byte;
        }
        ++iter;
    }
}

template<class ConversionTraits, class Iter1, class Iter2>
void encode(Iter1 start, Iter1 end, Iter2 out)
{
    Iter1 iter = start;
    size_t start_bit = 0;
    bool has_backlog = false;
    unsigned char backlog = 0;

    while (has_backlog || iter != end) {
        if (!has_backlog) {
            if (start_bit + ConversionTraits::group_length() < 8) {
                                                unsigned char v = extract_partial_bits(*iter, start_bit, ConversionTraits::group_length());
                *out++ = ConversionTraits::encode(v);
                                                start_bit += ConversionTraits::group_length();
            } else {
                                                backlog = *iter++;
                has_backlog = true;
            }
        } else {
                                    unsigned char v;
            if (iter == end)
                 v = extract_overlapping_bits(backlog, 0, start_bit, ConversionTraits::group_length());
            else
                 v = extract_overlapping_bits(backlog, *iter, start_bit, ConversionTraits::group_length());
            *out++ = ConversionTraits::encode(v);
            has_backlog = false;
            start_bit = (start_bit + ConversionTraits::group_length()) % 8;
        }
    }

    while (start_bit != 0)
    {
        *out++ = '=';
        start_bit = (start_bit + ConversionTraits::group_length()) % 8;
    }
}

} // impl

using namespace bn::impl;

template<class Iter1, class Iter2>
void encode_b16(Iter1 start, Iter1 end, Iter2 out)
{
    encode<b16_conversion_traits>(start, end, out);
}

template<class Iter1, class Iter2>
void encode_b32(Iter1 start, Iter1 end, Iter2 out)
{
    encode<b32_conversion_traits>(start, end, out);
}

template<class Iter1, class Iter2>
void encode_b64(Iter1 start, Iter1 end, Iter2 out)
{
    encode<b64_conversion_traits>(start, end, out);
}

template<class Iter1, class Iter2>
void decode_b16(Iter1 start, Iter1 end, Iter2 out)
{
    decode<b16_conversion_traits>(start, end, out);
}

template<class Iter1, class Iter2>
void decode_b32(Iter1 start, Iter1 end, Iter2 out)
{
    decode<b32_conversion_traits>(start, end, out);
}

template<class Iter1, class Iter2>
void decode_b64(Iter1 start, Iter1 end, Iter2 out)
{
    decode<b64_conversion_traits>(start, end, out);
}

} // bn

#endif // BASEN_HPP
