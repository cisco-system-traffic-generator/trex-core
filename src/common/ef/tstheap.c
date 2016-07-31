#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>
#include "efence.h"

/*
 * This is a simple program to exercise the allocator. It allocates and frees
 * memory in a pseudo-random fashion. It should run silently, using up time
 * and resources on your system until you stop it or until it has gone
 * through TEST_DURATION (or the argument) iterations of the loop.
 */

extern C_LINKAGE double drand48(void); /* For pre-ANSI C systems */

#define	POOL_SIZE	1024
#define	LARGEST_BUFFER	30000
#define	TEST_DURATION	1000000

void *	pool[POOL_SIZE];

#ifdef	FAKE_DRAND48
/*
 * Add -DFAKE_DRAND48 to your compile flags if your system doesn't
 * provide drand48().
 */

#ifndef	ULONG_MAX
#define	ULONG_MAX	~(1L)
#endif

double
drand48(void)
{
	return (random() / (double)ULONG_MAX);
}
#endif

int
main(int argc, char * * argv)
{
	int	count = 0;
	int	duration = TEST_DURATION;

	if ( argc >= 2 )
		duration = atoi(argv[1]);

	for ( ; count < duration; count++ ) {
		void * *	element = &pool[(int)(drand48() * POOL_SIZE)];
		size_t		size = (size_t)(drand48() * (LARGEST_BUFFER + 1));

		if ( *element ) {
			free( *element );
			*element = 0;
		}
		else if ( size > 0 ) {
			*element = malloc(size);
		}
	}
	return 0;
}
