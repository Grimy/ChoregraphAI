#include "base.h"

#define INDEX(x, y) (y * (y + 1) / 2 + x - 1)

typedef i8 Coords __attribute__((ext_vector_type(2)));

static u64 los(double x, double y)
{
	i64 cx = (i64) (x + .5);
	i64 cy = (i64) (y + .5);
	u64 path = 0;
	if (cx > cy)
		return 0xFFFFFFFFFFFFFFFF;
	while (cy) {
		double err_x = abs((cx - 1 - x) * y - (cy - y) * x);
		double err_y = abs((cx - x) * y - (cy - 1 - y) * x);
		if (cx < 10 && cy <= 10)
			path |= 1lu << INDEX(cx, cy);
		if (cx > 0 && abs(err_x - err_y) < .001)
			path |= 1lu << INDEX(cx - 1, cy);
		if (!cx || err_y < err_x + .001)
			--cy;
		else
			--cx;
	}
	return path;
}

// _mm256_and_pd
static void codegen(i8 x, i8 y)
{
	if (INDEX(x, y) > 1)
		printf("if (!(walls & %#018lx) || !(walls & %#018lx) || !(walls & %#018lx)"
			"|| !(walls & %#018lx) || !(walls & %#018lx))\n",
			los(x - .55, y - .55),
			los(x + .55, y + .55),
			los(x - .55, y + .55),
			los(x + .55, y - .55),
			los(x, y));
	printf("tile->revealed = true;\n");
	printf("walls |= (u64) (tile->class == WALL) << %d;\n", INDEX(x, y));
	printf("tile += x;\n");
}

int main(void)
{
	i8 y, x;
	for (y = 1; y <= 10; ++y) {
		u64 mask = 0;
		for (x = 0; x <= min(y, 9); ++x) {
			codegen(x, y);
			mask |= 1lu << INDEX(x, y);
		}
		printf("if (walls >= %#018lx) return;\n", mask);
		printf("if (!(walls & %#018lx))\ntile->revealed = true;\n",
			los(x - .55, y + .55));
		printf("tile += y - %d * x;\n", y + 1);
	}
}
