// solve.c - finds the optimal route for a level

#include "chore.h"

#define MAX_LENGTH    24
#define MAX_BACKTRACK 8

// Don’t explore routes that exceed those thresholds
static i32 _Atomic cost_cutoff = MAX_LENGTH;

static i32 initial_score;
static GameState initial_state;

static _Atomic i32 simulated_beats;

static i32 cost_function()
{
	i32 damage = initial_state.monsters[1].hp - g.monsters[1].hp;
	i32 bombs_spent = initial_state.inventory[BOMBS] - g.inventory[BOMBS];
	return g.current_beat + damage + bombs_spent;
}

// Computes the score of the current game state
static i32 fitness_function()
{
	if (player.hp <= 0)
		return 255;
	double distance = L1(player.pos - stairs);
	return cost_function()
		+ (i32) (distance / 3)
		+ 3 * !g.miniboss_killed
		+ 2 * !g.sarcophagus_killed
		+ 6 * !g.inventory[JEWELED];
}

static void handle_victory()
{
	u32 ok = 0;
	i32 cost = cost_function();
	i64 length = g.length;
	u8 input[MAX_LENGTH];
	memcpy(input, g.input, MAX_LENGTH);

	for (u32 i = 1; i <= 256; ++i) {
		simulated_beats += length;
		g = initial_state;
		g.seed = i;
		for (i64 beat = 1; beat < length && player.hp > 0; ++beat)
			do_beat(input[beat]);
		ok += player_won();
		if ((ok + 2) * 4 < i)
			return;
	}

	cost -= ok == 256;
	if (cost < cost_cutoff)
		cost_cutoff = cost;

	// display the winning route
	static const char* symbols[] = {"←", "↓", "→", "↑", "s", "z", "X"};
	#pragma omp critical
	{
	printf("%d/%ld ", cost, length - 1);
	for (i64 i = 1; i < length; ++i)
		printf("%s", symbols[input[i]]);
	printf("\t(%2.1f%%)\n", ok / 2.56);
	}
}

// Starts with the given route, then tries all possible inputs
static void explore(GameState const *route, bool omp)
{
	simulated_beats += 6;
	i32 score_cutoff = initial_score + MAX_BACKTRACK - (simulated_beats >> 20);

	for (u8 i = 0; i < 6; ++i) {
		if (i || omp)
			g = *route;
		do_beat(i);
		i32 score = fitness_function();
		i32 cost = cost_function();
		assert(score >= 0);

		if (player_won()) {
			handle_victory();
		} else if (score < score_cutoff && cost < cost_cutoff) {
			GameState copy = g;
			if (score > score_cutoff - 4)
				explore(&copy, false);
			else
				#pragma omp task
				explore(&copy, true);
		}
	}
}

// `solve` entry point: solves the dungeon
int main(i32 argc, char **argv)
{
	xml_parse(argc, argv);
	do_beat(6);
	initial_state = g;
	initial_score = fitness_function();

	#pragma omp parallel
	#pragma omp single nowait
	explore(&initial_state, true);

	fprintf(stderr, "%d\n", simulated_beats);
}
