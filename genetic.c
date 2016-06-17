#define CRASH             255
#define RESEED_RNG        10000
#define CPU_AFF           0

// Fuzzing parameters
#define HAVOC_MAX         2048
#define MAX_FILE          32
#define MAX_BACKTRACK     2
#define HAVOC_STACKING    (queue_cycle + 1)

#include <sched.h>
#include <string.h>
#include <sys/file.h>
#include <sys/time.h>

struct queue_entry {
	struct queue_entry *next;         // Next element, if any
	u8 input[MAX_FILE];               // Test input
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

static u8 out_buf[MAX_FILE];
static u32 len;

static s32 crashes_fd;                // Persistent fd for crashes
static s32 routes_fd;                 // Persistent fd for routes
static s32 urandom_fd;                // Persistent fd for /dev/urandom
static s32 fsrv_ctl_fd;               // Fork server control pipe (write)
static s32 fsrv_st_fd;                // Fork server status pipe (read)
static s32 forksrv_pid;               // PID of the fork server

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

/* Possible inputs */
static u8 inputs[5] = "efji<";

/* Get unix time in microseconds */
static s64 get_cur_time(void)
{
	static struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000L + tv.tv_usec;
}

/* Set CPU affinity (on systems that support it). */
static void set_cpu_affinity(u32 cpu_id)
{
	cpu_set_t c;

	CPU_ZERO(&c);
	CPU_SET(cpu_id, &c);
	if (sched_setaffinity(0, sizeof(c), &c))
		PFATAL("sched_setaffinity failed: %s");
}

/* Generate a random number (from 0 to limit - 1). This may
   have slight bias. */
static inline u64 UR(u64 limit)
{
	static s32 rand_cnt;
	assert(limit > 0);

	if (!rand_cnt--) {
		u32 seed[2];
		ck_read(urandom_fd, &seed, sizeof(seed), "/dev/urandom");
		srandom(seed[0]);
		rand_cnt = (RESEED_RNG / 2) + (seed[1] % RESEED_RNG);
	}

	return (u64) random() % limit;
}

/* Describe integer. The value returned should be five characters or less. */
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

/* Describe queue entry. */
static char* DQ(struct queue_entry *q) {
	static char buf[3*MAX_FILE + 1];
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

static char* DR(s64 part, s64 total)
{
	static char buf[12];
	strcpy(buf, DI(part));
	strcat(buf, "/");
	strcat(buf, DI(total));
	return buf;
}

/* Describe time delta. Returns one static buffer, 34 chars of less. */
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

/* Append new test case to the queue. */
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

/* Execute target application, monitoring for timeouts. Return status
   information. */
static u8 run_target()
{
	s64 pid = fork();
	s32 status;

	if (!pid)
		for (;;)
			do_beat();
	if (pid < 0)
		PFATAL("fork() failed: %s");
	wait(&status);

	/* Report outcome to caller. */
	total_execs++;
	return WIFSIGNALED(status) ? CRASH : WEXITSTATUS(status);
}

/* A spiffy retro stats screen! This is called every stats_update_freq
   execve() calls, plus in several other circumstances. */
static void show_stats(void)
{
	static s64 last_us;
	static double avg_exec, last_execs;
#define LEFT  "│ %12s: " WHITE "%s" BLACK TERM_JUMP(29)
#define RIGHT "│ %10s: %s%s" BLACK TERM_JUMP(54) "│\n"
#define LINE  "│ %11s: " WHITE "%s" BLACK TERM_JUMP(54) "│\n"

	/* Don’t update at more than 60FPS */
	s64 cur_us = get_cur_time();
	if (cur_us < last_us + 1000000 / 60)
		return;

	/* Calculate smoothed exec speed stats. */
	double cur_avg = (double) (total_execs - last_execs) * 1000000 / (cur_us - MAX(last_us, start_time));
	if (cur_avg < 1000000)
		avg_exec = avg_exec * .99 + cur_avg * (avg_exec != 0 ? .01 : 1);
	last_us = cur_us;
	last_execs = total_execs;

	/* Now, for the visuals... */
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
	printf(RIGHT, "crashes", total_crashes ? RED : WHITE, DI(total_crashes));
	printf("├─" GREEN " stage progress " BLACK "──────────┴────────────────────────┤\n");
	printf(LINE,  "best route",   DQ(best));
	printf(LINE,  "now fuzzing",  DQ(queue_cur));
	printf(LINE,  "total execs",  DI(total_execs));
	printf(LINE,  "exec/sec",     DI((s64) avg_exec));
	printf("└────────────────────────────────────────────────────┘\n");

	/* Hallelujah! */
	fflush(0);
}

/* Write a modified test case, run program, process results. Handle
   error conditions, returning 1 if it's time to bail out. This is
   a helper function for fuzz_one(). */
static void common_fuzz_stuff()
{
	// Check for duplicates
	if (exists_in_tree())
		return;

	u8 status = run_target();

	if (status == CRASH) {
		total_crashes++;
		last_crash_time = get_cur_time();
		write(crashes_fd, out_buf, len);
		write(crashes_fd, "\n", 1);
	}
	else if (status == 0 && len <= best->score) {
		total_routes++;
		last_route_time = get_cur_time();
		write(routes_fd, out_buf, len);
		write(routes_fd, "\n", 1);
	}
	u8 game_over = !(status >> 7);
	status &= 0x7F;

	add_to_tree(game_over);
	add_to_queue(status + (u8) len);
}

static void mutate()
{
	u64 del_from, insert_at;

	switch (len < 2 ? 2 : UR(3)) {
		/* Update a single byte. */
		case 0:
			out_buf[UR(len)] = inputs[UR(LENGTH(inputs))];
			break;

		/* Delete a single byte. */
		case 1:
			del_from = UR(len);
			memmove(out_buf + del_from, out_buf + del_from + 1, len - del_from - 1);
			len--;
			break;

		/* Insert a single byte. */
		case 2:
			assert(len < MAX_FILE);
			insert_at = UR(len + 1);
			memmove(out_buf + insert_at + 1, out_buf + insert_at, len - insert_at);
			out_buf[insert_at] = inputs[UR(LENGTH(inputs))];
			len++;
			break;
	}
}

/* Take the current entry from the queue, fuzz it for a while. */
static void fuzz_one()
{
	if (queue_cur->was_fuzzed && pending_stems && UR(20))
		return;

	if (!queue_cur->favored && pending_favored && UR(20))
		return;

	len = queue_cur->len;
	memcpy(out_buf, queue_cur->input, len);

	/* Inserting 8-bit integers. */
	for (u64 i = 0; i < LENGTH(inputs); i++) {
		out_buf[len] = inputs[i];
		++len;
		common_fuzz_stuff();
		--len;
	}

	/* We essentially just do several thousand runs (depending on perf_score)
	   where we take the input file and make random stacked tweaks. */
	u64 havoc_passes = HAVOC_MAX / (1 + queue_cur->score - best->score);

	for (u64 pass = 0; pass < havoc_passes; ++pass) {
		s64 mutation_count = 2 << UR((u64) HAVOC_STACKING);

		for (s64 i = 0; i < mutation_count; ++i)
			mutate();

		common_fuzz_stuff();

		/* Restore out_buf to its original state. */
		len = queue_cur->len;
		memcpy(out_buf, queue_cur->input, len);
	}

	/* Update pending_stems count if we made it through the calibration
	   cycle and have not seen this entry before. */
	if (!queue_cur->was_fuzzed) {
		queue_cur->was_fuzzed = 1;
		pending_stems--;
		pending_favored -= queue_cur->favored;
	}
}

/* Prepare persistent fds. */
static void setup_fds(void)
{
	crashes_fd = ck_open("crashes", O_WRONLY | O_CREAT | O_TRUNC, 0600);
	routes_fd  = ck_open("routes",  O_WRONLY | O_CREAT | O_TRUNC, 0600);
	urandom_fd = ck_open("/dev/urandom", O_RDONLY, 0);
}

static char display_prompt() {
	return current_beat < len ? (char) out_buf[current_beat] : 0;
}
