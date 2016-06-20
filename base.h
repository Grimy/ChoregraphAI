#include <stdint.h>
#include <errno.h>

// Convenient name for integer types
typedef enum {false, true} bool;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

// Some basic math macros
#define LENGTH(array) ((long) (sizeof(array) / sizeof(*(array))))
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

#define TERM_JUMP(x)  "\x1b[K\x1b[" #x "G"
#define TERM_HOME     "\x1b[H"
#define TERM_CLEAR    TERM_HOME "\x1b[2J"

// Die verbosely
#define FATAL(...) do { \
	printf(WHITE ORANGE "\n[-] " __VA_ARGS__); \
	printf(WHITE "\nAt: " CYAN "%s(), %s:%u\n\n" WHITE, __FUNCTION__, __FILE__, __LINE__); \
	exit(1); \
} while (0)

#define PFATAL(...) FATAL(__VA_ARGS__, strerror(errno))

// Error-checking versions of open(), read() and write() that call FATAL()
#define ck_open(pathname, flags, mode) __extension__ ({ \
		s32 _res = open(pathname, flags, mode); \
		if (_res < 0) PFATAL("Unable to open %s: %s", pathname); \
		_res; })

#define ck_read(fd, buf, len, fn) do { \
	s64 _len = (len); \
	s64 _res = read(fd, buf, (u64) _len); \
	if (_res < 0) PFATAL("Error reading %s: %s", fn); \
	else if (_res != _len) FATAL("Short read from %s", fn); \
} while (0)

#define ck_write(fd, buf, len, fn) do { \
	s64 _len = (len); \
	s64 _res = write(fd, buf, (u64) _len); \
	if (_res < 0) PFATAL("Error writing %s: %s", fn); \
	else if (_res != _len) FATAL("Short write to %s", fn); \
} while (0)
