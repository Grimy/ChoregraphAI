// base.h - commonly useful typedefs/macros

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#define ARRAY_SIZE(arr) ((i64) (sizeof(arr) / sizeof((arr)[0])))

#define min(x, y) ({ \
	__typeof(x) _min1 = (x), _min2 = (y); \
	(__typeof(x)) (_min1 < _min2 ? _min1 : _min2); })

#define max(x, y) ({ \
	__typeof(x) _max1 = (x), _max2 = (y); \
	(__typeof(x)) (_max1 > _max2 ? _max1 : _max2); })

#define clamp(val, lo, hi) min((__typeof(val)) max(val, lo), hi)

#define swap(a, b) do { __typeof(a) _swap = (a); (a) = (b); (b) = _swap; } while (0)

#define abs(x) ({ __typeof(x) _abs = (x); _abs < 0 ? -_abs : _abs; })

#define SIGN(x) (((x) > 0) - ((x) < 0))

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
