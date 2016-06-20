#include <errno.h>
#include <string.h>
#include <sys/time.h>

#define MAX_LENGTH 32

struct queue_entry {
	struct queue_entry *next;     // Next element, if any
	char input[MAX_LENGTH];       // Test input
	u32 len;                      // Input length
	u32 score;                    // Return code when run
};

static i64 queued_stems;              // Total number of queued testcases
static i64 complete_stems;
static i64 start_time;                // Unix start time (us)

static struct queue_entry *queue;     // Fuzzing queue (linked list)
static u32 best_len;

// Possible inputs
static const char inputs[5] = "efji<";

// Returns the current time in microseconds since the epoch
static i64 get_cur_time(void)
{
	static struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000L + tv.tv_usec;
}

static char* prettify_route(char input[MAX_LENGTH], u32 length) {
	static char buf[3*MAX_LENGTH + 1];
	sprintf(buf, "%d ", length);

	for (i64 i = 0; i < length; ++i) {
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

static void timestamp() {
	i64 hundredths = (get_cur_time() - start_time) / 10000;
	i64 seconds = (hundredths / 100) % 60;
	i64 minutes = (hundredths / 100 / 60) % 60;
	printf("[%02ld:%02ld.%02ld] ", minutes, seconds, hundredths % 100);
}

// Appends the current mutation to the queue. */
static void add_to_queue(char input[MAX_LENGTH], u32 len, u16 score)
{
	struct queue_entry *q = calloc(1, sizeof(*q));
	memcpy(q->input, input, len);
	q->len = len;
	q->score = score;

	struct queue_entry *prev = queue;
	while (prev->next && prev->next->score < score)
		prev = prev->next;
	q->next = prev->next;
	prev->next = q;
	queued_stems++;
}

// Forks to the simulator using the current input, saves the results
static void run_simulation(char input[32], u32 len)
{
	i32 status;
	i64 pid = fork();

	if (!pid) {
		fclose(stderr);
		srand(0);
		for (i64 i = 0; i < len; ++i)
			do_beat(input[i]);
		status = 2 * (i32) current_beat + L1(player.pos - stairs);
		if (!miniboss_defeated)
			status += 8 - harpies_defeated;
		exit(status);
	}
	if (pid < 0)
		FATAL("fork() failed: %s", strerror(errno));
	wait(&status);

	if (WIFSIGNALED(status)) {
		timestamp();
		printf(RED "%s\n" WHITE, prettify_route(input, len));
	} else if (WEXITSTATUS(status) == 0) {
		best_len = len;
		timestamp();
		printf(GREEN "%s\n" WHITE, prettify_route(input, len));
	} else if (WEXITSTATUS(status) != DEATH) {
		add_to_queue(input, len, (u16) (status));
	}
}

static void fuzz_one(struct queue_entry *q)
{
	static char input[MAX_LENGTH];

	complete_stems++;
	if (complete_stems % 1000 == 0) {
		timestamp();
		printf("%ld/%ld\n", complete_stems, queued_stems);
	}
	if (q->len >= best_len - 1)
		return;

	memcpy(input, q->input, q->len);

	// Try adding each possible input at the end
	for (u64 i = 0; i < LENGTH(inputs); i++) {
		input[q->len] = inputs[i];
		run_simulation(input, q->len + 1);
	}
}

static void init()
{
	start_time = get_cur_time();
	for (queue = calloc(1, sizeof(*queue)); queue; queue = queue->next)
		fuzz_one(queue);
}
