#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <setjmp.h>
#include <signal.h>
#include "efence.h"

/*
 * Electric Fence confidence tests.
 * Make sure all of the various functions of Electric Fence work correctly.
 */

#ifndef	PAGE_PROTECTION_VIOLATED_SIGNAL
#define	PAGE_PROTECTION_VIOLATED_SIGNAL	SIGSEGV
#endif

struct diagnostic {
	int		(*test)(void);
	int		expectedStatus;
	const char *	explanation;
};

extern int	EF_PROTECT_BELOW;
extern int	EF_ALIGNMENT;

static sigjmp_buf	env;

/*
 * There is still too little standardization of the arguments and return
 * type of signal handler functions.
 */
static
void
segmentationFaultHandler(
int signalNumber
#if ( defined(_AIX) )
, ...
#endif
)
 {
	signal(PAGE_PROTECTION_VIOLATED_SIGNAL, SIG_DFL);
	siglongjmp(env, 1);
}

static int
gotSegmentationFault(int (*test)(void))
{
	if ( sigsetjmp(env,1) == 0 ) {
		int			status;

		signal(PAGE_PROTECTION_VIOLATED_SIGNAL
		,segmentationFaultHandler);
		status = (*test)();
		signal(PAGE_PROTECTION_VIOLATED_SIGNAL, SIG_DFL);
		return status;
	}
	else
		return 1;
}

static char *	allocation;
/* c is global so that assignments to it won't be optimized out. */
char	c;

static int
testSizes(void)
{
	/*
	 * If ef_number can't hold all of the bits of a void *, have the user
	 * add -DUSE_ LONG_LONG to the compiler flags so that ef_number will be
	 * declared as "unsigned long long" instead of "unsigned long".
	 */
	return ( sizeof(ef_number) < sizeof(void *) );
}

static int
allocateMemory(void)
{
	allocation = (char *)malloc(1);

	if ( allocation != 0 )
		return 0;
	else
		return 1;
}

static int
freeMemory(void)
{
	free(allocation);
	return 0;
}

static int
protectBelow(void)
{
	EF_PROTECT_BELOW = 1;
	return 0;
}

static int
read0(void)
{
	c = *allocation;

	return 0;
}

static int
write0(void)
{
	*allocation = 1;

	return 0;
}

static int
read1(void)
{
	c = allocation[1];

	return 0;
}

static int
readMinus1(void)
{
	c = allocation[-1];
	return 0;
}

static struct diagnostic diagnostics[] = {
	{
		testSizes, 0,
		"Please add -DLONG_LONG to the compiler flags and recompile."
	},
	{
		allocateMemory, 0,
		"Allocation 1: This test allocates a single byte of memory."
	},
	{
		read0, 0,
		"Read valid memory 1: This test reads the allocated memory."
	},
	{
		write0, 0,
		"Write valid memory 1: This test writes the allocated memory."
	},
	{
		read1, 1,
		"Read overrun: This test reads beyond the end of the buffer."
	},
	{
		freeMemory, 0,
		"Free memory: This test frees the allocated memory."
	},
	{
		protectBelow, 0,
		"Protect below: This sets Electric Fence to protect\n"
		"the lower boundary of a malloc buffer, rather than the\n"
		"upper boundary."
	},
	{
		allocateMemory, 0,
		"Allocation 2: This allocates memory with the lower boundary"
		" protected."
	},
	{
		read0, 0,
		"Read valid memory 2: This test reads the allocated memory."
	},
	{
		write0, 0,
		"Write valid memory 2: This test writes the allocated memory."
	},
	{
		readMinus1, 1,
		"Read underrun: This test reads before the beginning of the"
		" buffer."
	},
	{
		0, 0, 0
	}
};

static const char	failedTest[]
 = "Electric Fence confidence test failed.\n";

static const char	newline = '\n';

int
main(int argc, char * * argv)
{
	static const struct diagnostic *	diag = diagnostics;
	

	EF_PROTECT_BELOW = 0;
	EF_ALIGNMENT = 0;

	while ( diag->explanation != 0 ) {
		int	status = gotSegmentationFault(diag->test);

		if ( status != diag->expectedStatus ) {
			/*
			 * Don't use stdio to print here, because stdio
			 * uses malloc() and we've just proven that malloc()
			 * is broken. Also, use _exit() instead of exit(),
			 * because _exit() doesn't flush stdio.
			 */
			write(2, failedTest, sizeof(failedTest) - 1);
			write(2, diag->explanation, strlen(diag->explanation));
			write(2, &newline, 1);
			_exit(-1);
		}
		diag++;
	}
	return 0;
}
