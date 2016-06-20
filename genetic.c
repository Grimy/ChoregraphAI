#include <errno.h>
#include <string.h>
#include <sys/time.h>

#define MAX_LENGTH 32
#define MAX_SCORE  50

typedef struct route {
	struct route *next;           // Next element, if any
	char input[MAX_LENGTH];       // Test input
	u64 len;                      // Input length
} Route;

static i64 queued_stems;              // Total number of queued testcases
static i64 complete_stems;
static i64 start_time;                // Unix start time (us)

static Route* routes[MAX_SCORE + 1];  // Priority queue of routes to explore
static u64 best_len;                  // Length of the shortest winning route
static u32 cur_score = MAX_SCORE;

// Returns the current time in microseconds since the epoch
static i64 get_cur_time(void)
{
	static struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000L + tv.tv_usec;
}

// Returns a human-readable representation of a route
static char* prettify_route(char *input, u64 length) {
	static char buf[3*MAX_LENGTH + 1];
	sprintf(buf, "%lu ", length);

	for (u64 i = 0; i < length; ++i) {
		switch (input[i]) {
		case 'e': strcat(buf, "←"); break;
		case 'f': strcat(buf, "↓"); break;
		case 'j': strcat(buf, "↑"); break;
		case 'i': strcat(buf, "→"); break;
		case '<': strcat(buf, "s"); break;
		default: abort();
		}
	}

	return buf;
}

// Prints the time since the program started to STDOUT
static void timestamp() {
	i64 hundredths = (get_cur_time() - start_time) / 10000;
	i64 seconds = (hundredths / 100) % 60;
	i64 minutes = (hundredths / 100 / 60) % 60;
	printf("[%02ld:%02ld.%02ld] ", minutes, seconds, hundredths % 100);
}

// Appends the given route to the queue.
static void add_to_queue(char *input, u64 len, u16 score)
{
	Route *q = calloc(1, sizeof(*q));
	memcpy(q->input, input, len);
	q->len = len;
	q->next = routes[score];
	routes[score] = q;
	cur_score = MIN(cur_score, score);
	queued_stems++;
}

// Removes the best route from the priority queue and returns it
static Route* pop_queue() {
	while (routes[cur_score] == NULL)
		++cur_score;
	assert(cur_score <= MAX_SCORE);
	Route *result = routes[cur_score];
	routes[cur_score] = result->next;
	return result;
}

// Forks to the simulator, tries the given route, saves the results
static void run_simulation(char *input, u64 len)
{
	i32 status;
	i64 pid = fork();

	if (!pid) {
		fclose(stderr);
		srand(0);
		for (u64 i = 0; i < len; ++i)
			do_beat(input[i]);
		status = 2 * (i32) current_beat + L1(player.pos - stairs);
		if (!miniboss_defeated)
			status += 8 - harpies_defeated;
		exit(status);
	}

	if (pid < 0)
		FATAL("fork() failed: %s", strerror(errno));
	if (wait(&status) != pid)
		FATAL("wait() failed: %s", strerror(errno));

	if (WIFSIGNALED(status)) {
		timestamp();
		printf(RED "%s\n" WHITE, prettify_route(input, len));
	} else if (WEXITSTATUS(status) == 0) {
		best_len = len;
		timestamp();
		printf(GREEN "%s\n" WHITE, prettify_route(input, len));
	} else if (WEXITSTATUS(status) <= MAX_SCORE) {
		add_to_queue(input, len, (u16) WEXITSTATUS(status));
	}
}

// Start with the given route, then try all possible inputs
static void explore(Route *q)
{
	complete_stems++;
	if (complete_stems % 1000 == 0) {
		timestamp();
		printf("%ld/%ld: %u\n", complete_stems, queued_stems, cur_score);
	}
	if (q->len >= best_len - 1)
		return;

	// Try adding each possible input at the end
	for (u64 i = 0; i < 5; i++) {
		q->input[q->len] = "efji<"[i];
		run_simulation(q->input, q->len + 1);
	}
}

static void init()
{
	start_time = get_cur_time();
	for (Route *route = calloc(1, sizeof(*route)); route; route = pop_queue()) {
		explore(route);
		free(route);
	}
}
