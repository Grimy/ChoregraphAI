// solve.c - finds the optimal route for a level

#include <sys/time.h>
#include <pthread.h>

#include "main.c"

#define MAX_LENGTH    32
#define MAX_BACKTRACK 8
#define BUF_SIZE      (1 << 17)

typedef struct route {
	GameState state;
	struct route *next;    // Next element, if any
	i64 score;             // How good the route looks (lower is better)
	i64 length;            // Number of inputs
	u8 input[MAX_LENGTH];  // Inputs composing the route
} Route;

// Memory pool of routes to explore
static Route routes[BUF_SIZE];

// Priority queue of pointers into the pool, ordered by fitness
static Route* queue[MAX_LENGTH];
static i32 cur_score = MAX_LENGTH;

// Don’t explore routes that exceed those thresholds
static volatile i64 length_cutoff = MAX_LENGTH;
static i32 score_cutoff = MAX_LENGTH;

static GameState initial_state;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static u64 explored_routes;
static u64 queued_routes;

// Returns a human-readable representation of a route
static char* prettify_route(const Route *route)
{
	static const char* symbols[] = {"←", "↓", "→", "↑", "s", "z", "X"};
	static char buf[3 * MAX_LENGTH + 1];

	sprintf(buf, "%lu ", route->length);
	for (i64 i = 0; i < route->length; ++i)
		strcat(buf, symbols[route->input[i]]);
	return buf;
}

// Appends the given route to the priority queue
static void add_to_queue(Route *route, i32 score)
{
	static u32 i;
	Route *new;

	// Loop over the memory pool until we find a free slot
	// To avoid infinite loops, we decrease the cutoff every other loop
	pthread_mutex_lock(&mutex);
	++queued_routes;
	do {
		new = &routes[++i % BUF_SIZE];
		score_cutoff -= i % (2 * BUF_SIZE) == 0;
	} while (new->length && new->score < score_cutoff);
	pthread_mutex_unlock(&mutex);

	new->length = route->length;
	memcpy(new->input, route->input, MAX_LENGTH);
	new->score = score;
	new->state = g;

	pthread_mutex_lock(&mutex);
	new->next = queue[score];
	queue[score] = new;
	cur_score = min(cur_score, score);
	pthread_mutex_unlock(&mutex);
}

// Removes the best route from the priority queue and returns it
static Route* pop_queue()
{
	Route *result = NULL;

	pthread_mutex_lock(&mutex);
	while (queue[cur_score] == NULL && cur_score < score_cutoff)
		++cur_score;
	if (cur_score < score_cutoff) {
		result = queue[cur_score];
		queue[cur_score] = result->next;
	}
	pthread_mutex_unlock(&mutex);

	return result;
}

// Computes the score of a route, based on the current game state
static i32 fitness_function() {
	if (player.hp <= 0)
		return 255;
	double distance = L1(player.pos - stairs);
	return g.current_beat + 1*(g.current_beat >= 4)
		- 3 * g.miniboss_killed
		- 2 * g.sarcophagus_killed
		- (g.player_damage - 1)
		+ (i32) (distance / 2.3);
}

static void handle_victory(Route *route)
{
	u32 ok = 0;

	for (u32 i = 1; i <= 256; ++i) {
		g = initial_state;
		g.seed = i;
		for (i64 beat = 0; beat < route->length && player.hp > 0; ++beat)
			do_beat(route->input[beat]);
		ok += player_won();
		if ((ok + 2) * 4 < i)
			return;
	}

	length_cutoff = min(length_cutoff, route->length);
	printf("%s\t(%2.1f%%)\n", prettify_route(route), ok / 2.56);
}

// Starts with the given route, then tries all possible inputs
static void explore(Route *route)
{
	if (route->length >= length_cutoff)
		return;
	assert(route->score < score_cutoff);

	++route->length;
	++explored_routes;

	// Try adding each possible input at the end
	for (u8 i = 0; i < 6; ++i) {
		route->input[route->length - 1] = i;
		g = route->state;
		do_beat(i);
		i32 score = fitness_function();
		assert(score >= 0);
		score_cutoff = min(score_cutoff, fitness_function() + MAX_BACKTRACK);

		if (player_won())
			handle_victory(route);
		else if (score < score_cutoff)
			add_to_queue(route, (u16) score);
	}

	// Mark the route as done
	route->length = 0;
}

static void* run()
{
	Route *route;
	while ((route = pop_queue()))
		explore(route);
	return NULL;
}

// `solve` entry point: solves the dungeon
int main(i32 argc, char **argv)
{
	xml_parse(argc, argv);
	do_beat(6);

	initial_state = g;
	routes[0].state = g;
	explore(&routes[0]);

	pthread_t child;
	for (i64 i = 1; i < JOBS; ++i)
		pthread_create(&child, NULL, run, NULL);
	run();

	fprintf(stderr, "%lu/%lu\n", explored_routes, queued_routes);
}
