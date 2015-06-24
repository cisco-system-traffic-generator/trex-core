#ifndef BIT_MAN_H
#define BIT_MAN_H
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



template <class T>
inline T btGetShift(unsigned int stopbit){
    return(T)((sizeof(T)*8)-stopbit-1);
}

//this function return mask with 1 from the start bit 
// 0 in this bit are the MSB - big edian mode
// if T is int (32) bit 31 is the last                                                     
template <class T>
inline T btGetMask(unsigned int startbit,
				   unsigned int stopbit){
    register T shft=btGetShift<T>(stopbit);
    return ((T)( (((1<<(stopbit-startbit+1))-1)<<shft)) );
}


//this function are used for big endian mode                      
// e.x btGetMaskBitBigE(0x80000000,0,0)==1
// e.x btGetMaskBitBigE(0xc0000000,0,1)==3
template <class T>
inline T btGetMaskBitBigE(T a, 
                          int startbit,
                          int stopbit ) {
    if((sizeof(T) * 8) == (stopbit - startbit + 1))// the case where the mask is the whole data
    {
        return a;
    }
    else
    {
    	register T mask=btGetMask<T>(startbit,stopbit);
    	register T shift=btGetShift<T>(stopbit);
    	T result;
    	result=((a & mask) >>shift);
    	return(result);
    }
}

inline uint32_t btGetMaskBitBigE32(uint32_t a, 
                                       int startbit,
                                       int stopbit ) {
    return(btGetMaskBitBigE<uint32_t>(a,startbit,stopbit));
}

inline unsigned short btGetMaskBitBigE16(uint16_t a, 
                                       int startbit,
                                       int stopbit ) {
     return(btGetMaskBitBigE<uint16_t>(a,startbit,stopbit));
}

inline uint8_t btGetMaskBitBigE8(uint8_t a, 
                                       int startbit,
                                       int stopbit ) {
     return(btGetMaskBitBigE<uint8_t>(a,startbit,stopbit));
}


template <class T>
inline void btSetMaskBitBigE(T & a, 
                             int startbit,
                             int stopbit,
                             T  newval) {
    if((sizeof(T) * 8) == (stopbit - startbit + 1))// the case where the mask is the whole data
    {
        a = newval;
    }
    else
    {
	    register T mask=btGetMask<T>(startbit,stopbit);
	    register T shift=btGetShift<T>(stopbit);
	    a=((a & ~mask) | ( (newval <<shift) & mask ) );
    }
}



inline void btSetMaskBitBigE32(uint32_t & a, 
                               int           startbit,
                               int           stopbit,
                               uint32_t  newVal
                                ) {
     btSetMaskBitBigE<uint32_t>(a,startbit,stopbit,newVal);
}

inline void btSetMaskBitBigE16(uint16_t  & a, 
                               int            startbit,
                               int            stopbit,
                               uint16_t newVal  ) {
     btSetMaskBitBigE<uint16_t>(a,startbit,stopbit,newVal);
}

inline void  btSetMaskBitBigE8(uint8_t & a, 
                               int           startbit,
                               int           stopbit,
                               uint8_t newVal ) {
     btSetMaskBitBigE<uint8_t>(a,startbit,stopbit,newVal);
}



template <class T>
inline T btGetMaskBit(T a, 
                      int startbit,
                      int stopbit ) {
    return(btGetMaskBitBigE<T>(a,(sizeof(T)*8)-1-startbit,(sizeof(T)*8)-1-stopbit));
}

template <class T>
inline void btSetMaskBit(T & a, 
                             int startbit,
                             int stopbit,
                             T  newval) {
    btSetMaskBitBigE<T>(a,(sizeof(T)*8)-1-startbit,((sizeof(T)*8)-1-stopbit),newval);
}


inline unsigned int btGetMaskBit32(unsigned int a, 
                                       int startbit,
                                       int stopbit ) {
     return(btGetMaskBit<unsigned int>(a,startbit,stopbit));
}

inline unsigned short btGetMaskBit16(unsigned short a, 
                                       int startbit,
                                       int stopbit ) {
     return(btGetMaskBit<unsigned short>(a,startbit,stopbit));
}

inline uint8_t btGetMaskBit8(uint8_t a, 
                                       int startbit,
                                       int stopbit ) {
     return(btGetMaskBit<uint8_t>(a,startbit,stopbit));
}


inline void btSetMaskBit32(unsigned int & a, 
                               int           startbit,
                               int           stopbit,
                               unsigned int  newVal
                                ) {
     btSetMaskBit<unsigned int>(a,startbit,stopbit,newVal);
}

/* start > stop             startbit = 10 , 
                            stop    = 8 

count like big E

*/
inline void btSetMaskBit16(unsigned short & a, 
                               int            startbit,
                               int            stopbit,
                               unsigned short newVal  ) {
     btSetMaskBit<unsigned short>(a,startbit,stopbit,newVal);
}

inline void  btSetMaskBit8(uint8_t & a, 
                               int           startbit,
                               int           stopbit,
                               uint8_t newVal ) {
     btSetMaskBit<uint8_t>(a,startbit,stopbit,newVal);
}

#endif


