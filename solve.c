// solve.c - finds the optimal route for a level

#include <sys/time.h>
#include <pthread.h>

#include "main.c"

#define MAX_LENGTH    32
#define MAX_SCORE     64
#define MAX_BACKTRACK 6

typedef struct route {
	GameState state;
	struct route *next;           // Next element, if any
	i64 len;                      // Number of inputs
	u8 input[MAX_LENGTH];         // Inputs composing the route
} Route;

static Route* routes[MAX_SCORE];      // Priority queue of routes to explore
static i32 cur_score = MAX_SCORE - 1; // Index inside the queue
static i32 best_score;
static i64 best_len = MAX_LENGTH;     // Length of the shortest winning route
static GameState initial_state;
static u64 queued_routes;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Returns a human-readable representation of a route
static char* prettify_route(const Route *route)
{
	static const char* symbols[] = {"←", "↓", "→", "↑", "s", "z"};
	static char buf[3 * MAX_LENGTH + 1];

	sprintf(buf, "%lu ", route->len);
	for (i64 i = 0; i < route->len; ++i)
		strcat(buf, symbols[route->input[i]]);
	return buf;
}

static void shrink_queue()
{
	static i32 score = MAX_SCORE;
	Route *removed;

	pthread_mutex_lock(&mutex);
	removed = routes[--score];
	best_score = min(best_score, score - MAX_BACKTRACK);
	routes[score] = NULL;
	pthread_mutex_unlock(&mutex);

	while (removed != NULL) {
		--queued_routes;
		free(removed);
		removed = removed->next;
	}
}

// Appends the given route to the priority queue
static void add_to_queue(Route *route, i32 score)
{
	Route *new = malloc(sizeof(*new));
	*new = *route;
	new->state = g;

	pthread_mutex_lock(&mutex);
	new->next = routes[score];
	routes[score] = new;
	cur_score = min(cur_score, score);
	++queued_routes;
	pthread_mutex_unlock(&mutex);

	while (queued_routes > 65536)
		shrink_queue();
}

// Removes the best route from the priority queue and returns it
static Route* pop_queue()
{
	pthread_mutex_lock(&mutex);
	while (routes[cur_score] == NULL)
		if (++cur_score >= MAX_SCORE)
			exit(0);
	Route *result = routes[cur_score];
	routes[cur_score] = result->next;
	--queued_routes;
	pthread_mutex_unlock(&mutex);

	return result;
}

// lower is better
static i32 fitness_function() {
	if (player.hp <= 0)
		return 255;
	return g.current_beat
		- 2 * g.miniboss_killed
		- 2 * g.sarcophagus_killed
		+ L1(player.pos - stairs) * 2 / 5;
}

static void handle_victory(Route *route)
{
	u32 ok = 0;

	for (u32 i = 1; i <= 256 && (ok + 2) * 4 >= i; ++i) {
		g = initial_state;
		g.seed = i;
		for (i64 beat = 0; beat < route->len && player.hp > 0; ++beat)
			do_beat(route->input[beat]);
		ok += player_won();
	}

	if (ok < 64)
		return;
	best_len = min(best_len, route->len);
	printf("%s\t(%2.1f%%)\n", prettify_route(route), ok / 2.56);
}

// Starts with the given route, then tries all possible inputs
static void explore(Route *route)
{
	// Don’t explore routes that cannot beat the current best
	if (route->len >= best_len)
		return;

	++route->len;

	// Try adding each possible input at the end
	for (u8 i = 0; i < 6; ++i) {
		route->input[route->len - 1] = i;
		g = route->state;
		do_beat(i);
		i32 score = fitness_function();
		assert(score >= 0);
		best_score = min(best_score, score);
		if (player_won())
			handle_victory(route);
		else if (score < best_score + MAX_BACKTRACK)
			add_to_queue(route, (u16) score);
	}

	free(route);
}

static void* thread()
{
	for (;;)
		explore(pop_queue());
}

// `solve` entry point: solves the dungeon
int main(i32 argc, char **argv)
{
	xml_parse(argc, argv);
	do_beat(6);

	g.seed = 0;
	initial_state = g;
	best_score = fitness_function();

	Route *route = calloc(1, sizeof(*route));
	route->state = initial_state;
	explore(route);

	pthread_t children;
	for (i64 i = 0; i < JOBS; ++i)
		pthread_create(&children, NULL, thread, NULL);
	pthread_exit(NULL);
}
