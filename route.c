// route.c - finds the optimal route for a level

#include <errno.h>
#include <sys/time.h>

#define MAX_LENGTH    32
#define MAX_SCORE     64
#define MAX_BACKTRACK 4

typedef struct route {
	struct route *next;           // Next element, if any
	u8 input[MAX_LENGTH];         // Inputs composing the route
	u64 len;                      // Number of inputs
} Route;

static i64 queued_routes = 1;         // Total number of interesting routes
static i64 start_time;                // Unix start time (us)

static Route* routes[MAX_SCORE];      // Priority queue of routes to explore
static u32 cur_score = MAX_SCORE;     // Index inside the queue
static u64 best_len = MAX_LENGTH;     // Length of the shortest winning route

// Returns the current time in microseconds since the epoch
static i64 get_cur_time(void)
{
	static struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000L + tv.tv_usec;
}

// Prints the time since the program started to STDOUT
static void timestamp()
{
	i64 hundredths = (get_cur_time() - start_time) / 10000;
	i64 seconds = (hundredths / 100) % 60;
	i64 minutes = (hundredths / 100 / 60) % 60;
	printf("[%02ld:%02ld.%02ld] ", minutes, seconds, hundredths % 100);
}

// Returns a human-readable representation of a route
static char* prettify_route(Route *route)
{
	static const char* symbols[] = {"←", "↓", "→", "↑", "s", "z"};
	static char buf[3 * MAX_LENGTH + 1];

	sprintf(buf, "%lu ", route->len);
	for (u64 i = 0; i < route->len; ++i)
		strcat(buf, symbols[route->input[i]]);
	return buf;
}

// Appends the given route to the priority queue
static void add_to_queue(Route *route, u16 score)
{
	Route *new = malloc(sizeof(*new));
	*new = *route;
	new->next = routes[score];
	routes[score] = new;

	cur_score = MIN(cur_score, score);
	queued_routes++;
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
	return 2 * current_beat + L1(player.pos - stairs)
		+ 4 * !miniboss_defeated - harpies_defeated;
}
	
// Forks to the simulator, tries the given route, saves the results
static void run_simulation(Route *route)
{
	i32 status;
	i64 pid = fork();

	if (!pid) {
		srand(0);
		for (u64 i = 0; i < route->len; ++i)
			do_beat(route->input[i]);
		assert(fitness_function() > 0);
		exit(fitness_function());
	}

	if (pid < 0)
		FATAL("fork() failed: %s", strerror(errno));
	if (wait(&status) != pid)
		FATAL("wait() failed: %s", strerror(errno));

	if (WIFSIGNALED(status)) {
		timestamp();
		printf(RED "%s\n" WHITE, prettify_route(route));
	} else if (WEXITSTATUS(status) == 0) {
		best_len = route->len;
		timestamp();
		printf(GREEN "%s\n" WHITE, prettify_route(route));
	} else if (WEXITSTATUS(status) < fitness_function() + MAX_BACKTRACK) {
		add_to_queue(route, WEXITSTATUS(status));
	}
}

// Starts with the given route, then tries all possible inputs
static void explore(Route *route)
{
	static i64 explored_routes;
	explored_routes++;
	if (explored_routes % 1000 == 0) {
		timestamp();
		printf("%ld/%ld: %u\n", explored_routes, queued_routes, cur_score);
	}

	// Don’t explore routes that cannot beat the current best
	if (route->len >= best_len)
		return;

	// Try adding each possible input at the end
	++route->len;
	for (u8 i = 0; i < 6; i++) {
		route->input[route->len - 1] = i;
		run_simulation(route);
	}
}

// `solve` entry point: solves the dungeon
static void run()
{
	start_time = get_cur_time();
	for (Route *route = calloc(1, sizeof(*route)); route; route = pop_queue()) {
		explore(route);
		free(route);
	}
	timestamp();
	printf("%ld/%ld: %u\n", queued_routes, queued_routes, cur_score);
}
