#ifndef CONFIG_H
#define CONFIG_H

#include "machine.h"

#define ADD_CACHE_SIZE 4096
#define ADD_CACHE_MAX_LENGTH 256

/*
 * Max size of a file allowed to be read by 'read_file()'.
 * ignored in batchmode
 */
#define READ_FILE_MAX_SIZE	50000

/* Version of the game in the form x.xx.xx (leading zeroes) gc.
 * Two digits will be appended, that is the patch level.
 */
#define GAME_VERSION "4.05."

/*
 * If an object is left alone for a certain time, then the
 * function clean_up will be called. This function can do anything,
 * like destructing the object. If the function isn't defined by the
 * object, then nothing will happen.
 *
 */
#define TIME_TO_CLEAN_UP	3600

/*
 * How many seconds until an object is reset again.
 * Set this value high if big system, otherwise low.
 * No castles:	 1800	(30 minutes)
 * >100 castles:10000	(almost 3 hours).
 */
#define TIME_TO_RESET	900	/* one quarter */


/*
 * Define the maximum stack size of the stack machine. This stack will also
 * contain all local variables and arguments.
 */
#define EVALUATOR_STACK_SIZE	50000

/*
 * Define the maximum call depth for functions.
 */
#define MAX_TRACE		90

/*
 * Define the size of the compiler stack. This defines how complex
 * expressions the compiler can parse. The value should be big enough.
 */
#define COMPILER_STACK_SIZE	4000

/*
 * Max number of local variables in a function.
 */
#define MAX_LOCAL	40

/* Maximum number of evaluated nodes/loop.
 * If this is exceeded, current function is halted.
 */
#define MAX_COST	2000000

/*
 * This is the maximum array size allowed for one single array.
 */
#define MAX_ARRAY_SIZE 5000

/*
 * Reserve an extra memory area from malloc(), to free when we run out
 * of memory to get some warning and start Armageddon.
 * If this value is 0, no area will be reserved.
 */
#define RESERVED_SIZE		200000

/* Define the size of the shared string hash table.
 */

#define	HTABLE_SIZE 2203	 /* there is a table of some primes too */

/*
 * Object hash table size.
 * Define this like you did with the strings; probably set to about 1/4 of
 * the number of objects in a game, as the distribution of accesses to
 * objects is somewhat more uniform than that of strings.
 */

#define OTABLE_SIZE	107
#define OHASH_SIZE      701
#define FIND_FUNCTION_HASHSIZE 4711


/*
 * Define MAX_BYTE_TRANSFER to the number of bytes you allow to be read
 * and written with read_bytes and write_bytes (In non-batch mode)
 */

#define MAX_BYTE_TRANSFER 50000
   
/************************************************************************/
/*	END OF CONFIG -- DO NOT ALTER ANYTHING BELOW THIS LINE		*/
/************************************************************************/

#define YYDEBUG 1
#define YYOPT
#define MATH

#define HEARTBEAT_INTERVAL 2000000

#define NUM_CONSTS 10

#define MAX_EFUN_SOCKS 80

#define AVERAGE_COST 10
#define MAX_COST_PER_INSTR 1000
#define MAX_FUNC 2000

/*
 Enable run time debugging. It will use more time and space.
 When the flag is changed, be sure to recompile everything.
 Simply comment out this line if not wanted.
 If you change anything in the source, you are strongly encouraged to have
 DEBUG defined.
 If you will not change anything, you are still strongly encouraged to have
 it defined, as long as the game is not bug free.
*/

/* #define DEBUG */
/* #define WARN */
#if defined(MALLOC_smalloc) && defined(DEBUG)
#define MALLOC_DEBUG
#endif
/* #define OPCPROF */


/*
 * this is a future planned define for non-unix system
 */

/* #define NO_SOCKETS */

#define USE_SPRINTF

#define LPC_SUFFIX ".c"


/* Choose your stralloc */
#define STRALLOC_REFS
/* #define STRALLOC_GC */
/* #define STRALLOC_HYBRID */

#define STRALLOC_GC_TIME 60

#endif
