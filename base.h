// base.h - commonly useful typedefs/macros

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Convenient names for integer types

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

#define SWAP(a, b) do { __typeof(a) _swap = (a); (a) = (b); (b) = _swap; } while (0)

#define SIGN(x) (((x) > 0) - ((x) < 0))

#define abs(x) ((x) > 0 ? (x) : -(x))
#define min(x, y) ({ __typeof(x) X = (x), Y = (y); (__typeof(x)) (X < Y ? X : Y); })
#define max(x, y) ({ __typeof(x) X = (x), Y = (y); (__typeof(x)) (X > Y ? X : Y); })

#define streq(a, b) (!strcmp((a), (b)))

// Terminal ANSI codes
#define CLEAR   "\033[m"
#define REVERSE "\033[7m"
#define RED     "\033[31m"
#define BROWN   "\033[33m"
#define BLUE    "\033[34m"
#define PINK    "\033[35m"
#define BLACK   "\033[90m"
#define ORANGE  "\033[91m"
#define GREEN   "\033[92m"
#define YELLOW  "\033[93m"
#define CYAN    "\033[94m"
#define PURPLE  "\033[95m"
#define WHITE   "\033[97m"

// Die verbosely
#define FATAL(message, ...) do { \
	fprintf(stderr, RED message WHITE "\n", __VA_ARGS__); \
	exit(255); } while (0);
