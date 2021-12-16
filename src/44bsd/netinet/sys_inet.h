#ifndef _SYS_INET_H_
#define _SYS_INET_H_


#include <stdint.h>                         // uint32_t
#include <stddef.h>                         // NULL
#include <stdbool.h>                        // bool
#include <limits.h>                         // INT_MAX,UINT_MAX
#include <math.h>                           // pow@cc_cubic.c

#include <linux/errno.h>                    // EMSGSIZE
#include <sys/types.h>                      // u_int,u_long,...

#include <malloc.h>                         // malloc(),free()
#include <assert.h>                         // assert()


// --<sys>---------------------------------------------------------------


// <sys/cdefs.h>
#define __packed            __attribute__((__packed__))
#define __aligned(x)        __attribute__((__aligned__(x)))
#define __unused            __attribute__((unused))
#define __predict_true(exp) __builtin_expect(!!(exp), 1)


// <sys/systm.h>
#define KASSERT(exp,msg)    do {} while(0)
#if !defined(__cplusplus)
#define bcopy(from, to, len) __builtin_memmove((to), (from), (len))
#define memmove(dest, src, n) __builtin_memmove((dest), (src), (n))
#endif


// <sys/param.h>
#define roundup2(x, y)  (((x)+((y)-1))&(~((y)-1))) /* if y is powers of two */
#define howmany(x, y)   (((x)+((y)-1))/(y))
#define roundup(x, y)   ((((x)+((y)-1))/(y))*(y))  /* to any y */


// <sys/libkern.h>
static __inline int imax(int a, int b) { return (a > b ? a : b); }
static __inline int imin(int a, int b) { return (a < b ? a : b); }
static __inline long lmax(long a, long b) { return (a > b ? a : b); }
static __inline long lmin(long a, long b) { return (a < b ? a : b); }
static __inline u_int max(u_int a, u_int b) { return (a > b ? a : b); }
static __inline u_int min(u_int a, u_int b) { return (a < b ? a : b); }
static __inline u_long ulmin(u_long a, u_long b) { return (a < b ? a : b); }
static __inline u_long ulmax(u_long a, u_long b) { return (a > b ? a : b); }


// --<inet>--------------------------------------------------------------


#include <netinet/ip.h>
#define IPTOS_ECN_NOTECT    IPTOS_ECN_NOT_ECT
#include <netinet/ip6.h>


#endif /* !_SYS_INET_H_ */
