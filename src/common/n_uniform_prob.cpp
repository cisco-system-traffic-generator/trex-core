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


#include <common/n_uniform_prob.h>
#include <assert.h>


u32 KxuNuRand::getRandom() { 
   const Distribution& dist = mDist[mRand->getRandomInRange( mDist.size() )]; 
   return ( mRand->getRandom() <= dist.mProb ) ? dist.mA : dist.mB; 
}

static void prob_conv_fix_size(std::vector<double> prob,
                           std::vector<u32> & result ){
   double sum=0.0;
   for (auto& n : prob)
      sum += n;
   result.clear();

   int i;
   for (i=0; i<prob.size(); i++) {
       result.push_back(floatToFixed_0_32(prob[i]/sum));
   }
}


static void normProbs( std::vector<u32>& probs );
static void computeBiDist( std::vector<u32>& p, u32 n, 
                           Distribution& dist, u32 aIdx, u32 bIdx );


// Since we are using fixed point math, we first implement 
// a fixed 0.32 point inversion:  1/(n-1) 
#define DIST_INV( n ) ( 0xFFFFFFFF/( (n) - 1 ))


KxuNuRand::KxuNuRand( const std::vector<double> prob, 
                      KxuRandUniform* rand ){
    std::vector<u32>  fix_p;
    prob_conv_fix_size(prob,fix_p);

    init(fix_p, rand);
}


void KxuNuRand::init(const std::vector<u32>& dist, 
                     KxuRandUniform* rand ){
    mRand = rand;

    if ( dist.size() == 1 ){
       // handle a the special case of just one symbol.
       mDist.push_back( Distribution( 0, 0, 0 ));

    } else {

       // The non-uniform distribution is passed in as an argument to the
       // constructor. This is a series of integers in the desired proportions.
       // Normalize these into a series of probabilities that sum to 1, expressed 
       // in 0.32 fixed point.
       std::vector<u32> p; p = dist;
       normProbs( p );

       // Then we count up the number of non-zero probabilities so that we can 
       // figure out the number of distributions we need.
       u32 numDistros = 0;
       for ( u32 i = 0; i < p.size(); i++ )
          if ( p[i] ) numDistros++;
       if ( numDistros < 2 ) 
           numDistros = 2;
       u32 thresh = DIST_INV( numDistros );

       // reserve space for the distributions.      
       mDist.resize( numDistros - 1 );

       u32 aIdx = 0; 
       u32 bIdx = 0; 
       for ( u32 i = 0; i < mDist.size(); i++ ) { 
          // find a small prob, non-zero preferred
          while ( aIdx < p.size()-1 ){
             if (( p[aIdx] <= thresh ) && p[aIdx] ) break;
             aIdx++;
          }
          assert (aIdx < p.size());
          if ( p[aIdx] > thresh ){
             aIdx = 0;
             while ( aIdx < p.size()-1 ){
                if ( p[aIdx] <= thresh ) break;
                aIdx++;
             }
          }
          assert (aIdx < p.size());
          // find a prob that is not aIdx, and the sum is more than thresh.
          while ( bIdx < p.size()-1 ){
             if ( bIdx == aIdx ) { bIdx++; continue; } // can't be aIdx
             assert (bIdx < p.size());
             if ((( p[aIdx] >> 1 ) + ( p[bIdx] >> 1 )) >= ( thresh >> 1 )) break;
             bIdx++; // find a sum big enough or equal
          }

          // We've selected 2 symbols, at indexes aIdx, and bIdx.
          // This function will initialize a new binary distribution, and make 
          // the appropriate adjustments to the input non-uniform distribution.
          computeBiDist( p, numDistros, mDist[i], aIdx, bIdx );
          assert (bIdx < p.size());
          if (( bIdx < aIdx ) && ( p[bIdx] < thresh ))
             aIdx = bIdx;
          else
             aIdx++;
       }       
    }
}

KxuNuRand::KxuNuRand( const std::vector<u32>& dist, KxuRandUniform* rand ){
    init(dist, rand);
}


static void computeBiDist( std::vector<u32>& p, u32 n, 
                           Distribution& dist, u32 aIdx, u32 bIdx ) { 
   dist.mA = aIdx; 
   dist.mB = bIdx; 
   if ( aIdx == bIdx ){
      dist.mProb = 0;
   } else {
      if ((( p[aIdx] >> 1 ) * ( n - 1 )) >= 0x80000000 ){
          dist.mProb = 0xFFFFFFFF;
      }else{
          dist.mProb = p[aIdx] * ( n - 1 ); 
      }
      p[bIdx] -= ( DIST_INV( n ) - p[aIdx] );
   }
   p[aIdx] = 0; 
} 

static bool tryDistributingErrorToBiggest(std::vector<u32>& probs){
    /* Returns true if the error is smaller than the max prob and can be distributed
    to it. False otherwise */
    u32 max = 0;
    u32 maxIdx = 0;
    u32 sum = 0;

    for (u32 i = 0; i < probs.size(); i++ ){
        sum += probs[i];
        if (probs[i] > max) {
            max = probs[i];
            maxIdx = i;
        }
    }
    u32 error = -sum; //4G - sum
    u32 AllowedError = max / 1000 ; // We permit an error of 0.1% of the maximum.
    if (error < AllowedError) {
        // Distribute the error to the max, it is indistinguishable (0.1%)
        probs[maxIdx] += error;
        return true;
    }
    return false; 
}

static void distributeErrorRelatively(std::vector<u32>& probs){
    u32 sum = 0;
    u32 error = 0;
    u32 errorPerEntry = 0;

    for (u32 i = 0; i < probs.size(); i++) {
        sum += probs[i];
    }
    error = - sum; // 4G - sum

    for (u32 i = 0; i < probs.size(); i++) {
        errorPerEntry = error * fixedToFloat_0_32(probs[i]);
        probs[i] += errorPerEntry;
    }
}

static void distributeOnesToNoError(std::vector<u32>& probs){
    u32 sum = 0;
    u32 error = 0;

    for (u32 i = 0; i < probs.size(); i++) {
        sum += probs[i];
    }
    error = - sum;
    assert (error < probs.size());

    while(error){
        probs[--error]++;
    }
}

static void normProbs( std::vector<u32>& probs ){

   // how many non-zero probabilities?
   u32 numNonZero = 0;
   for ( u32 i = 0; i < probs.size(); i++ ){
       if ( probs[i] ) numNonZero++;
   }
 
   if ( numNonZero == 0 ){
      // degenerate all zero probability array. 
      // Can't do anything with it... result is undefined
      assert(0);
      return;
   }
 
   if ( numNonZero == 1 ){
      // trivial case with only one real prob - handle special because 
      // computation would overflow below anyway.
      for ( u32 i = 0; i < probs.size(); i++ ){
          probs[i] = probs[i] ? U32_MAX : 0;
      }
      return;
   }

   if(!tryDistributingErrorToBiggest(probs)) {
       distributeErrorRelatively(probs);
       if (!tryDistributingErrorToBiggest(probs)) {
           distributeOnesToNoError(probs);
       }
   }

   // The sum of the probabilities in a probability distribution is always 1, in our case 4G = 0.
   uint64_t sum = 0;
   uint64_t fourGiga = 0x100000000; // 2^32
   for ( u32 i = 0; i < probs.size(); i++ ) {
       sum += probs[i];
   }
   assert (sum == fourGiga);
}

void Kx_norm_prob(std::vector<double> prob,
                  std::vector<double> & result ){

       double sum=0.0;
       for (auto& n : prob)
          sum += n;
       result.clear();

       int i;
       for (i=0; i<prob.size(); i++) {
           result.push_back(prob[i]/sum);
       }
}

void Kx_dump_prob(std::vector<double> prob){
    int i;
    for (i=0; i<prob.size(); i++) {
        printf(" %f \n", prob[i]*100.0);
    }
}

