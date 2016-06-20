#include <string.h>
#include <sys/time.h>

#define MAX_LENGTH        32

struct queue_entry {
	struct queue_entry *next;         // Next element, if any
	u8 input[MAX_LENGTH];             // Test input
	u64 depth;                        // Path depth
	u32 len;                          // Input length
	u32 score;                        // Return code when run
};

static u8 out_buf[MAX_LENGTH];
static u32 len = 0;

static s64 queued_stems;              // Total number of queued testcases
static s64 complete_stems;
static s64 total_execs;               // Total execve() calls
static s64 start_time;                // Unix start time (us)

static struct queue_entry *queue;     // Fuzzing queue (linked list)
static u32 best_len;

// Possible inputs
static const u8 inputs[5] = "efji<";

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
static char* DQ(u8 input[32], u32 length) {
	static char buf[3*MAX_LENGTH + 1];
	strcpy(buf, DI(length));
	strcat(buf, " ");

	for (s64 i = 0; i < length; ++i) {
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

// Appends the current mutation to the queue. */
static void add_to_queue(u16 score)
{
	struct queue_entry *q = calloc(1, sizeof(*q));
	memcpy(q->input, out_buf, len);
	q->len = len;
	q->score = score;

	struct queue_entry *prev = queue;
	while (prev->next && prev->next->score < score)
		prev = prev->next;
	q->next = prev->next;
	prev->next = q;
	queued_stems++;
}

static void timestamp() {
	s64 hundredths = (get_cur_time() - start_time) / 10000;
	s64 seconds = (hundredths / 100) % 60;
	s64 minutes = (hundredths / 100 / 60) % 60;
	printf("[%02ld:%02ld.%02ld] ", minutes, seconds, hundredths % 100);
}

// Forks to the simulator using the current input, saves the results
static void run_simulation()
{
	s32 status;
	s64 pid = fork();
	total_execs++;

	if (!pid) {
		fclose(stderr);
		srand(0);
		for (;;)
			do_beat();
	}
	if (pid < 0)
		error("fork() failed");
	wait(&status);

	if (WIFSIGNALED(status)) {
		printf(RED "%.*s\n" WHITE, len, out_buf);
	} else if (WEXITSTATUS(status) == 0) {
		best_len = len;
		timestamp();
		printf(GREEN "%s\n" WHITE, DQ(out_buf, len));
	} else if (WEXITSTATUS(status) != 255) {
		add_to_queue((u16) (status));
	}
}

static void fuzz_one()
{
	complete_stems++;
	if (complete_stems % 1000 == 0) {
		timestamp();
		printf("%ld/%ld\n", complete_stems, queued_stems);
	}
	if (queue->len >= best_len - 1)
		return;

	len = queue->len;
	memcpy(out_buf, queue->input, len);

	// Try adding each possible input at the end
	for (u64 i = 0; i < LENGTH(inputs); i++) {
		out_buf[len++] = inputs[i];
		run_simulation();
		--len;
	}
}

// Pulls an input from the current mutation
static char player_input()
{
	return current_beat < len ? (char) out_buf[current_beat] : 0;
}

static void init()
{
	start_time = get_cur_time();
	for (queue = calloc(1, sizeof(*queue)); queue; queue = queue->next)
		fuzz_one();
}
