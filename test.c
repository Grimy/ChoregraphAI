// test.c - unit testing

#include "chore.h"

static Monster *skeleton;

static void test(Coords dir, Coords d, bool expected)
{
	// Setup the player
	g.current_beat = 0;
	player.pos = player.prev_pos = spawn;
	if (abs(dir.y ? d.y : d.x) == 2)
		player.prev_pos += dir;

	// Setup the skeleton
	TILE(skeleton->pos).monster = 0;
	skeleton->pos = spawn - d;
	TILE(skeleton->pos).monster = (u8) (skeleton - g.monsters);
	skeleton->prev_pos = spawn - d - dir;
	skeleton->delay = 0;

	// Do the test
	do_beat(' ');
	assert(skeleton->vertical == expected);
}

int main(void)
{
	static const Coords up = {0, -1}, down = {0, 1}, right = {1, 0}, left = {-1, 0};

	xml_parse(2, (char*[]) {"", "TEST.xml"});
	for (skeleton = g.monsters; skeleton->class != SKELETON_1; ++skeleton);

	test(up, (Coords) {-3, 1}, false);
	test(up, (Coords) {-2, 1}, false);
	test(up, (Coords) {-1, 1}, false);
	test(up, (Coords) { 1, 1}, true);
	test(up, (Coords) { 2, 1}, false);
	test(up, (Coords) { 3, 1}, false);

	test(down, (Coords) {-3, -1}, false);
	test(down, (Coords) {-2, -1}, false);
	test(down, (Coords) {-1, -1}, false);
	test(down, (Coords) { 1, -1}, true);
	test(down, (Coords) { 2, -1}, false);
	test(down, (Coords) { 3, -1}, false);
	
	test(right, (Coords) {-1, -3}, true);
	test(right, (Coords) {-1, -2}, true);
	test(right, (Coords) {-1, -1}, false);
	test(right, (Coords) {-1,  1}, false);
	test(right, (Coords) {-1,  2}, true);
	test(right, (Coords) {-1,  3}, true);

	test(left, (Coords) {1, -3}, true);
	test(left, (Coords) {1, -2}, true);
	test(left, (Coords) {1, -1}, true);
	test(left, (Coords) {1,  1}, true);
	test(left, (Coords) {1,  2}, true);
	test(left, (Coords) {1,  3}, true);


	test(up, (Coords) {-3, 2}, false);
	test(up, (Coords) {-2, 2}, false);
	test(up, (Coords) {-1, 2}, true);
	test(up, (Coords) { 1, 2}, true);
	test(up, (Coords) { 2, 2}, false); // true if x > 0
	test(up, (Coords) { 3, 2}, false);

	test(down, (Coords) {-3, -2}, false);
	test(down, (Coords) {-2, -2}, false);
	test(down, (Coords) {-1, -2}, true);
	test(down, (Coords) { 1, -2}, true);
	test(down, (Coords) { 2, -2}, false); // true if x > 0
	test(down, (Coords) { 3, -2}, false);

	test(right, (Coords) {-2, -3}, true);
	test(right, (Coords) {-2, -2}, false);
	test(right, (Coords) {-2, -1}, false);
	test(right, (Coords) {-2,  1}, false);
	test(right, (Coords) {-2,  2}, false);
	test(right, (Coords) {-2,  3}, true);

	test(left, (Coords) {2, -3}, true);
	test(left, (Coords) {2, -2}, true);
	test(left, (Coords) {2, -1}, false);
	test(left, (Coords) {2,  1}, false);
	test(left, (Coords) {2,  2}, true);
	test(left, (Coords) {2,  3}, true);

	printf("All tests OK\n");
}
