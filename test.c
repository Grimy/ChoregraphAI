static Monster skeleton = {.class = SKELETON_1, .hp = 1};

static void test(Coords dir, Coords d, bool expected)
{
	TILE(skeleton.pos).monster = NULL;
	player.pos = player.prev_pos = spawn;
	if (ABS(dir.y ? d.y : d.x) == 2)
		player.prev_pos += dir;
	skeleton.pos = spawn - d;
	skeleton.prev_pos = spawn - d - dir;
	assert(player.pos.x - skeleton.pos.x == d.x);
	assert(player.pos.y - skeleton.pos.y == d.y);
	basic_seek(&skeleton, d);
	assert(skeleton.vertical == expected);
}

static void run()
{
	static const Coords up = {0, -1}, down = {0, 1}, right = {1, 0}, left = {-1, 0};
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
	do_beat('t');
}
