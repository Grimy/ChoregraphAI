// route.c - finds the optimal route for a level

#include <sys/time.h>

#define MAX_LENGTH    32
#define MAX_SCORE     64

typedef struct route {
	struct route *next;           // Next element, if any
	u64 len;                      // Number of inputs
	u8 input[MAX_LENGTH];         // Inputs composing the route
} Route;

static i64 start_time;                // Unix start time (us)
static Route* routes[MAX_SCORE];      // Priority queue of routes to explore
static i32 cur_score = MAX_SCORE;     // Index inside the queue
static i32 worst_score;
static u64 best_len = MAX_LENGTH;     // Length of the shortest winning route
static GameState initial_state;

// Returns the current time in microseconds since the epoch
static i64 get_cur_time(void)
{
	static struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000L + tv.tv_usec;
}

// Prints the time since the program started to STDOUT
static char* timestamp()
{
	static char buf[16];
	i64 hundredths = (get_cur_time() - start_time) / 10000;
	i64 seconds = (hundredths / 100) % 60;
	i64 minutes = (hundredths / 100 / 60) % 60;
	sprintf(buf, "[%02ld:%02ld.%02ld]", minutes, seconds, hundredths % 100);
	return buf;
}

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
	return 5 * g.current_beat / 2
		+ L1(player.pos - stairs)
		- 7 * g.miniboss_killed
		- 6 * g.sarcophagus_killed
		- 2 * g.harpies_killed;
}

static double success_rate(Route *route)
{
	u32 ok = 0;

	for (u32 i = 0; i < 256; ++i) {
		g = initial_state;
		g.seed = i;
		for (u64 beat = 0; beat < route->len; ++beat)
			do_beat(route->input[beat]);
		ok += fitness_function() == 0;
	}

	return (double) ok / 256;
}

// Starts with the given route, then tries all possible inputs
static void explore(Route *route)
{
	// Don’t explore routes that cannot beat the current best
	if (route->len >= best_len)
		return;

	g = initial_state;
	for (u64 i = 0; i < route->len; ++i)
		do_beat(route->input[i]);
	GameState saved_state = g;
	++route->len;

	// Try adding each possible input at the end
	for (u8 i = 0; i < 6; i++) {
		route->input[route->len - 1] = i;
		g = saved_state;
		do_beat(i);
		i32 status = fitness_function();
		assert(status >= 0);
		if (status == 0) {
			double rate = success_rate(route);
			if (rate > .2) {
				best_len = route->len;
				printf("%s " GREEN "%s (%2.1f%%)\n" WHITE,
					timestamp(), prettify_route(route), rate * 100);
			}
		} else if (status < worst_score + 8) {
			add_to_queue(route, (u16) status);
		}
	}
}

// `solve` entry point: solves the dungeon
static void run()
{
	start_time = get_cur_time();
	initial_state = g;
	worst_score = fitness_function();
	for (Route *route = calloc(1, sizeof(*route)); route; route = pop_queue()) {
		explore(route);
		free(route);
	}
}
