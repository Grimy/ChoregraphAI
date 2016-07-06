// solve.c - finds the optimal route for a level

#include <sys/time.h>

#include "main.c"

#define MAX_LENGTH    32
#define MAX_SCORE     64
#define MAX_BACKTRACK 3

typedef struct route {
	struct route *next;           // Next element, if any
	u64 len;                      // Number of inputs
	u8 input[MAX_LENGTH];         // Inputs composing the route
} Route;

static Route* routes[MAX_SCORE];      // Priority queue of routes to explore
static i32 explored_routes;
static i32 cur_score = MAX_SCORE;     // Index inside the queue
static i32 worst_score;
static u64 best_len = MAX_LENGTH;     // Length of the shortest winning route
static GameState initial_state;

// Returns a human-readable representation of a route
static char* prettify_route(const Route *route)
{
	static const char* symbols[] = {"←", "↓", "→", "↑", "s", "z"};
	static char buf[3 * MAX_LENGTH + 1];

	sprintf(buf, "%lu ", route->len);
	for (u64 i = 0; i < route->len; ++i)
		strcat(buf, symbols[route->input[i]]);
	return buf;
}

// Appends the given route to the priority queue
static void add_to_queue(Route *route, i32 score)
{
	Route *new = malloc(sizeof(*new));
	*new = *route;
	new->next = routes[score];
	routes[score] = new;
	cur_score = min(cur_score, score);
}

// Removes the best route from the priority queue and returns it
static Route* pop_queue()
{
	while (routes[cur_score] == NULL)
		if (++cur_score >= MAX_SCORE)
			return NULL;
	Route *result = routes[cur_score];
	routes[cur_score] = result->next;
	return result;
}

// lower is better
static i32 fitness_function() {
	if (player.hp <= 0)
		return 255;
	if (player_won())
		return 0;
	return g.current_beat
			- 2 * g.miniboss_killed
			- 2 * g.sarcophagus_killed
			- g.harpies_killed
			+ L1(player.pos - stairs) * 2 / 5;
}

static void winning_route(Route *route)
{
	u32 ok = 0;

	for (u32 i = 1; i <= 256 && (ok + 2) * 4 >= i; ++i) {
		g = initial_state;
		g.seed = i;
		for (u64 beat = 0; beat < route->len; ++beat)
			do_beat(route->input[beat]);
		ok += fitness_function() == 0;
	}

	if (ok < 64)
		return;
	best_len = route->len;
	printf("[%6d] " GREEN "%s (%2.1f%%)\n" WHITE,
		explored_routes, prettify_route(route), ok / 2.56);
}

// Starts with the given route, then tries all possible inputs
static void explore(Route *route)
{
	// Don’t explore routes that cannot beat the current best
	if (route->len >= best_len)
		return;
	++explored_routes;

	g = initial_state;
	for (u64 i = 0; i < route->len; ++i)
		do_beat(route->input[i]);
	GameState saved_state = g;
	++route->len;

	// Try adding each possible input at the end
	for (u8 i = 0; i < 6; ++i) {
		route->input[route->len - 1] = i;
		g = saved_state;
		do_beat(i);
		i32 status = fitness_function();
		assert(status >= 0);
		if (status == 0)
			winning_route(route);
		else if (status < worst_score + MAX_BACKTRACK)
			add_to_queue(route, (u16) status);
	}
}

// `solve` entry point: solves the dungeon
int main(i32 argc, char **argv)
{
	xml_parse(argc, argv);
	g.seed = 0;
	initial_state = g;
	worst_score = fitness_function();
	for (Route *route = calloc(1, sizeof(*route)); route; route = pop_queue()) {
		explore(route);
		free(route);
	}
}
