// solve.c - finds the optimal route for a level

#include "chore.h"

#define MAX_LENGTH    24
#define MAX_BACKTRACK 6

// Don’t explore routes that exceed those thresholds
static i32 _Atomic length_cutoff = MAX_LENGTH - 1;
static i32 score_cutoff = MAX_LENGTH;

static GameState initial_state;

static _Atomic u64 explored_routes;

// Computes the score of the current game state
static i32 fitness_function() {
	if (player.hp <= 0)
		return 255;
	double distance = L1(player.pos - stairs);
	return 8 + g.current_beat
		+ (i32) (distance / 2.3)
		- 3 * g.miniboss_killed
		- 2 * g.sarcophagus_killed
		- g.player_damage
		- player.hp
		- g.player_bombs;
}

static void handle_victory()
{
	u32 ok = 0;
	GameState copy = g;

	for (u32 i = 1; i <= 256; ++i) {
		g = initial_state;
		g.seed = i;
		for (i64 beat = 0; beat < copy.length && player.hp > 0; ++beat)
			do_beat(copy.input[beat]);
		ok += player_won();
		if ((ok + 2) * 4 < i)
			return;
	}

	if (copy.length < length_cutoff)
		length_cutoff = copy.length;

	// display the winning route
	static const char* symbols[] = {"←", "↓", "→", "↑", "s", "z", "X"};
	printf("%ld/%d/%d ", copy.length - 1,
			initial_state.monsters[1].hp - copy.monsters[1].hp,
			initial_state.player_bombs - copy.player_bombs);
	for (i64 i = 1; i < copy.length; ++i)
		printf("%s", symbols[copy.input[i]]);
	printf("\t(%2.1f%%)\n", ok / 2.56);
}

// Starts with the given route, then tries all possible inputs
static void explore(GameState *route)
{
	++explored_routes;

	for (u8 i = 0; i < 6; ++i) {
		g = *route;
		do_beat(i);
		i32 score = fitness_function();
		assert(score >= 0);

		if (player_won()) {
			handle_victory();
		} else if (score < score_cutoff && g.length < length_cutoff) {
			GameState copy = g;
			#pragma omp task
			explore(&copy);
		}
	}
}

// `solve` entry point: solves the dungeon
int main(i32 argc, char **argv)
{
	xml_parse(argc, argv);
	do_beat(6);
	score_cutoff = fitness_function() + MAX_BACKTRACK;
	initial_state = g;

	#pragma omp parallel
	#pragma omp single
	explore(&initial_state);

	fprintf(stderr, "%lu\n", explored_routes);
}
