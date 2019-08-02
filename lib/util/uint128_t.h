#ifndef __UINT128_T__
#define __UINT128_T__

#include <iostream>
#include <stdexcept>
#include <stdint.h>
#include <utility>
#include <string>

class uint128_t{
    private:
        uint64_t UPPER, LOWER;

    public:
                uint128_t();
        uint128_t(const uint128_t & rhs);

        template <typename T> explicit uint128_t(const T & rhs){
            UPPER = 0;
            LOWER = rhs;
        }

        template <typename S, typename T> uint128_t(const S & upper_rhs, const T & lower_rhs){
            UPPER = upper_rhs;
            LOWER = lower_rhs;
        }

        
                uint128_t operator=(const uint128_t & rhs);

        template <typename T> uint128_t operator=(const T & rhs){
            UPPER = 0;
            LOWER = rhs;
            return *this;
        }

                operator bool() const;
        operator char() const;
        operator int() const;
        operator uint8_t() const;
        operator uint16_t() const;
        operator uint32_t() const;
        operator uint64_t() const;

                uint128_t operator&(const uint128_t & rhs) const;
        uint128_t operator|(const uint128_t & rhs) const;
        uint128_t operator^(const uint128_t & rhs) const;
        uint128_t operator&=(const uint128_t & rhs);
        uint128_t operator|=(const uint128_t & rhs);
        uint128_t operator^=(const uint128_t & rhs);
        uint128_t operator~() const;

        template <typename T> uint128_t operator&(const T & rhs) const{
            return uint128_t(0, LOWER & (uint64_t) rhs);
        }

        template <typename T> uint128_t operator|(const T & rhs) const{
            return uint128_t(UPPER, LOWER | (uint64_t) rhs);
        }

        template <typename T> uint128_t operator^(const T & rhs) const{
            return uint128_t(UPPER, LOWER ^ (uint64_t) rhs);
        }

        template <typename T> uint128_t operator&=(const T & rhs){
            UPPER = 0;
            LOWER &= rhs;
            return *this;
        }

        template <typename T> uint128_t operator|=(const T & rhs){
            LOWER |= (uint64_t) rhs;
            return *this;
        }

        template <typename T> uint128_t operator^=(const T & rhs){
            LOWER ^= (uint64_t) rhs;
            return *this;
        }

                uint128_t operator<<(const uint128_t & rhs) const;
        uint128_t operator>>(const uint128_t & rhs) const;
        uint128_t operator<<=(const uint128_t & rhs);
        uint128_t operator>>=(const uint128_t & rhs);

        template <typename T>uint128_t operator<<(const T & rhs) const{
            return *this << uint128_t(rhs);
        }

        template <typename T>uint128_t operator>>(const T & rhs) const{
            return *this >> uint128_t(rhs);
        }

        template <typename T>uint128_t operator<<=(const T & rhs){
            *this = *this << uint128_t(rhs);
            return *this;
        }

        template <typename T>uint128_t operator>>=(const T & rhs){
            *this = *this >> uint128_t(rhs);
            return *this;
        }

                bool operator!() const;
        bool operator&&(const uint128_t & rhs) const;
        bool operator||(const uint128_t & rhs) const;

        template <typename T> bool operator&&(const T & rhs){
            return ((bool) *this && rhs);
        }

        template <typename T> bool operator||(const T & rhs){
            return ((bool) *this || rhs);
        }

                bool operator==(const uint128_t & rhs) const;
        bool operator!=(const uint128_t & rhs) const;
        bool operator>(const uint128_t & rhs) const;
        bool operator<(const uint128_t & rhs) const;
        bool operator>=(const uint128_t & rhs) const;
        bool operator<=(const uint128_t & rhs) const;

        template <typename T> bool operator==(const T & rhs) const{
            return (!UPPER && (LOWER == (uint64_t) rhs));
        }

        template <typename T> bool operator!=(const T & rhs) const{
            return (UPPER | (LOWER != (uint64_t) rhs));
        }

        template <typename T> bool operator>(const T & rhs) const{
            return (UPPER || (LOWER > (uint64_t) rhs));
        }

        template <typename T> bool operator<(const T & rhs) const{
            return (!UPPER)?(LOWER < (uint64_t) rhs):false;
        }

        template <typename T> bool operator>=(const T & rhs) const{
            return ((*this > rhs) | (*this == rhs));
        }

        template <typename T> bool operator<=(const T & rhs) const{
            return ((*this < rhs) | (*this == rhs));
        }

                uint128_t operator+(const uint128_t & rhs) const;
        uint128_t operator+=(const uint128_t & rhs);
        uint128_t operator-(const uint128_t & rhs) const;
        uint128_t operator-=(const uint128_t & rhs);
        uint128_t operator*(const uint128_t & rhs) const;
        uint128_t operator*=(const uint128_t & rhs);

    private:
        std::pair <uint128_t, uint128_t> divmod(const uint128_t & lhs, const uint128_t & rhs) const;

    public:
		uint128_t operator/(const uint128_t & rhs) const;
        uint128_t operator/=(const uint128_t & rhs);
        uint128_t operator%(const uint128_t & rhs) const;
        uint128_t operator%=(const uint128_t & rhs);

        template <typename T> uint128_t operator+(const T & rhs) const{
            return uint128_t(UPPER + ((LOWER + (uint64_t) rhs) < LOWER), LOWER + (uint64_t) rhs);
        }

        template <typename T> uint128_t operator+=(const T & rhs){
            UPPER = UPPER + ((LOWER + rhs) < LOWER);
            LOWER = LOWER + rhs;
            return *this;
        }

        template <typename T> uint128_t operator-(const T & rhs) const{
            return uint128_t((uint64_t) (UPPER - ((LOWER - rhs) > LOWER)), (uint64_t) (LOWER - rhs));
        }

        template <typename T> uint128_t operator-=(const T & rhs){
            *this = *this - rhs;
            return *this;
        }

        template <typename T> uint128_t operator*(const T & rhs) const{
            return (*this) * (uint128_t(rhs));
        }

        template <typename T> uint128_t operator*=(const T & rhs){
            *this = *this * uint128_t(rhs);
            return *this;
        }

        template <typename T> uint128_t operator/(const T & rhs) const{
            return *this / uint128_t(rhs);
        }

        template <typename T> uint128_t operator/=(const T & rhs){
            *this = *this / uint128_t(rhs);
            return *this;
        }

        template <typename T> uint128_t operator%(const T & rhs) const{
            return *this - (rhs * (*this / rhs));
        }

        template <typename T> uint128_t operator%=(const T & rhs){
            *this = *this % uint128_t(rhs);
            return *this;
        }

                uint128_t operator++();
        uint128_t operator++(int);

                uint128_t operator--();
        uint128_t operator--(int);

                uint64_t upper() const;
        uint64_t lower() const;

                uint8_t bits() const;

                std::string str(uint8_t base = 10, const unsigned int & len = 0) const;
};

extern const uint128_t uint128_0;
extern const uint128_t uint128_1;
extern const uint128_t uint128_64;
extern const uint128_t uint128_128;


template <typename T> T operator&(const T & lhs, const uint128_t & rhs){
    return (T) (lhs & (T) rhs.lower());
}

template <typename T> T operator|(const T & lhs, const uint128_t & rhs){
    return (T) (lhs | (T) rhs.lower());
}

template <typename T> T operator^(const T & lhs, const uint128_t & rhs){
    return (T) (lhs ^ (T) rhs.lower());
}

template <typename T> T operator&=(T & lhs, const uint128_t & rhs){
    lhs &= (T) rhs.lower(); return lhs;
}

template <typename T> T operator|=(T & lhs, const uint128_t & rhs){
    lhs |= (T) rhs.lower(); return lhs;
}

template <typename T> T operator^=(T & lhs, const uint128_t & rhs){
    lhs ^= (T) rhs.lower(); return lhs;
}

template <typename T> bool operator==(const T & lhs, const uint128_t & rhs){
    return (!rhs.upper() && ((uint64_t) lhs == rhs.lower()));
}

template <typename T> bool operator!=(const T & lhs, const uint128_t & rhs){
    return (rhs.upper() | ((uint64_t) lhs != rhs.lower()));
}

template <typename T> bool operator>(const T & lhs, const uint128_t & rhs){
    return (!rhs.upper()) && ((uint64_t) lhs > rhs.lower());
}

template <typename T> bool operator<(const T & lhs, const uint128_t & rhs){
    if (rhs.upper()){
        return true;
    }
    return ((uint64_t) lhs < rhs.lower());
}

template <typename T> bool operator>=(const T & lhs, const uint128_t & rhs){
    if (rhs.upper()){
            return false;
    }
    return ((uint64_t) lhs >= rhs.lower());
}

template <typename T> bool operator<=(const T & lhs, const uint128_t & rhs){
    if (rhs.upper()){
            return true;
    }
    return ((uint64_t) lhs <= rhs.lower());
}

template <typename T> T operator+(const T & lhs, const uint128_t & rhs){
    return (T) (rhs + lhs);
}

template <typename T> T & operator+=(T & lhs, const uint128_t & rhs){
    lhs = (T) (rhs + lhs);
    return lhs;
}

template <typename T> T operator-(const T & lhs, const uint128_t & rhs){
    return (T) (uint128_t(lhs) - rhs);
}

template <typename T> T & operator-=(T & lhs, const uint128_t & rhs){
    lhs = (T) (uint128_t(lhs) - rhs);
    return lhs;
}

template <typename T> T operator*(const T & lhs, const uint128_t & rhs){
    return lhs * (T) rhs.lower();
}

template <typename T> T & operator*=(T & lhs, const uint128_t & rhs){
    lhs *= (T) rhs.lower();
    return lhs;
}

template <typename T> T operator/(const T & lhs, const uint128_t & rhs){
    return (T) (uint128_t(lhs) / rhs);
}

template <typename T> T & operator/=(T & lhs, const uint128_t & rhs){
    lhs = (T) (uint128_t(lhs) / rhs);
    return lhs;
}

template <typename T> T operator%(const T & lhs, const uint128_t & rhs){
    return (T) (uint128_t(lhs) % rhs);
}

template <typename T> T & operator%=(T & lhs, const uint128_t & rhs){
    lhs = (T) (uint128_t(lhs) % rhs);
    return lhs;
}

std::ostream & operator<<(std::ostream & stream, const uint128_t & rhs);
#endif
