// solve.c - finds the optimal route for a level

#include "chore.h"

#define WORK_FACTOR 3

// Don’t explore routes that exceed those thresholds
static i32 _Atomic best_cost;

static GameState initial_state;
static i32 initial_distance;

static _Atomic i32 simulated_beats;

// Returns the cost (in beats) of the current route.
// This is the number of beats it takes, plus the value of spent resources.
static i32 cost_function()
{
	return g.current_beat
		+ (initial_state.monsters[1].hp - g.monsters[1].hp)
		+ (initial_state.inventory[BOMBS] - g.inventory[BOMBS])
		+ 4 * (initial_state.inventory[JEWELED] - g.inventory[JEWELED]);
}

// Estimates the number of beats it will take to clear the level.
static i32 distance_function()
{
	return (i32) ((L1(player.pos - stairs) + 2) / 3) + 2 * g.locking_enemies;
}

// When a winning route is found with RNG disabled, estimate its probability
// of success by running it on a battery of different RNGs.
// If it’s consistent enough, display it, and stop exploring costlier routes.
static void handle_victory()
{
	u32 ok = 0;
	i32 cost = cost_function();
	i32 length = g.current_beat;
	u8 input[ARRAY_SIZE(g.input)];
	memcpy(input, g.input, sizeof(g.input));

	for (u32 i = 1; i <= 256; ++i) {
		simulated_beats += length;
		g = initial_state;
		g.seed = i;
		for (i64 beat = 0; beat < length && player.hp > 0; ++beat)
			ok += do_beat(input[beat]);
		if ((ok + 2) * 4 < i)
			return;
	}

	cost += ok != 256;
	best_cost = min(cost, best_cost);
	printf("%2d/%-2d%4.0f%%\t%s\n", cost, length, ok / 2.56, input);
}

// Recursively try all possible inputs, starting at the given point.
static void explore(GameState const *route, bool omp)
{
	static const u8 symbols[6] = {'e', 'f', 'i', 'j', '<', ' '};

	simulated_beats += ARRAY_SIZE(symbols);
	// printf("%s\n", g.input);

	for (u8 i = 0; i < ARRAY_SIZE(symbols); ++i) {
		if (i || omp)
			g = *route;

		if (do_beat(symbols[i])) {
			handle_victory();
			continue;
		}

		i32 cost = cost_function();
		if (cost >= best_cost || !player.hp)
			continue;

		i32 distance = WORK_FACTOR + initial_distance - distance_function();
		double best_speed = initial_distance / (double) best_cost;
		double speed = distance / (double) cost;

		if (speed >= best_speed) {
			GameState copy = g;
			#pragma omp task
			explore(&copy, true);
		}
	}
}

// `solve` entry point: solves the dungeon
int main(i32 argc, char **argv)
{
	xml_parse(argc, argv);
	initial_state = g;
	initial_distance = distance_function();
	best_cost = initial_distance + 2 + WORK_FACTOR;

	printf("Goal: %d\n", best_cost);

	#pragma omp parallel
	#pragma omp single nowait
	explore(&initial_state, true);

	fprintf(stderr, "%d\n", simulated_beats);
}
