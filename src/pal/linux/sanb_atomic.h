#ifndef SANB_ATOMIC_
#define SANB_ATOMIC_
#include <stdlib.h>

#define  FORCE_NON_INILINE  __attribute__((noinline))  

static inline void sanb_smp_store_memory_barrier (void)
{
    asm volatile ("sfence":::"memory");
}

static inline void sanb_smp_load_memory_barrier (void)
{
    asm volatile ("lfence":::"memory");
}

static inline void sanb_smp_memory_barrier (void)
{
    asm volatile ("mfence":::"memory");
}


static inline bool
sanb_atomic_compare_and_set_32 (uint32_t old_value, uint32_t new_value,
			   volatile uint32_t *addr)
{
    long result;
#if __WORDSIZE == 64 || defined(__x86_64__)
    asm volatile ("   lock cmpxchgl %2, 0(%3);" /* do the atomic operation   */
		  "   sete %b0;"      /* on success the ZF=1, copy that to   */
				      /* the low order byte of %eax (AKA %al)*/
		  "   movzbq %b0, %0;"/* zero extend %al to all of %eax      */
		  : "=a" (result)
		  : "0" (old_value), "q" (new_value), "r" (addr)
		  : "memory" );
#else
    asm volatile ("   lock cmpxchgl %2, 0(%3);" /* do the atomic operation   */
		  "   sete %b0;"      /* on success the ZF=1, copy that to   */
				      /* the low order byte of %eax (AKA %al)*/
		  "   movzbl %b0, %0;"/* zero extend %al to all of %eax      */
		  : "=a" (result)
		  : "0" (old_value), "q" (new_value), "r" (addr)
		  : "memory" );
#endif    
    return (bool)result;
}



/*
 * FIXME: on some processors the cmpxchg8b() instruction does not exist. On
 *   those processors this will cause a seg-fault. The only way to implement
 *   this operation on such a processor is to use a global lock.
 */
static inline bool
sanb_atomic_compare_and_set_64 (uint64_t old_value, uint64_t new_value,
			   volatile void *ptr)
{
    volatile uint64_t *addr = (volatile uint64_t *)ptr;
#if __WORDSIZE == 64 || defined(__x86_64__)
    uint64_t prev;
    asm volatile ("   lock cmpxchgq %2, 0(%3);" /* do the atomic operation      */
		  : "=a" (prev)     /* result will be stored in RAX        */
		  : "0" (old_value), "q" (new_value), "r" (addr)
		  : "memory");
    return (bool) (prev == old_value);
#else
    uint64_t result;
    asm volatile ("   movl 0(%2), %%ebx;"    /* load ECX:EBX with new_value  */
		  "   movl 4(%2), %%ecx;"
	          "   lock cmpxchg8b 0(%3);" /* do the atomic operation      */
		  "   sete %b0;\n"      /* on success the ZF=1, copy that to   */
				      /* the low order byte of %eax (AKA %al)*/
		  "   movzbl %b0, %0;"/* zero extend %al to all of %eax      */
		  : "=A" (result)     /* result will be stored in EDX:EAX    */
		  : "0" (old_value), "r" (&new_value), "r" (addr)
		  : "memory", "ecx", "ebx" );
    return (bool) result;
#endif    
}


static inline void
sanb_atomic_add_32 (volatile uint32_t *addr, uint32_t increment)
{
    asm volatile ("   lock addl %0, 0(%1);"
		  :
		  : "q" (increment), "r" (addr)
		  : "memory" );
}


static inline void
sanb_atomic_subtract_32 (volatile uint32_t *addr, uint32_t decrement)
{
    asm volatile ("   lock subl %0, 0(%1);"
		  :
		  : "q" (decrement), "r" (addr)
		  : "memory" );
}


/*
 * It is not possible to do an atomic 64 bit add in 32-bit mode. Fortunately
 * it is possible to do a 64-bit cmpxchg, so we can use that to implement a
 * 64-bit atomic_add.
 */
static inline void
sanb_atomic_add_64 (volatile void *ptr, uint64_t increment)
{
    volatile uint64_t *addr = (volatile uint64_t *)ptr;
    uint64_t original;

    do {
	original = *addr;
    } while (!sanb_atomic_compare_and_set_64(original, original + increment, addr));
}



static inline uint32_t sanb_atomic_dec2zero32(volatile void *ptr){
    volatile uint32_t *addr = (volatile uint32_t *)ptr;
    uint32_t original;
    do {
    original = *addr;
    } while (!sanb_atomic_compare_and_set_32(original,  original ? (original - 1):0, addr));
    return (original);
}



static inline uint32_t
sanb_atomic_add_return_32_old (volatile uint32_t *addr, uint32_t increment)
{
    uint32_t original;
    
    asm volatile ("   lock xaddl %1, 0(%2);"
		  : "=r" (original)
		  : "0" (increment), "q" (addr)
		  : "memory" );
    return original ;
}


static inline uint64_t
sanb_atomic_add_return_64_old (volatile void *ptr, uint64_t increment)
{
    volatile uint64_t *addr = (volatile uint64_t *)ptr;
    uint64_t original;

    do {
	original = *addr;
    } while (!sanb_atomic_compare_and_set_64(original, original + increment, addr));
    return original ;
}



static inline void* 
sanb_mem_read_ptr(void **v) {

    volatile void **p=(volatile void **)v;
    return ((void *)(*p));
}

static inline void 
sanb_mem_write_ptr(void **v,void *value) {

    volatile void **p=(volatile void **)v;
    *p = value;
}



#endif
