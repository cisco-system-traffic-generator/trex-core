#ifndef NONE_UNIFORM_PROB_H
#define NONE_UNIFORM_PROB_H

/*
taken from here https://oroboro.com/non-uniform-random-numbers/

Copyright (c) 2015-2017 oroboro 

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

#include <stdio.h>
#include <stdint.h>
#include <vector>

#define U32_MAX 0xFFFFFFFF
 
class KxuRand {
 public:
    virtual ~KxuRand() { ; }
    virtual uint32_t    getRandom() = 0;
    virtual double getRandomUnit() { return (((double)getRandom()) / U32_MAX ); }
 };
 
class KxuRandUniform : public KxuRand {
 public:
    virtual ~KxuRandUniform() { ; }
    virtual void setSeed( uint32_t seed ) = 0;
 
    uint32_t  getRandomInRange( uint32_t n ) 
       { uint64_t v = getRandom(); v *= n; return uint32_t( v >> 32 ); }
    uint32_t  getRandomInRange( uint32_t start, uint32_t end ) 
       { return getRandomInRange( end - start ) + start; }
};
 
// a dead simple Linear Congruent random number generator
class KxuLCRand : public KxuRandUniform {
 public:
    KxuLCRand( uint32_t seed = 555 ) { setSeed( seed ); }
    void setSeed( uint32_t seed ) { if ( !seed ) seed = 0x333; mState = seed | 1; }
    uint32_t getRandom() { mState = ( mState * 69069 ) + 1; return mState; }
 
 private:
    uint32_t mState;
};

typedef uint32_t u32 ;


class Distribution {  
public:
   Distribution() {}
   Distribution( u32 a, 
                 u32 b, 
                 u32 p ) { mA = a; mB = b, mProb = p; }
 
   u32 mA; 
   u32 mB; 
   u32 mProb; 
};

class KxuNuRand : public KxuRand { 
public:
   KxuNuRand( const std::vector<u32>& dist, KxuRandUniform* rand );
   KxuNuRand( const std::vector<double> prob, KxuRandUniform* rand );

   u32 getRandom();
 
protected:
   std::vector<Distribution> mDist;
   KxuRandUniform*           mRand;
private:
    void init(const std::vector<u32>& dist, KxuRandUniform* rand );
};

inline double fixedToFloat_0_32( u32 val ) { return (((double)val) / U32_MAX ); }
inline u32    floatToFixed_0_32( double val ) { return (u32)( val * U32_MAX ); }

void Kx_norm_prob(std::vector<double> prob,
                  std::vector<double> & result );

void Kx_dump_prob(std::vector<double> prob);


#endif
