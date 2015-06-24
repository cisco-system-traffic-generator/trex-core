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


#include "TCPOptions.h"
#include "IPHeader.h"

bool                    TCPOptions::ourOneShotDump          = false;
TCPOptions::Counters    TCPOptions::ourCounters             = {0,0,0};

TCPOptions::TCPOptions(uint8_t*   argOptionsP, uint16_t argOptionsSize):
myOptionsP      (argOptionsP),
myOptionsSize   (argOptionsSize),
myCurrentOptionP((Option*)argOptionsP)
{
    ;
}

bool    TCPOptions::doesContain(TCPOptions::Kind::Val argKind)
{
    uint32_t  optionTypeIdx = 0;// Used to verify that we're not in infinite loop

    // we'll run over the whole list of TLVs and check each relative to the time stamp 
    // option kind value.
    do 
    {
        Kind::Val   theCurrentOptionKind;
        uint8_t       theOptionLength;
        getCurrentOption(theCurrentOptionKind, theOptionLength);
        optionTypeIdx++;
        if( theCurrentOptionKind == argKind )
        {
            return true;
        }
    } while( (nextOption() == true)  && (optionTypeIdx < MaxOptionsInPacket));

    if(optionTypeIdx >= MaxOptionsInPacket)
    {
        ourCounters.itsPossibleEndlessLoop++;
    }

    // none of the TLV was of type TimeStamp
    return false;
}

//--------------------------------------------------------------------------------------------------
// Set methods
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Miscellaneous Operations
//--------------------------------------------------------------------------------------------------

void    TCPOptions::dump(FILE*  argOutputFile)
{
    fprintf(argOutputFile, "\nNot supported yet!\n");
}

void    TCPOptions::get(TCPOptions::Counters& argCounters)
{
     argCounters.itsOptionsSizeMismatch = ourCounters.itsOptionsSizeMismatch;  
     argCounters.itsZeroLengthOptions   = ourCounters.itsZeroLengthOptions;    
     argCounters.itsPossibleEndlessLoop = ourCounters.itsPossibleEndlessLoop;  
}

uint8_t*  TCPOptions::getCurrentOption    (Kind::Val&    argKind, uint8_t& argLength)
{
    argKind     = (Kind::Val)myCurrentOptionP->theKind;
    argLength   = getCurrentOptionLength();
    return(uint8_t*)myCurrentOptionP;
}


uint8_t   TCPOptions::getCurrentOptionLength()
{
    if(myCurrentOptionP->theKind == Kind::NO_OP || 
       myCurrentOptionP->theKind == Kind::EOL)
    {
        return 1;
    }
    else
    {
        return myCurrentOptionP->theLength;
    }
}


// This is a patch for temporary packet dump facility
//
#define MAX_BUFFER_SIZE (8 * 2048)
static  char    theDumpBuffer[MAX_BUFFER_SIZE] ={0};

bool    TCPOptions::isLastOption        ()
{
    // Below is the total length of the options we've passed so far
    uint32_t  theCurrentOffset = (uint32_t)((uintptr_t)((char *)myCurrentOptionP - (char *)myOptionsP));

    uint32_t  theCurrentLength = getCurrentOptionLength();
    if( (theCurrentOffset + theCurrentLength) >= myOptionsSize )
    {
        // debug check
        if( (theCurrentOffset + theCurrentLength) > myOptionsSize )
        {
            ourCounters.itsOptionsSizeMismatch++;
            if(ourOneShotDump == false)
            {
                ourOneShotDump = true;//disable for next time
                uint8_t*    theIPPacket     = (uint8_t*)((uintptr_t)(myOptionsP - 40));
                IPHeader* theIPHeader     = (IPHeader*)theIPPacket;
                uint16_t    thePacketLength = theIPHeader->getTotalLength();

                if(thePacketLength < (MAX_BUFFER_SIZE / 8))
                {
                    int numWritten = sprintf(theDumpBuffer, "\nDump Size %u\n", thePacketLength);

                    for (uint32_t i = 0; i < thePacketLength; i++)
                    {
                        numWritten += sprintf(theDumpBuffer + numWritten,"%.2x ", theIPPacket[i]); 

                        if ((i % 16) == 15)
                        {
                            numWritten += sprintf(theDumpBuffer + numWritten,"\n");
                        }
                    }
                }
            }
        }
        return true;
    }
    else
    {
        return false;
    }
}

//--------------------------------------------------------------------------------------------------
// Set methods
//--------------------------------------------------------------------------------------------------

bool    TCPOptions::nextOption          ()
{
    if( isLastOption() == true )
    {
        return false;
    }
    else
    {
        uint32_t  theCurrentOptionLength = getCurrentOptionLength();
        if(theCurrentOptionLength > 0)
        {
            uint8_t*  theCurrentP = (uint8_t*)myCurrentOptionP;
            theCurrentP         += theCurrentOptionLength;
            myCurrentOptionP    = (Option*)theCurrentP;
            return true;
        }
        else
        {
            ourCounters.itsZeroLengthOptions++;
            return false;
        }
    }
}

