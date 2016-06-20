#include <stdint.h>

// Convenient name for integer types
typedef enum {false, true} bool;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

// Some basic math macros
#define LENGTH(array) ((i64) (sizeof(array) / sizeof(*(array))))
#define MIN(a, b)     ((a) > (b) ? (b) : (a))
#define MAX(a, b)     ((a) > (b) ? (a) : (b))
#define SIGN(x)       (((x) > 0) - ((x) < 0))
#define ABS(x)        ((x) < 0 ? -(x) : (x))
#define RNG(limit)    ((u64) rand() % (limit))

// Terminal ANSI codes
#define WHITE  "\033[m"
#define RED    "\033[31m"
#define YELLOW "\033[33m"
#define BLUE   "\033[34m"
#define PINK   "\033[35m"
#define BLACK  "\033[90m"
#define ORANGE "\033[91m"
#define GREEN  "\033[92m"
#define CYAN   "\033[94m"
#define PURPLE "\033[95m"

#define TERM_HOME     "\x1b[H"
#define TERM_CLEAR    TERM_HOME "\x1b[2J"

// Die verbosely
#define FATAL(message, ...) do { \
	fprintf(stderr, RED message WHITE "\n", __VA_ARGS__); \
	exit(255); } while (0);
