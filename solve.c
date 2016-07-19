// solve.c - finds the optimal route for a level

#include <sys/time.h>
#include <pthread.h>

#include "main.c"

#define MAX_LENGTH    24
#define MAX_BACKTRACK 8

typedef struct route {
	GameState state;
	struct route *next;    // Next element, if ny
	i64 length;            // Number of inputs
	u8 input[MAX_LENGTH];  // Inputs composing the route
} Route;

// Don’t explore routes that exceed those thresholds
static i64 _Atomic length_cutoff = MAX_LENGTH - 1;
static i32 _Atomic score_cutoff = MAX_LENGTH;
static Route pool[6];

static GameState initial_state;

static _Atomic u64 explored_routes;
static _Atomic u64 spawned_threads;

// Returns a human-readable representation of a route
static void pretty_print(const Route *route)
{
	static const char* symbols[] = {"←", "↓", "→", "↑", "s", "z", "X"};

	printf("%ld/%d/%d ", route->length + 1, 3 - player.hp, 3 - g.player_bombs);
	for (i64 i = 0; i <= route->length; ++i)
		printf("%s", symbols[route->input[i]]);
}

// Computes the score of a route, based on the current game state
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

static void handle_victory(Route *route)
{
	u32 ok = 0;

	for (u32 i = 1; i <= 256; ++i) {
		g = initial_state;
		g.seed = i;
		for (i64 beat = 0; beat <= route->length && player.hp > 0; ++beat)
			do_beat(route->input[beat]);
		ok += player_won();
		if ((ok + 2) * 4 < i)
			return;
	}

	if (route->length < length_cutoff)
		length_cutoff = route->length;
	if (fitness_function() + MAX_BACKTRACK < score_cutoff)
		score_cutoff = fitness_function() + MAX_BACKTRACK;

	pretty_print(route);
	printf("\t(%2.1f%%)\n", ok / 2.56);
}

static void* explore(void *);

static void maybe_start_thread(Route *route, u8 input)
{
	if (spawned_threads < ARRAY_SIZE(pool)) {
		Route *new = &pool[spawned_threads++];
		new->state = g;
		new->length = route->length + 1;
		memcpy(new->input, route->input, (u64) route->length);
		new->input[route->length] = input;

		pthread_t child;
		pthread_create(&child, NULL, explore, new);
		return;
	}
	Route new = {.state = g, .length = route->length + 1};
	memcpy(new.input, route->input, (u64) route->length);
	new.input[route->length] = input;
	explore(&new);
}

// Starts with the given route, then tries all possible inputs
static void* explore(void *arg)
{
	Route *route = (Route*) arg;

	if (route->length > length_cutoff)
		return NULL;

	++explored_routes;

	// Try adding each possible input at the end
	for (u8 i = 0; i < 6; ++i) {
		route->input[route->length] = i;

		g = route->state;
		do_beat(i);
		i32 score = fitness_function();
		assert(score >= 0);

		if (player_won())
			handle_victory(route);
		else if (score < score_cutoff)
			maybe_start_thread(route, i);
	}

	return NULL;
}

// `solve` entry point: solves the dungeon
int main(i32 argc, char **argv)
{
	xml_parse(argc, argv);
	do_beat(6);
	score_cutoff = fitness_function() + MAX_BACKTRACK;

	initial_state = g;
	Route route = {.state = g};
	explore(&route);

	fprintf(stderr, "%lu\n", explored_routes);
}
