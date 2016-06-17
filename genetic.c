// Fuzzing parameters
#define MAX_LENGTH        32
#define MAX_PASSES        2048
#define MAX_BACKTRACK     2
#define STACKING          (queue_cycle + 1)

#include <sched.h>
#include <string.h>
#include <sys/file.h>
#include <sys/time.h>

struct queue_entry {
	struct queue_entry *next;         // Next element, if any
	u8 input[MAX_LENGTH];             // Test input
	u64 depth;                        // Path depth
	u32 len;                          // Input length
	u8 was_fuzzed;                    // Had any fuzzing done yet?
	u8 favored;                       // Currently favored?
	u16 score;                        // Return code when run
};

struct tree_node {
	struct tree_node *child[5];
	bool was_run;
	bool game_over;
};

enum { UPDATE, DELETE, INSERT };

static u8 out_buf[MAX_LENGTH];
static u32 len;

static s32 crashes_fd;                // Persistent fd for crashes
static s32 routes_fd;                 // Persistent fd for routes
static s32 fsrv_ctl_fd;               // Fork server control pipe (write)
static s32 fsrv_st_fd;                // Fork server status pipe (read)

static s64 queues_stems;              // Total number of queued testcases
static s64 queued_favored;            // Paths deemed favorable
static s64 pending_stems;             // Queued but not done yet
static s64 pending_favored;           // Pending favored paths
static s64 total_routes;              // Total number of crashes
static s64 total_crashes;             // Total number of hangs
static s64 total_execs;               // Total execve() calls
static s64 queue_cycle;               // Queue round counter
static s64 start_time;                // Unix start time (us)
static s64 last_path_time;            // Time for most recent path (us)
static s64 last_fav_time;             // Time for most recent favorite (us)
static s64 last_crash_time;           // Time for most recent crash (us)
static s64 last_route_time;           // Time for most recent route (us)

static struct queue_entry *queue;     // Fuzzing queue (linked list)
static struct queue_entry *queue_cur; // Current offset within the queue
static struct queue_entry *queue_top; // Top of the list
static struct queue_entry *best;      // Best testcase score so far

static struct tree_node *tree;

static s64 input_index(u8 input) {
	return input == 'e' ? 0 :
		   input == 'f' ? 1 :
		   input == 'i' ? 2 :
		   input == 'j' ? 3 :
		   input == '<' ? 4 : -1;
}

static bool exists_in_tree() {
	struct tree_node *t = tree;
	for (s64 i = 0; i < len; ++i) {
		if (t == NULL)
			return false;
		if (t->game_over)
			return true;
		t = t->child[input_index(out_buf[i])];
	}
	return t && t->was_run;
}

static void add_to_tree(bool game_over) {
	if (tree == NULL)
		tree = calloc(1, sizeof(*tree));
	struct tree_node *t = tree;
	for (s64 i = 0; i < len; ++i) {
		s64 index = input_index(out_buf[i]);
		if (t->child[index] == NULL)
			t->child[index] = calloc(1, sizeof(*tree));
		t = t->child[index];
	}
	t->was_run = true;
	t->game_over = game_over;
}

// Possible inputs
static u8 inputs[5] = "efji<";

// Returns the current time in microseconds since the epoch
static s64 get_cur_time(void)
{
	static struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000L + tv.tv_usec;
}

// Describe integer. The value returned is always five characters or less.
static char* DI(s64 val)
{
	static char buf[6];
	if (val < 100000)
		sprintf(buf, "%lu", val);
	else if (val < 10000000)
		sprintf(buf, "%luk", val >> 10);
	else
		strcpy(buf, "a lot");
	return buf;
}

// Describe queue entry.
static char* DQ(struct queue_entry *q) {
	static char buf[3*MAX_LENGTH + 1];
	char *p = stpcpy(buf, DI(q->score));
	*p++ = ' ';

	for (s64 i = 0; i < q->len; ++i) {
		switch (q->input[i]) {
		case 'e': p = stpcpy(p, "←"); break;
		case 'f': p = stpcpy(p, "↓"); break;
		case 'j': p = stpcpy(p, "↑"); break;
		case 'i': p = stpcpy(p, "→"); break;
		case '<': p = stpcpy(p, "s"); break;
		default: abort();
		}
	}
	return buf;
}

// Describe ratio.
static char* DR(s64 part, s64 total)
{
	static char buf[12];
	strcpy(buf, DI(part));
	strcat(buf, "/");
	strcat(buf, DI(total));
	return buf;
}

// Describe time difference.
static char* DTD(s64 cur_us, s64 event_us)
{
	static char tmp[24];

	if (!event_us)
		return "   N/A";

	s64 delta = (cur_us - event_us) / 10000;
	u32 t_m = (delta / 100 / 60) % 60;
	u32 t_s = (delta / 100) % 60;
	u32 t_c = (delta) % 100;

	sprintf(tmp, "%02u:%02u.%02u", t_m, t_s, t_c);
	return tmp;
}

// Appends the current mutation to the queue. */
static void add_to_queue(u16 score)
{
	if (best && score > best->score + MAX_BACKTRACK)
		return;

	struct queue_entry* q = calloc(1, sizeof(*q));

	memcpy(q->input, out_buf, len);
	q->len = len;
	q->score = score;

	if (!best || score <= best->score) {
		best = q;
		q->favored = 1;
		queued_favored++;
		pending_favored++;
		last_fav_time = get_cur_time();
	}

	if (queue_top)
		queue_top->next = q;
	else
		queue = q;
	queue_top = q;

	queues_stems++;
	pending_stems++;
	last_path_time = get_cur_time();
}

// Forks to the simulator using the current input, saves the results
static void run_simulation()
{
	// Check for duplicates
	if (exists_in_tree())
		return;

	s32 status;
	s64 pid = fork();
	total_execs++;

	if (!pid) {
		fclose(stderr);
		for (;;)
			do_beat();
	}
	if (pid < 0)
		error("fork() failed");
	wait(&status);

	if (WIFSIGNALED(status)) {
		total_crashes++;
		last_crash_time = get_cur_time();
		write(crashes_fd, out_buf, len);
		write(crashes_fd, "\n", 1);
		return;
	}

	u8 game_over = !(WEXITSTATUS(status) >> 7);
	status = WEXITSTATUS(status) & 0x7F;

	if (status == 0 && len <= best->score) {
		total_routes++;
		last_route_time = get_cur_time();
		write(routes_fd, out_buf, len);
		write(routes_fd, "\n", 1);
	}

	add_to_tree(game_over);
	add_to_queue((u16) (status) + (u16) len);
}

// Updates the stats screen.
static void show_stats(void)
{
	static s64 last_us;
	static double avg_exec, last_execs;
#define LEFT  "│ %12s: " WHITE "%s" BLACK TERM_JUMP(29)
#define RIGHT "│ %10s: %s%s" BLACK TERM_JUMP(54) "│\n"
#define LINE  "│ %11s: " WHITE "%s" BLACK TERM_JUMP(54) "│\n"

	// Don’t update at more than 60FPS
	s64 cur_us = get_cur_time();
	if (cur_us < last_us + 1000000 / 60)
		return;

	// Compute the average execution speed
	double cur_avg = (double) (total_execs - last_execs) * 1000000 / (cur_us - MAX(last_us, start_time));
	if (cur_avg < 1000000)
		avg_exec = avg_exec * .9 + cur_avg * (avg_exec != 0 ? .1 : 1);
	last_us = cur_us;
	last_execs = total_execs;

	printf(TERM_HOME YELLOW "\n                       cotton fuzzer\n\n" BLACK);
	printf("┌─" GREEN " process timing " BLACK "──────────┬──");
	printf(GREEN " overall results " BLACK "─────┐\n");
	printf(LEFT,  "run time",     DTD(cur_us, start_time));
	printf(RIGHT, "cycles",       queue_cycle ? YELLOW : WHITE, DI(queue_cycle));
	printf(LEFT,  "latest stem",  DTD(cur_us, last_path_time));
	printf(RIGHT, "stems",        WHITE, DR(pending_stems, queues_stems));
	printf(LEFT,  "latest fav",   DTD(cur_us, last_fav_time));
	printf(RIGHT, "favorites",    WHITE, DR(pending_favored, queued_favored));
	printf(LEFT,  "latest route", DTD(cur_us, last_route_time));
	printf(RIGHT, "routes",       total_routes ? GREEN : WHITE, DI(total_routes));
	printf(LEFT,  "latest crash", DTD(cur_us, last_crash_time));
	printf(RIGHT, "crashes",      total_crashes ? RED : WHITE, DI(total_crashes));
	printf("├─" GREEN " stage progress " BLACK "──────────┴────────────────────────┤\n");
	printf(LINE,  "best route",   DQ(best));
	printf(LINE,  "now fuzzing",  DQ(queue_cur));
	printf(LINE,  "total execs",  DI(total_execs));
	printf(LINE,  "exec/sec",     DI((s64) avg_exec));
	printf("└────────────────────────────────────────────────────┘\n");
}

// Applies a single mutation to the current stem
static void mutate()
{
	u64 del_from, insert_at;

	switch (len < 2 ? INSERT : RNG(3)) {
	case UPDATE:
		assert(LENGTH(inputs) == 5);
		assert(len > 1);
		out_buf[RNG(len)] = inputs[RNG(LENGTH(inputs))];
		break;

	case DELETE:
		assert(LENGTH(inputs) == 5);
		assert(len > 1);
		del_from = RNG(len);
		memmove(out_buf + del_from, out_buf + del_from + 1, len - del_from - 1);
		len--;
		break;

	case INSERT:
		assert(LENGTH(inputs) == 5);
		assert(len < MAX_LENGTH);
		insert_at = RNG(len + 1);
		memmove(out_buf + insert_at + 1, out_buf + insert_at, len - insert_at);
		out_buf[insert_at] = inputs[RNG(LENGTH(inputs))];
		len++;
		break;
	}
}

// Take the current entry from the queue, mutate it randomly
static void fuzz_one()
{
	if (queue_cur->was_fuzzed && pending_stems && RNG(20))
		return;

	if (!queue_cur->favored && pending_favored && RNG(20))
		return;

	len = queue_cur->len;
	memcpy(out_buf, queue_cur->input, len);

	// Try adding each possible input at the end
	for (u64 i = 0; i < LENGTH(inputs); i++) {
		out_buf[len] = inputs[i];
		++len;
		run_simulation();
		--len;
	}

	// Randomly mutate the stem
	for (u64 passes = MAX_PASSES / (1 + queue_cur->score - best->score); passes; --passes) {
		for (s64 mutations = 2 << RNG((u64) STACKING); mutations; --mutations)
			mutate();
		run_simulation();
		len = queue_cur->len;
		memcpy(out_buf, queue_cur->input, len);
	}

	// Mark the stem as done (no longer pending)
	if (!queue_cur->was_fuzzed) {
		queue_cur->was_fuzzed = 1;
		pending_stems--;
		pending_favored -= queue_cur->favored;
	}
}

// Pulls an input from the current mutation
static char player_input()
{
	return current_beat < len ? (char) out_buf[current_beat] : 0;
}

static void __attribute__((noreturn)) init() {
	add_to_queue(200);

	crashes_fd = open("crashes", O_WRONLY | O_CREAT | O_TRUNC, 0600);
	routes_fd  = open("routes",  O_WRONLY | O_CREAT | O_TRUNC, 0600);
	start_time = get_cur_time();
	queue_cur = queue;

	for (;;) {
		fuzz_one();
		show_stats();
		queue_cur = queue_cur->next;
		if (!queue_cur) {
			queue_cycle++;
			queue_cur = queue;
		}
	}
}
