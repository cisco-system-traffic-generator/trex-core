/*
 * Copyright (c) 2022-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef IBDIAG_PPCC_H
#define IBDIAG_PPCC_H

#include <map>
#include <stdint.h>
#include <string>
#include <vector>

using namespace std;

#define ALGO_VERSION_HASH_MAKE(algoId, versionMajor, versionMinor) \
    ((((algoId) & 0xFFFF) << 16) | (((versionMajor) & 0xFF) << 8) | ((versionMinor) & 0xFF))
#define ALGO_VERSION_HASH_ALGO_ID(hash) (uint16_t)(((hash) >> 16) & 0xFFFFUL)
#define ALGO_VERSION_HASH_VERSION_MAJOR(hash) (uint8_t)(((hash) >> 8) & 0xFFUL)
#define ALGO_VERSION_HASH_VERSION_MINOR(hash) (uint8_t)((hash) & 0xFFUL)

class PPCCParameter
{
private:
    string name;

    uint32_t defVal;
    uint32_t minVal;
    uint32_t maxVal;

    uint8_t minMaxFlag; // 0 bit - min val exists, 1 bit - max val exists

    inline bool IsMinValPresent()
    {
        return minMaxFlag & 1;
    }

    inline bool IsMaxValPresent()
    {
        return minMaxFlag & 2;
    }

public:
    PPCCParameter(string name, uint32_t defVal) : name(name), defVal(defVal), minVal(0),
                                                  maxVal(0), minMaxFlag(0)
    {}

    string GetName()
    {
        return name;
    }

    uint32_t GetDefVal()
    {
        return defVal;
    }

    bool GetMinVal(uint32_t &minVal)
    {
        minVal = this->minVal;
        return IsMinValPresent();
    }

    void SetMinVal(uint32_t minVal)
    {
        this->minVal = minVal;
        minMaxFlag |= 1;
    }

    bool GetMaxVal(uint32_t &maxVal)
    {
        maxVal = this->maxVal;
        return IsMaxValPresent();
    }

    void SetMaxVal(uint32_t maxVal)
    {
        this->maxVal = maxVal;
        minMaxFlag |= 2;
    }
};

class PPCCAlgo
{
private:
    string name;
    uint32_t versionHash;
    vector<PPCCParameter> parameters;
    map<string, size_t> paramMap;
    vector<string> counters;

public:
    PPCCAlgo(const string &name, uint32_t versionHash) : name(name), versionHash(versionHash) {}

    const string& GetName() const { return name; }

    uint32_t GetVersionHash()
    {
        return versionHash;
    }

    PPCCParameter *GetParameter(size_t paramIdx)
    {
        if (paramIdx >= parameters.size())
            return NULL;

        return &parameters[paramIdx];
    }

    PPCCParameter *GetParameter(const string &paramName)
    {
        map<string, size_t>::iterator it = paramMap.find(paramName);
        if (it == paramMap.end())
            return NULL;

        return &parameters[it->second];
    }

    void AddParameter(const string &paramName, uint32_t paramDefVal)
    {
        parameters.push_back(PPCCParameter(paramName, paramDefVal));
        paramMap[paramName] = parameters.size() - 1;
    }

    string GetCounter(size_t counterIdx)
    {
        if (counterIdx >= counters.size())
            return {};

        return counters[counterIdx];
    }

    void AddCounter(const string &counter)
    {
        counters.push_back(counter);
    }
};

class PPCCAlgoDatabase
{
private:
    enum ParserGlobalState
    {
        PARSE_GLOBAL,
        PARSE_SECTION
    };

    enum ParserSectionState
    {
        PARSE_KEY,
        PARSE_VALUE
    };

    struct ParserPPCCAlgo
    {
        size_t startLine;
        size_t endLine;
        string name;
        uint32_t versionHash;
        vector<pair<string, uint32_t>> paramDefVals;
        vector<pair<string, uint32_t>> paramMinVals;
        vector<pair<string, uint32_t>> paramMaxVals;
        vector<string> counters;
    };

    static const string sectionStartStr;
    static const string sectionEndStr;
    static const string nameStr;
    static const string versionStr;
    static const string releaseDateStr;
    static const string descriptionStr;
    static const string authorStr;
    static const string supportedDevicesStr;
    static const string ppccParameterNameListStr;
    static const string ppccParameterMinValsStr;
    static const string ppccParameterMaxValsStr;
    static const string ppccCountersNameListStr;

    map<uint32_t, PPCCAlgo> algoMap;

private:
    int ParseUint32(const char *str, size_t line, uint32_t &val);

    int ParseTupleList(const char *str, vector<pair<string, string>> &tupleList);
    int ParseSimpleList(const char *str, vector<string> &simpleList);

    int HandleKeyVal(const string key, const string &val, size_t line, ParserPPCCAlgo &parserAlgo);
    int HandleLine(const string &key, const string &val, ParserGlobalState &globalState,
                   size_t line, ParserPPCCAlgo &parserAlgo);
    int FillNewAlgo(ParserPPCCAlgo &parserAlgo);

    int ParseFile(const string &fileName);

public:
    int ParseDir(const string &ppcc_path);
    PPCCAlgo *GetAlgo(uint16_t algoId, uint8_t versionMajor, uint8_t versionMinor);
};

#endif
