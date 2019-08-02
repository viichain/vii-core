#pragma once


#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cbitset_portability.h"

struct bitset_s
{
    uint64_t* array;
    size_t arraysize;
    size_t capacity;
};

typedef struct bitset_s bitset_t;

bitset_t* bitset_create(void);

bitset_t* bitset_create_with_capacity(size_t size);

void bitset_free(bitset_t* bitset);

void bitset_clear(bitset_t* bitset);

bitset_t* bitset_copy(const bitset_t* bitset);

bool bitset_resize(bitset_t* bitset, size_t newarraysize, bool padwithzeroes);

static inline size_t
bitset_size_in_bytes(const bitset_t* bitset)
{
    return bitset->arraysize * sizeof(uint64_t);
}

static inline size_t
bitset_size_in_bits(const bitset_t* bitset)
{
    return bitset->arraysize * 64;
}

static inline size_t
bitset_size_in_words(const bitset_t* bitset)
{
    return bitset->arraysize;
}

static inline bool
bitset_grow(bitset_t* bitset, size_t newarraysize)
{
    if (bitset->capacity < newarraysize)
    {
        uint64_t* newarray;
        bitset->capacity = newarraysize * 2;
        if ((newarray = (uint64_t*)realloc(
                 bitset->array, sizeof(uint64_t) * bitset->capacity)) == NULL)
        {
            free(bitset->array);
            return false;
        }
        bitset->array = newarray;
    }
    memset(bitset->array + bitset->arraysize, 0,
           sizeof(uint64_t) * (newarraysize - bitset->arraysize));
    bitset->arraysize = newarraysize;
    return true; // success!
}

bool bitset_trim(bitset_t* bitset);

void bitset_shift_left(bitset_t* bitset, size_t s);

void bitset_shift_right(bitset_t* bitset, size_t s);

static inline void
bitset_set(bitset_t* bitset, size_t i)
{
    size_t shiftedi = i >> 6;
    if (shiftedi >= bitset->arraysize)
    {
        if (!bitset_grow(bitset, shiftedi + 1))
        {
            abort();
        }
    }
    bitset->array[shiftedi] |= ((uint64_t)1) << (i % 64);
}

static inline void
bitset_unset(bitset_t* bitset, size_t i)
{
    size_t shiftedi = i >> 6;
    if (shiftedi >= bitset->arraysize)
    {
        return;
    }
    bitset->array[shiftedi] &= ~(((uint64_t)1) << (i % 64));
}

static inline bool
bitset_get(const bitset_t* bitset, size_t i)
{
    size_t shiftedi = i >> 6;
    if (shiftedi >= bitset->arraysize)
    {
        return false;
    }
    return (bitset->array[shiftedi] & (((uint64_t)1) << (i % 64))) != 0;
}

size_t bitset_count(const bitset_t* bitset);

size_t bitset_minimum(const bitset_t* bitset);

size_t bitset_maximum(const bitset_t* bitset);

bool bitset_inplace_union(bitset_t* b1, const bitset_t* b2);

size_t bitset_union_count(const bitset_t* b1, const bitset_t* b2);

void bitset_inplace_intersection(bitset_t* b1, const bitset_t* b2);

static inline size_t
bitset_intersection_count(const bitset_t* b1, const bitset_t* b2)
{
    size_t answer = 0;
    size_t minlength = b1->arraysize < b2->arraysize ? b1->arraysize : b2->arraysize;
    for (size_t k = 0; k < minlength; ++k)
    {
        answer += __builtin_popcountll(b1->array[k] & b2->array[k]);
    }
    return answer;
}

void bitset_inplace_difference(bitset_t* b1, const bitset_t* b2);

size_t bitset_difference_count(const bitset_t* b1, const bitset_t* b2);

bool bitset_inplace_symmetric_difference(bitset_t* b1, const bitset_t* b2);

size_t bitset_symmetric_difference_count(const bitset_t* b1,
                                         const bitset_t* b2);

bool bitset_equal(const bitset_t* b1, const bitset_t* b2);

bool bitset_subseteq(const bitset_t* b1, const bitset_t* b2);

static inline bool
nextSetBit(const bitset_t* bitset, size_t* i)
{
    size_t x = *i >> 6;
    if (x >= bitset->arraysize)
    {
        return false;
    }
    uint64_t w = bitset->array[x];
    w >>= (*i & 63);
    if (w != 0)
    {
        *i += __builtin_ctzll(w);
        return true;
    }
    x++;
    while (x < bitset->arraysize)
    {
        w = bitset->array[x];
        if (w != 0)
        {
            *i = x * 64 + __builtin_ctzll(w);
            return true;
        }
        x++;
    }
    return false;
}

static inline size_t
nextSetBits(const bitset_t* bitset, size_t* buffer, size_t capacity,
            size_t* startfrom)
{
    if (capacity == 0)
        return 0; // sanity check
    size_t x = *startfrom >> 6;
    if (x >= bitset->arraysize)
    {
        return 0; // nothing more to iterate over
    }
    uint64_t w = bitset->array[x];
    w >>= (*startfrom & 63);
    size_t howmany = 0;
    size_t base = x << 6;
    while (howmany < capacity)
    {
        while (w != 0)
        {
            uint64_t t = w & (~w + 1);
            int r = __builtin_ctzll(w);
            buffer[howmany++] = r + base;
            if (howmany == capacity)
                goto end;
            w ^= t;
        }
        x += 1;
        if (x == bitset->arraysize)
        {
            break;
        }
        base += 64;
        w = bitset->array[x];
    }
end:
    if (howmany > 0)
    {
        *startfrom = buffer[howmany - 1];
    }
    return howmany;
}

typedef bool (*bitset_iterator)(size_t value, void* param);

static inline bool
bitset_for_each(const bitset_t* b, bitset_iterator iterator, void* ptr)
{
    size_t base = 0;
    for (size_t i = 0; i < b->arraysize; ++i)
    {
        uint64_t w = b->array[i];
        while (w != 0)
        {
            uint64_t t = w & (~w + 1);
            int r = __builtin_ctzll(w);
            if (!iterator(r + base, ptr))
                return false;
            w ^= t;
        }
        base += 64;
    }
    return true;
}

static inline void
bitset_print(const bitset_t* b)
{
    printf("{");
    for (size_t i = 0; nextSetBit(b, &i); i++)
    {
        printf("%zu, ", i);
    }
    printf("}");
}
