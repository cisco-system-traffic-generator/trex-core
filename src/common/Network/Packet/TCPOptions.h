/*
Copyright (c) 2015-2015 Cisco Systems, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#ifndef _TCP_OPTIONS_H_
#define _TCP_OPTIONS_H_

#include "PacketHeaderBase.h"


class TCPOptions
{

public:

//--------------------------------------------------------------------------------------------------
// Typedef & Enum
//--------------------------------------------------------------------------------------------------

    struct  Kind
    {
        enum Val
        {
            EOL         =   0,
            NO_OP       =   1,
            MSS         =   2,
            WIN_SCL     =   3,
            SACK_PER    =   4,
            SACK        =   5,
            TIME_STAMP  =   8
        };
    };

    enum
    {
        TimeStampSize   =   12
    };

    enum 
    {
        MaxOptionsInPacket = 64
    };

    struct  Option
    {
        uint8_t   theKind;
        uint8_t   theLength; //Not always valid, depends on theKind
        uint8_t   theData[1];//variable. [1] only for compilation
    };

    struct  Counters
    {
        uint32_t    itsOptionsSizeMismatch; 
        uint32_t    itsZeroLengthOptions;  
        uint32_t    itsPossibleEndlessLoop;
    };


//--------------------------------------------------------------------------------------------------
// Constructor & Destructor
//--------------------------------------------------------------------------------------------------

    TCPOptions(uint8_t*   argOptionsP, uint16_t argOptionsSize);

//--------------------------------------------------------------------------------------------------
// Utility methods
//--------------------------------------------------------------------------------------------------

    /**
     * This method allows the user of this class to query whether
     * the options in hand contains a specific type of its
     * interest.
     * 
     * @return  true if the specified option exist.
     */
    bool    doesContain(Kind::Val);

//--------------------------------------------------------------------------------------------------
// Get methods
//--------------------------------------------------------------------------------------------------


    /**
     * Returns a pointer to the current option.
     * With the kind and length of it.
     * 
     * @param argKind
     * @param argLength
     * @return 
     */
    uint8_t*  getCurrentOption    (Kind::Val&    argKind, uint8_t& argLength);

    /**
     * This method gives the length of the current option.
     * Can vary since there are two options that are exception to
     * the usual TLV definition. 
     * 
     * @return 
     */
    uint8_t   getCurrentOptionLength();

    /**
     * This is an internal method that verifies based on the current
     * pointer, the total options length and the current options length,
     * whether this is the last option.
     * 
     * @return 
     */
    bool    isLastOption        ();

//--------------------------------------------------------------------------------------------------
// Set methods
//--------------------------------------------------------------------------------------------------

    /**
     * This method tells the object to advance one option ahead.
     * i.e. to the next option.
     * 
     * @return True if next option exist.
     *         False if we are at the last option.
     */
    bool    nextOption          ();


//--------------------------------------------------------------------------------------------------
// Miscellaneous Operations
//--------------------------------------------------------------------------------------------------

    static  void get (Counters&);

            void dump(FILE* argOutputFile);


private:

//--------------------------------------------------------------------------------------------------
// Data members
//--------------------------------------------------------------------------------------------------

    uint8_t*           myOptionsP;
    uint16_t           myOptionsSize;

    Option*          myCurrentOptionP;

    static  Counters ourCounters;

    static  bool     ourOneShotDump;
};


#endif // _TCP_OPTIONS_H_

