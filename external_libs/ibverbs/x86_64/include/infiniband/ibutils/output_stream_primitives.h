/*
 * Copyright (c) 2020-2020 Mellanox Technologies LTD. All rights reserved.
 *
 * This software is available to you under the terms of the
 * OpenIB.org BSD license included below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */


#ifndef OUTPUT_STREAM_PRIMITIVES_H_
#define OUTPUT_STREAM_PRIMITIVES_H_

#include <ios>
#include <ostream>
#include <iomanip>

class ostream_flags_saver
{
    private:
        std::ostream &            m_stream;
        std::ios_base::fmtflags   m_flags;

    public:
        inline ostream_flags_saver(std::ostream & stream)
            : m_stream(stream), m_flags(m_stream.flags())
        {
        }

        inline ~ostream_flags_saver() {
            m_stream.flags(m_flags);
        }
};

//
// Class wrappers sections
//
class CSTR
{
    public:
        const char * m_ptr;

        CSTR(const char * ptr)
           : m_ptr(ptr)
        {
        }
};


template<typename _Type>
    class HEX_T {
        public:
            const _Type     m_value;
            const int       m_width;
            const char      m_fill;

        public:
            HEX_T(_Type value, int width, char fill)
                : m_value(value), m_width(width), m_fill(fill)
            {
            }
    };

template<typename _Type>
    class PTR_T : public HEX_T<_Type>{

        public:
            PTR_T(_Type value, int width, char fill)
                : HEX_T<_Type>(value, width, fill)
            {
            }
    };

template<typename _Type>
    class DEC_T {
        public:
            const _Type     m_value;
            const int       m_width;
            const char      m_fill;

            DEC_T(_Type value, int width, char fill)
                : m_value(value), m_width(width), m_fill(fill)
            {
            }
    };


template<typename _Type>
    class FLOAT_T {
        public:
            const _Type     m_value;
            const int       m_precision;
            const bool      m_scientific;

            FLOAT_T(_Type value, int prec, bool isScientific)
                : m_value(value), m_precision(prec), m_scientific(isScientific)
            {
            }
    };



template<typename _Type>
    class EDGED_T {
        public:
            const _Type & m_value;
            const char *  m_left;
            const char *  m_right;

            EDGED_T(const _Type & value, const char * left, const char * right)
                : m_value(value), m_left(left), m_right(right)
            {
            }
    };

template<typename _Type>
    class QUOTED_T {
        public:
            const _Type & m_value;
            const char    m_left;
            const char    m_right;

            QUOTED_T(const _Type & value, char left, char right)
                : m_value(value), m_left(left), m_right(right)
            {
            }
    };

template<typename _TypeLeft, typename _TypeRight>
    class IF_T {
        public:
            const bool       m_condition;
            const _TypeLeft  m_left;
            const _TypeRight m_right;

            IF_T(bool condition, const _TypeLeft & left, const _TypeRight & right)
                : m_condition(condition), m_left(left), m_right(right)
            {
            }
    };

//
// Functions section
//
template<typename _Type>
    inline PTR_T<_Type> PTR(_Type value, int width = (int)(sizeof(_Type) * 2), char fill = '0') {
        return PTR_T<_Type>(value, width, fill);
    }

template<typename _Type>
    inline HEX_T<_Type> HEX(_Type value, int width = (int)(sizeof(_Type) * 2), char fill = '0') {
        return HEX_T<_Type>(value, width, fill);
    }

template<typename _Type>
    inline DEC_T<_Type> DEC(_Type value, int width = 0, char fill = ' ') {
        return DEC_T<_Type>(value, width, fill);
    }

template<typename _Type>
    inline FLOAT_T<_Type> FLOAT(_Type value, int precision, bool isScientific = false) {
        return FLOAT_T<_Type>(value, precision, isScientific);
    }


template<typename _Type>
    inline EDGED_T<const _Type> EDGED(const _Type & value, const char * edge) {
        return EDGED_T<const _Type>(value, edge, edge);
    }

template<typename _Type>
    inline EDGED_T<const _Type> EDGED(const char * left, const _Type & value, const char * right) {
        return EDGED_T<const _Type>(value, left, right);
    }

template<typename _Type>
    inline QUOTED_T<_Type> QUOTED(const _Type & value, char quote = '\"') {
        return QUOTED_T<_Type>(value, quote, quote);
    }

template<typename _Type>
    inline QUOTED_T<_Type> QUOTED(char left, const _Type & value, char right) {
        return QUOTED_T<_Type>(value, left, right);
    }

template<typename _Type>
    inline QUOTED_T<_Type> BRACES(const _Type & value) {
        return QUOTED_T<_Type>(value, '{', '}');
    }

template<typename _Type>
    inline QUOTED_T<_Type> S_BRACKETS(const _Type & value) {
        return QUOTED_T<_Type>(value, '[', ']');
    }

template<typename _Type>
    inline QUOTED_T<_Type> R_BRACKETS(const _Type & value) {
        return QUOTED_T<_Type>(value, '(', ')');
    }

template<typename _TypeLeft, typename _TypeRight>
    inline IF_T<_TypeLeft, _TypeRight> IF(bool condition, const _TypeLeft & left, const _TypeRight & right) {
        return IF_T<_TypeLeft, _TypeRight>(condition, left, right);
    }

//
// Operators section
//
inline std::ostream & operator << (std::ostream & stream, const CSTR & v) {
    return stream << v.m_ptr;
}

template<typename _Type>
    inline std::ostream & operator << (std::ostream & stream, const HEX_T<_Type> & v)
    {
        ostream_flags_saver saver(stream);
        stream << std::hex << std::setfill(v.m_fill);

        if(v.m_width)
            stream << std::setw(v.m_width);

        return stream << +v.m_value;
    }

template<typename _Type>
    inline std::ostream & operator << (std::ostream & stream, const PTR_T<_Type> & v) {
        return stream << "0x" << (const HEX_T<_Type> &) v;
    }

template<typename _Type>
    inline std::ostream & operator << (std::ostream & stream, const DEC_T<_Type> & v)
    {
        ostream_flags_saver saver(stream);
        stream << std::dec << std::setfill(v.m_fill);

        if(v.m_width)
            stream << std::setw(v.m_width);

        return stream << +v.m_value;
    }

template<typename _Type>
    inline std::ostream & operator << (std::ostream & stream, const FLOAT_T<_Type> & v)
    {
        ostream_flags_saver saver(stream);

        if(v.m_precision)
            stream << std::fixed << std::setprecision(v.m_precision);

        if (v.m_scientific)
            stream << std::scientific;

        return stream << v.m_value;
    }

template<typename _Type>
    inline std::ostream & operator << (std::ostream & stream, const EDGED_T<_Type> & v) {
        return stream << v.m_left << v.m_value << v.m_right;
    }

template<typename _Type>
    inline std::ostream & operator << (std::ostream & stream, const QUOTED_T<_Type> & v) {
        return stream << v.m_left << v.m_value << v.m_right;
    }

template<typename _TypeLeft, typename _TypeRight>
    inline std::ostream & operator << (std::ostream & stream, const IF_T<_TypeLeft, _TypeRight> & v)
    {
        if (v.m_condition)
            return stream << v.m_left;

        return stream << v.m_right;
    }

#endif /* OUTPUT_STREAM_PRIMITIVES_H_ */
