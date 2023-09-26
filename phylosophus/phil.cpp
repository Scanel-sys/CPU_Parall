#include <cstdlib>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <semaphore.h>
#include <unistd.h>

#include <sys/time.h>
#include <unistd.h>

#include <chrono>
#include <thread>

typedef struct min_max_forks_ {
	int min_fork, max_fork;
} min_max_forks;

int cur_id = 0;
unsigned long long status_time;
unsigned long long end_time = 0;
unsigned long long start_time;

int freq[5] = { 0,0,0,0,0 };
min_max_forks phil_forks[5];

pthread_mutex_t stdout_mutex;
pthread_mutex_t get_id_mutex;
pthread_mutex_t * forks;

unsigned long long int timeInMilliseconds(void);
void define_min_max_forks();
void printf_with_mutex(int currtime, int id, const char* action);
void start_eating(int id);
void change_status(bool* eating);
bool is_time_end();
void close_handles();
void release_forks(int max_fork, int min_fork);
void * thread_entry(void* params);

int main(int argc, char* argv[]) {
	int working_time = atoi(argv[1]);
	status_time = atoi(argv[2]);

	unsigned char i;
	define_min_max_forks();

	pthread_t * threads = (pthread_t *)malloc(sizeof(pthread_t) * 5 );
	forks = (pthread_mutex_t *)malloc(5 * sizeof(pthread_mutex_t));

	for (i = 0; i < 5; i++)
		pthread_mutex_init(&forks[i], 0);

	pthread_mutex_init(&stdout_mutex, 0);
	pthread_mutex_init(&get_id_mutex, 0);

	pthread_mutex_lock(&get_id_mutex);


	for (i = 0; i < 5; i++)
        pthread_create(&threads[i], 0, thread_entry, NULL);

	start_time = timeInMilliseconds();
	end_time = working_time + start_time;

	pthread_mutex_unlock(&get_id_mutex);

	for(i = 0; i < 5; i++)
        pthread_join(threads[i], 0);

	close_handles();
    free(threads);
	free(forks);
	for (unsigned int i = 0; i < 5; i++)
        printf("Phil %d changed his status %d times\n", i + 1, freq[i]);

    return 0;
}


void define_min_max_forks() {
	phil_forks[0].min_fork = 0;
	phil_forks[0].max_fork = 4;

	for (unsigned char i = 1; i < 5; i++) {
		phil_forks[i].min_fork = i - 1;
		phil_forks[i].max_fork = i;
	}
}

void printf_with_mutex(int cur_time, int id, const char* action) {
	pthread_mutex_lock(&stdout_mutex);
	freq[id - 1]++;
	printf("%d:%d:%s\n", cur_time, id, action);
	pthread_mutex_unlock(&stdout_mutex);
	return;
}

void start_eating(int id) {
	int max_fork = phil_forks[id].max_fork;
	int min_fork = phil_forks[id].min_fork;
	pthread_mutex_lock(&forks[max_fork]);
	pthread_mutex_lock(&forks[min_fork]);

	if (is_time_end()) {
		for (unsigned int i = 0; i < 5; i++)
            printf("Phil %d changed his status %d times\n", i + 1, freq[i]);
		release_forks(max_fork, min_fork);
		close_handles();
		exit(0);
	}
	return;
}

void change_status(bool* eating) {
	if (*eating == false)
        *eating = true;
	else
        *eating = false;
}

bool is_time_end() {
	if (timeInMilliseconds() < end_time)
        return false;
	else
        return true;
}

void close_handles()
{
	for (unsigned i = 0; i < 5; i++)
		pthread_mutex_destroy(&forks[i]);
    
	pthread_mutex_destroy(&stdout_mutex);
	pthread_mutex_destroy(&get_id_mutex);
}

void release_forks(int max_fork, int min_fork)
{
	pthread_mutex_unlock(&forks[max_fork]);
	pthread_mutex_unlock(&forks[min_fork]);
}

void * thread_entry(void* params)
{
	pthread_mutex_lock(&get_id_mutex);
	int id = cur_id++;
	pthread_mutex_unlock(&get_id_mutex);

	bool eating = false;

	while (!is_time_end()) {
		if (eating == false)
        {
			change_status(&eating);
			start_eating(id);
			printf_with_mutex(timeInMilliseconds() - start_time, id + 1, "T->E");
		}
		else
        {
			change_status(&eating);
			printf_with_mutex(timeInMilliseconds() - start_time, id + 1, "E->T");
			release_forks(phil_forks[id].max_fork, phil_forks[id].min_fork);
		}
		// std::this_thread::sleep_for(std::chrono::milliseconds(status_time));
		usleep(status_time * 1000);
	}
	if(eating == true)
	{
		change_status(&eating);
		printf_with_mutex(timeInMilliseconds() - start_time, id + 1, "E->T");
		release_forks(phil_forks[id].max_fork, phil_forks[id].min_fork);
	}
	return 0;
}

unsigned long long int timeInMilliseconds(void) {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (((unsigned long long int)tv.tv_sec) * 1000) + (tv.tv_usec / 1000);
}