/*
 * Copyright (c) 2017-2021 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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


#ifndef CSV_PARSER_H_
#define CSV_PARSER_H_

#include <string>
#include <vector>
#include <map>
#include <fstream>      // std::ifstream
#include <sstream>      // std::isstream
#include <errno.h>

#include "ibis_types.h"
#include <infiniband/ibis/memory_pool.h>

#define LINE_BUFF_SIZE              (8*1024)
#define NUM_FIELDS_IN_INDEX_SECTION 5

#define START_INDEX_TABLE   "START_INDEX_TABLE"
#define END_INDEX_TABLE     "END_INDEX_TABLE"

enum CSV_PARSING_CHARS {
        COMMA,
        APOSTROPHES
};

static const char CSV_PARSING_CHARS_STRING[] = {
        ',', '"'
};
#define CSV_PARSING_CHAR_COMMA =   ','
#define CSV_PARSING_CHAR_APOSTROPHES = '"'


typedef pair < string , struct offset_info > pair_string_offset_info_t;

typedef vector < u_int8_t > vec_uint8_t;
typedef vector < const char * > vec_str_t;
typedef map < string, struct offset_info > map_str_to_offset_t;

struct offset_info {
    u_int64_t offset;
    u_int64_t length;
    u_int32_t start_line;
    u_int32_t num_lines;
};

class Exception : public std::exception
{
    public:
        //enum for each exception type, which can also be used to determin
        //exception class, useful for logging or other localisation methods
        //for generating a message of some sort.
        enum ExceptionType
        {
            //shouldnt ever be thrown
            UNKNOWN_EXCEPTION = 0,
            //same as above but has a string that may provide some info
            UNKNOWN_EXCEPTION_STR,
            //eg file not found
            FILE_OPEN_ERROR,
            //lexical cast type error
            TYPE_PARSE_ERROR,
            //NOTE: in many cases functions only check and throw this in debug
            INVALID_ARG,
            //an error occured while trying to parse data from a file
            FILE_PARSE_ERROR,
        };

        virtual ExceptionType GetExceptionType()const = 0;
        virtual const char* what() const throw(){ return "TYPE_GENERAL_ERROR"; }
};

class FileOpenError : public Exception
{
    public:
        enum Reason
        {
            FILE_NOT_FOUND,
            LOCKED,
            DOES_NOT_EXIST,
            ACCESS_DENIED
        };
        FileOpenError(Reason reason, const char *file, const char *dir);
        virtual ExceptionType GetExceptionType()const
        {
            return FILE_OPEN_ERROR;
        }

        virtual const char* what() const throw(){ return "FILE_OPEN_ERROR"; }

        Reason GetReason()const;
        const char* GetFile()const;
        const char* GetDir ()const;
    private:
        Reason reason;
        static const unsigned FILE_LEN = 256;
        static const unsigned DIR_LEN  = 256;
        char file[FILE_LEN], dir[DIR_LEN];
};

class TypeParseError : public Exception
{
    public:
        enum Reason
        {
            EMPTY_VALUE,
            TOO_LONG_VALUE,
            INVALID_VALUE,
            UNKNOWN_VALUE,
            UNKNOWN_TYPE,
            ACCESS_DENIED
        };

        TypeParseError(Reason reason, const char *parse_value):
            m_reason(reason), m_parse_value(parse_value){};
        ~TypeParseError()throw(){};
        virtual ExceptionType GetExceptionType()const
        {
            return TYPE_PARSE_ERROR;
        }

        virtual const char* what() const throw(){ return "TYPE_PARSE_ERROR"; }

        Reason GetReason()const {return m_reason;}
        const string& GetParseValue() const {return m_parse_value;}
    private:
        Reason m_reason;
        string m_parse_value;
};

class CsvParser;

class CsvFileStream {
    friend class CsvParser;

private:
    ifstream              m_cfs;
    string                m_file_name;
    map_str_to_offset_t   m_section_name_to_offset;

    ifstream &GetFileStream() { return m_cfs; }
    int UpdateSectionOffsetTable(CsvParser &csv_parser);

public:
    CsvFileStream(string file_name, CsvParser &csv_parser);
    ~CsvFileStream();

    bool IsFileOpen();
};

//Encapsulate the knowledge of parsing a specific Record field and update the
//SectionRecord
template <class SectionRecord>
class ParseFieldInfo {
public:
    typedef bool (SectionRecord::*setter_func_t) (const char *);
    typedef bool (*static_setter_func_t) (SectionRecord &obj, const char *);
private:
    string m_field_name;
    setter_func_t m_p_setter_func;
    static_setter_func_t m_p_static_setter_func;
    bool m_mandatory;
    string m_default_value;

public:
    ParseFieldInfo(const char *field_name,
                   setter_func_t p_setter_func);

    ParseFieldInfo(const char *field_name,
                   setter_func_t p_setter_func,
                   const string &default_value);

    ParseFieldInfo(const char *field_name,
                   static_setter_func_t p_static_setter_func);

    ParseFieldInfo(const char *field_name,
                   static_setter_func_t p_static_setter_func,
                   const string &default_value);

    inline const string & GetFieldName() const {
        return m_field_name;
    }

    inline bool IsMandatory() {
        return m_mandatory;
    }

    const string &GetDefaultValue() const {
        return m_default_value;
    }

    bool Handle(SectionRecord & obj, const char *value = NULL) const {
        value = value ? value : m_default_value.c_str();
        return m_p_setter_func ?
                (obj.*m_p_setter_func)(value):
                (*m_p_static_setter_func)(obj, value);
    }
};


template <class SectionRecord>
class SectionParser {

    vector < class ParseFieldInfo < SectionRecord > > m_parse_section_info;
    vector < SectionRecord > m_section_data;
    string m_section_name;

public:
    SectionParser();
    ~SectionParser();

    inline const vector < SectionRecord > &GetSectionData() const {
        return m_section_data;
    }

    void InsertSectionData(const SectionRecord &section_record) {
        m_section_data.push_back(section_record);
    }

    int Init(const string &section_name) {
        m_section_name = section_name;
        return SectionRecord::Init(m_parse_section_info);
    }

    vector < class ParseFieldInfo < SectionRecord > > &
        GetParseSectionInfo() {
        return m_parse_section_info;
    }

    void ClearData();

    inline const string &GetSectionName() {return m_section_name;}

};

typedef void(*ibis_log_msg_function_t)(const char *file_name, unsigned line_num,
                                       const char *function_name, int level,
                                       const char *format, ...);

class CsvParser
{
    friend class CsvFileStream;

private:
    static bool m_old_csv_file_supported;
    static ibis_log_msg_function_t m_log_msg_function;

    int GetNextLineAndSplitIntoTokens(istream& str,
                                      char *line_buff,
                                      vec_str_t &line_tokens);
public:
    CsvParser() {}
    ~CsvParser() {

    }

    // Old CSV file support functions
    static bool IsOldCsvFileSupported() { return m_old_csv_file_supported; }
    static void SetOldCsvFileSupported(bool supported) { m_old_csv_file_supported = supported; }

    template <class SectionRecord>
    int ParseSection(CsvFileStream &cfs, SectionParser <SectionRecord> &section_parser);

    template <class SectionRecord, class Handler>
    int ParseAndHandleSection(CsvFileStream &cfs,
                              SectionParser <SectionRecord> &section_parser,
                              Handler *p_handler,
                              int (Handler::*handler_func)(const SectionRecord &));

    bool IsSectionExist(CsvFileStream &cfs, const std::string &section_name) const;

    // parameter "reserved" added for comparability with ParseWithNA template
    static bool Parse(const char *field_str, string &field, u_int8_t reserved = 0);

    static bool Parse(const char *field_str, u_int64_t &field, u_int8_t base = 10);
    static bool Parse(const char *field_str, u_int32_t &field, u_int8_t base = 10);
    static bool Parse(const char *field_str, u_int16_t &field, u_int8_t base = 10);
    static bool Parse(const char *field_str, u_int8_t &field, u_int8_t base = 10);

    static bool Parse(const char *field_str, int64_t &field, u_int8_t base = 10);
    static bool Parse(const char *field_str, int32_t &field, u_int8_t base = 10);
    static bool Parse(const char *field_str, int16_t &field, u_int8_t base = 10);
    static bool Parse(const char *field_str, int8_t &field, u_int8_t base = 10);

    static bool isNA(const char *field_str);

    template<typename T>
        static bool ParseWithNA(const char *field_str, T & field, const T & defValue = T(), u_int8_t base = 10) {
            if (isNA(field_str)) {
                field = defValue;
                return true;
            }

            return Parse(field_str, field, base);
        }

    static bool ValidateStringInput(const char *field_str);
    static ibis_log_msg_function_t GetLogMsgFunction();

    static void SetLogMsgFunction(ibis_log_msg_function_t log_msg_function) {
        m_log_msg_function = log_msg_function;
    }
};


#include "csv_parser.hpp"
#endif /* CSV_PARSER_H_ */
