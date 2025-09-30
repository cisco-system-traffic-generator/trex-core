/*
 * Copyright (c) 2004-2010 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 2025-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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


#ifndef _OPT_PARSER_H_
#define _OPT_PARSER_H_


#include <stdlib.h>
#include <list>
#include <vector>
#include <map>
#include <set>
#include <string>
using namespace std;


/******************************************************/
typedef struct option_ifc {
    string option_name;             //must be valid
    char   option_short_name;       //can be used as short option, if space than no short flag
    string option_value;            //if "" has no argument
    string description;             //will be display in usage
    string default_value_str;
    u_int32_t attributes;           //attributes - ibis_cmd_opt_attribute_flags_t

    option_ifc() : option_short_name(0), attributes(0) {}
} option_ifc_t;

typedef enum ibis_cmd_opt_attribute_flags {          //values increase by power of 2
    IBIS_CMD_DEFAULT_OPT_ATTRIBUTES_FLAGS = 0,       //default attribute flags for option
    IBIS_CMD_NO_CONF_FILE = 0x1,                     //if not to generate in configuration file
    IBIS_CMD_HIDDEN_BIT = 0x2,
    IBIS_CMD_HIDDEN = IBIS_CMD_HIDDEN_BIT | IBIS_CMD_NO_CONF_FILE, //if not to dump in help message
    IBIS_CMD_INFO_FLAG_BIT = 0x4,
    IBIS_CMD_INFO_FLAG = IBIS_CMD_INFO_FLAG_BIT | IBIS_CMD_NO_CONF_FILE, //Just info about the app
    IBIS_CMD_DEPRECATED_BIT = 0x8,
    IBIS_CMD_DEPRECATED = IBIS_CMD_DEPRECATED_BIT | IBIS_CMD_NO_CONF_FILE, //if the flag is deprecated
    IBIS_CMD_BOOLEAN_FLAG = 0x10,                    //if the type of flag is boolean
} ibis_cmd_opt_attribute_flags_t;

typedef vector < struct option_ifc > vec_option_t;


class CommandLineRequester {
private:
    // members
    vec_option_t options;
    string name;
    string description;

    // methods
    string BuildOptStr(option_ifc_t &opt);

public:
    // methods
    CommandLineRequester(const string & req_name) : name(req_name) {}
    virtual ~CommandLineRequester() {}

    inline void AddOptions(option_ifc_t options[], int arr_size) {
        for (int i = 0; i < arr_size; ++i)
            this->options.push_back(options[i]);
    }
    inline void AddOptions(const string & option_name,
            char option_short_name,
            const string & option_value,
            const string & description,
            bool hidden = false) {
        option_ifc_t opt;
        opt.option_name = option_name;
        opt.option_short_name = option_short_name;
        opt.option_value = option_value;
        opt.description = description;
        if (hidden)
            opt.attributes = IBIS_CMD_HIDDEN;
        else
            opt.attributes = IBIS_CMD_DEFAULT_OPT_ATTRIBUTES_FLAGS;
        this->options.push_back(opt);
    }
    inline void AddOptions(string option_name,
            char option_short_name,
            string option_value,
            string description,
            const char *default_value_str,
            u_int32_t attributes = IBIS_CMD_DEFAULT_OPT_ATTRIBUTES_FLAGS) {
        this->AddOptions(option_name, option_short_name, option_value, description,
                         string(default_value_str), attributes);
    }
    inline void AddOptions(const string & option_name,
            char option_short_name,
            const string & option_value,
            const string & description,
            const string & default_value_str,
            u_int32_t attributes = IBIS_CMD_DEFAULT_OPT_ATTRIBUTES_FLAGS) {
        option_ifc_t opt;
        opt.option_name = option_name;
        opt.option_short_name = option_short_name;
        opt.option_value = option_value;
        opt.description = description;
        opt.default_value_str = default_value_str;
        opt.attributes = attributes;
        this->options.push_back(opt);
    }
    inline void AddDescription(const string & desc) { this->description = desc; }
    inline vec_option_t& GetOptions() { return this->options; }
    inline string& GetName() { return this->name; }

    inline int ParseBoolValue(string value, bool &result) {
        if (!(strncasecmp(value.c_str(), "FALSE", strlen("FALSE") + 1))) {
            result = false;
            return 0;
        }

        if (!(strncasecmp(value.c_str(), "TRUE", strlen("TRUE") + 1))) {
            result = true;
            return 0;
        }

        return 1;
    }

    inline bool IsValidBoolValue(string value) {
        if ((strncasecmp(value.c_str(), "TRUE", strlen("TRUE") + 1)) &&
            (strncasecmp(value.c_str(), "FALSE", strlen("FALSE") + 1)))
            return false;

        return true;
    }

    string GetUsageSynopsys(bool hidden_options = false);
    string GetUsageDescription();
    string GetUsageOptions(bool hidden_options = false);

    //Returns: 0 - OK / 1 - parsing error / 2 - OK but need to exit / 3 - some other error
    virtual int HandleOption(string name, string value) = 0;
};


/******************************************************/
typedef list < CommandLineRequester * > list_p_command_line_req;
typedef map < char, string > map_char_str;
typedef map < string, CommandLineRequester * > map_str_p_command_line_req;


class CommandLineParser {
private:
    // members
    list_p_command_line_req p_requesters_list;
    map_char_str short_opt_to_long_opt;
    map_str_p_command_line_req long_opt_to_req_map;

    string name;
    string last_error;
    string last_unknown_options;

    // methods
    void SetLastError(const char *fmt, ...);

    // helper function to ParseOptions
    void ParseOptionsClear(int argc, 
                           char **internal_argv,
                           struct option *options_arr);

public:

    set < string > args;

    // methods
    CommandLineParser(const string & parser_name) :
        name(parser_name), last_error(""), last_unknown_options("") {}
    ~CommandLineParser() {}

    inline const char * GetErrDesc() const { return this->last_error.c_str(); }
    inline const char * GetUnknownOptions() const { return this->last_unknown_options.c_str(); }
    inline list_p_command_line_req GetRequestersList() const { return this->p_requesters_list; }

    int AddRequester(CommandLineRequester *p_req);      //if multiple option than fail

    int ParseOptions(int argc, const char **argv,
            bool to_ignore_unknown_options = false,
            list_p_command_line_req *p_ignored_requesters_list = NULL);

    string GetUsage(bool hidden_options = false);
};


#endif          /* not defined _OPT_PARSER_H_ */
