/*
 * Copyright (c) 2004-2010 Mellanox Technologies LTD. All rights reserved.
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


#ifndef IBDM_REXIFC_H
#define IBDM_REXIFC_H


#include <string.h>
#include <sys/types.h>
#include <regex.h>
#include <stdio.h>
#include <iostream>
#include <string>

using namespace std;


/*
 * This file holds simplified object oriented interface for regular expressions
 */


/////////////////////////////////////////////////////////////////////////////
class rexMatch {
private:
    const char *str;
    int nMatches;
    regmatch_t *matches;

 public:
    // no default constructor
    rexMatch();

    // construct:
    rexMatch(const char *s, int numMatches) {
        str = s;
        nMatches = numMatches;
        matches = new regmatch_t[nMatches + 1];
    };

    // destrutor:
    ~rexMatch() {
        delete [] matches;
    };

    // usefull:
    int field(int num, char *buf) {
        // check for non match:
        if (num > nMatches || matches[num].rm_so < 0) {
            buf[0] = '\0';
            return 0;
        }
        strncpy(buf, str + matches[num].rm_so, matches[num].rm_eo - matches[num].rm_so + 1);
        buf[matches[num].rm_eo - matches[num].rm_so] = '\0';
        return 0;
    };

    // string returning the field
    int field(int num, string &res) {
        string tmp = string(str);
        // check for non match:
        if (num > nMatches || matches[num].rm_so < 0) {
            res = "";
            return 0;
        }
        res = tmp.substr(matches[num].rm_so, matches[num].rm_eo - matches[num].rm_so);
        return 0;
    };

    string field(int num) {
        string tmp = string(str);
        // check for non match:
        if (num > nMatches || matches[num].rm_so < 0) {
            return string("");
        }
        return tmp.substr(matches[num].rm_so, matches[num].rm_eo - matches[num].rm_so);
    };

    inline int numFields() {return nMatches;};

    friend class regExp;
};


/////////////////////////////////////////////////////////////////////////////
// This will basically hold the compiled regular expression
class regExp {
private:
    regex_t re;
    char *expr;
    int status;

public:
    // prevent default constructor
    regExp();

    // construct
    regExp(const char *pattern, int flags = REG_EXTENDED) {
        expr = new char[strlen(pattern) + 1];
        strcpy(expr, pattern);
        status = regcomp(&re, expr, flags);
        if (status) {
            cout << "-E- Fail to compile regular expression:%s\n" << pattern << endl;
        }
    }

    // destructor
    ~regExp() {
        regfree(&re);
        delete [] expr;
    }

    // access:
    inline const char *getExpr() const {return expr;};
    inline int valid() const {return (status == 0);};

    // usefullnss:
    class rexMatch * apply(const char *str, int flags = 0) {
        class rexMatch *res = new rexMatch(str, (int)re.re_nsub);
        if (regexec(&re, str, re.re_nsub + 1, res->matches, flags)) {
            delete res;
            return 0;
        }
        return res;
    }

    friend class rexMatch;
};


#endif /* IBDM_REXIFC_H */

