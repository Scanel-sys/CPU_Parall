#include <iostream>
#include <stdio.h>
#include <fstream>
#include <string>
#include <vector>
#include <pthread.h>
#include <time.h>

struct TaskData
{
    int threads_number;
    int data_size;
};

struct JobInfo
{
    struct JobData volatile * first; 
    struct JobData volatile * last;
};

struct JobData
{
    volatile struct JobData * next;
    int l;
    int r;
};


int * data = NULL;
std::vector <volatile JobData *> trash{};
struct JobInfo merge_jobs;

pthread_mutex_t job_mutex;
pthread_mutex_t trash_mutex;
pthread_mutex_t gate_mutex;
pthread_cond_t thread_gate;

int err_info(const char* function);

struct TaskData read_task_data(std::string file_name);
int putout_data(struct TaskData task_data, unsigned int time_counter, int * sorted_data);
int putout_task_data(std::string file_name, struct TaskData task_data, int * sorted_data);
int putout_time_data(std::string file_name, unsigned int time_counter);

void init_synch_vars();
void destroy_synch_vars();

unsigned long to_ms(struct timespec* tm);

void wake_up_lazy_threads();
void wait_for_threads(pthread_t * threads, int threads_number);
void * thread_entry(void * param);
int create_threads(pthread_t * threads, int threads_number);

volatile struct JobData * pop_job();
void push_job(int l, int r);
struct JobData * joballoc();
void free_done_jobs();

void quick_sort_basic(int l, int r);
void qsort(int l, int r);


int main()
{
    timespec t_start, t_end;
    TaskData task_data = read_task_data("input.txt");
    if(task_data.data_size == -1)
        return -1;

    pthread_t * threads = (pthread_t *)malloc(sizeof(pthread_t) * task_data.threads_number);
    if(create_threads(threads, task_data.threads_number) == -1)
    {
        free(threads);
        return -1;
    }

    init_synch_vars();

    clock_gettime(CLOCK_REALTIME, &t_start);
    qsort(0, task_data.data_size - 1);
    wake_up_lazy_threads();
    wait_for_threads(threads, task_data.threads_number);
    clock_gettime(CLOCK_REALTIME, &t_end);

    destroy_synch_vars();
    free_done_jobs();
    free(threads);
    if(putout_data(task_data, to_ms(&t_end) - to_ms(&t_start), data) == -1)
        return -1;

    return 0;
}


int err_info(const char* function) 
{ 
    fprintf(stderr, "%s failed : errno : %d\n", function, errno);
    return -1;
}


struct TaskData read_task_data(std::string file_name)
{
    struct TaskData output;
    std::ifstream input_file;
    input_file.open(file_name);

    if(!input_file.is_open())
    {
        std::cout << "Seems like file doesnt exist\n";
        err_info("read_task_data");
        output.data_size = -1;
        output.threads_number = -1;
        return output;
    }

    input_file >> output.threads_number >> output.data_size;
    
    data = (int *)malloc(sizeof(int) * output.data_size);
    for(int i = 0; i < output.data_size; i++)
        input_file >> data[i];
    
    input_file.close();
    return output;
}

int putout_data(struct TaskData task_data, unsigned int time_counter, int * sorted_data)
{
    if(putout_task_data("output.txt", task_data, sorted_data) < 0 || putout_time_data("time.txt", time_counter) < 0)
        return err_info("putout_data");
    return 0;
}

int putout_task_data(std::string file_name, struct TaskData task_data, int * sorted_data)
{
    std::ofstream output_file;
    output_file.open(file_name);
    if(!output_file.is_open())
    {
        std::cout << "File exists already? Cant create\n";
        return err_info("putout_task_datan"); 
    }

    output_file << task_data.threads_number << '\n' << task_data.data_size << '\n';
    for(int i = 0; i < task_data.data_size; i++)
        output_file << sorted_data[i] << ' ';
    output_file << '\n';

    output_file.close();
    return 0;
}

int putout_time_data(std::string file_name, unsigned int time_counter)
{
    std::ofstream output_file;
    output_file.open(file_name);
    if(!output_file.is_open())
    {
        std::cout << "File exists already? Cant create\n";
        return err_info("putout_time_data\n"); 
    }
    output_file << time_counter;
    output_file.close();
    return 0;
}


void init_synch_vars()
{
    pthread_mutex_init(&job_mutex, 0);    
    pthread_mutex_init(&gate_mutex, 0);    
    pthread_mutex_init(&trash_mutex, 0);    
    pthread_cond_init(&thread_gate, 0);
}

void destroy_synch_vars()
{
    pthread_mutex_destroy(&job_mutex);
    pthread_mutex_destroy(&gate_mutex);
    pthread_mutex_destroy(&trash_mutex);
    pthread_cond_destroy(&thread_gate);
}


unsigned long to_ms(struct timespec* tm)
{
return ((unsigned long) tm->tv_sec * 1000 +
        (unsigned long) tm->tv_nsec / 1000000);
}


void wake_up_lazy_threads()
{
    pthread_mutex_lock(&gate_mutex);
    pthread_cond_broadcast(&thread_gate);
    pthread_mutex_unlock(&gate_mutex);
}

void wait_for_threads(pthread_t * threads, int threads_number)
{
    for(int i = 0; i < threads_number; i++)
        pthread_join(threads[i], 0);
}

void * thread_entry(void * param)
{
    pthread_mutex_lock(&gate_mutex);
    pthread_cond_wait(&thread_gate, &gate_mutex);
    pthread_mutex_unlock(&gate_mutex);

    volatile JobData *temp_job;
    while(1)
    {
        temp_job = pop_job();
        if(temp_job == NULL)
            break;

        qsort(temp_job->l, temp_job->r);

        pthread_mutex_lock(&trash_mutex);
        trash.push_back(temp_job);
        pthread_mutex_unlock(&trash_mutex);
    }
    return 0;
}

int create_threads(pthread_t * threads, int threads_number)
{
    for(int i = 0; i < threads_number; i++)
    {
        if(0 != pthread_create(&threads[i], 0, thread_entry, NULL))
            return err_info("create_thread");
    }
    return 0;
}


volatile struct JobData * pop_job()
{
    volatile JobData *output = NULL;
    pthread_mutex_lock(&job_mutex);
    if(merge_jobs.first != NULL)
    {
        output = merge_jobs.first;
        merge_jobs.first = merge_jobs.first->next;
    }
    pthread_mutex_unlock(&job_mutex);
    return output;
}

void push_job(int l, int r)
{
    struct JobData * new_job = joballoc();
    new_job->l = l;
    new_job->r = r;
    pthread_mutex_lock(&job_mutex);
    if(merge_jobs.first == NULL)
    {
        merge_jobs.first = new_job;
        merge_jobs.last = new_job;
        merge_jobs.first->next = NULL;
    }
    else
    {
        merge_jobs.last->next = new_job;
        merge_jobs.last = new_job;
        merge_jobs.last->next = NULL;
    }
    pthread_mutex_unlock(&job_mutex);
}

struct JobData * joballoc()
{
    struct JobData * task = (struct JobData *)malloc(sizeof(struct JobData));
    task->next = NULL;
    task->l = task->r = -1;
    return task;
}

void free_done_jobs()
{
    for(int i = 0; i < trash.size(); i++)
        free((void*)trash[i]);
}


void quick_sort_basic(int l, int r)
{
    int t_left_idx, t_right_idx, mid_val, t;
    t_left_idx = l;
    t_right_idx = r;
    mid_val = data[(l + r) / 2];
    do
    {
        while(data[t_left_idx] < mid_val)
            t_left_idx++;
        while(data[t_right_idx] > mid_val)
            t_right_idx--;
        if(t_left_idx <= t_right_idx)
        {
            t = data[t_left_idx];
            data[t_left_idx] = data[t_right_idx];
            data[t_right_idx] = t;
            t_left_idx++;
            t_right_idx--;
        }
    }while(t_left_idx < t_right_idx);

    if(t_right_idx > l)
        quick_sort_basic(l, t_right_idx);
    if(t_left_idx < r)
        quick_sort_basic(t_left_idx, r);
}


void qsort(int l, int r)
{
    int t_left_idx, t_right_idx, mid_val, t;
    t_left_idx = l;
    t_right_idx = r;
    mid_val = data[(l + r) / 2];
    do
    {
        while(data[t_left_idx] < mid_val)
            t_left_idx++;
        while(data[t_right_idx] > mid_val)
            t_right_idx--;
        if(t_left_idx <= t_right_idx)
        {
            t = data[t_left_idx];
            data[t_left_idx] = data[t_right_idx];
            data[t_right_idx] = t;
            t_left_idx++;
            t_right_idx--;
        }
    }while(t_left_idx < t_right_idx);

/*
    Two variants of recoursing job.
    First one is smaller code and doesnt check if array part is
    smaller than 1000.

    Second variant check it.
    If array is lesser than 1000 size theres no need
    to crop it into smaller job for another thread.
    Malloc / free / popping the job.
    These funcs wont be called.

*/

// 1st variant
/*
    if(t_right_idx > l)
    {
        push_job(l, t_right_idx);
        if(t_left_idx < r)
        {
            pthread_mutex_lock(&gate_mutex);
            pthread_cond_signal(&thread_gate);
            pthread_mutex_unlock(&gate_mutex);
            qsort(t_left_idx, r);
        }
    }
    else if(t_left_idx < r)
        qsort(t_left_idx, r);
*/

// 2nd variant
    if(l < t_right_idx)
    {
        if(t_right_idx - l > 999)
        {
            if(t_left_idx < r)
            {
                push_job(l, t_right_idx);
                pthread_mutex_lock(&gate_mutex);
                pthread_cond_signal(&thread_gate);
                pthread_mutex_unlock(&gate_mutex);
                qsort(t_left_idx, r);
            }
            else
                qsort(l, t_right_idx);
        }
        else
        {
            if(t_left_idx < r)
            {
                if(r - t_left_idx < 1000)
                {
                    quick_sort_basic(l, t_right_idx);
                    quick_sort_basic(t_left_idx, r);
                }
                else
                {
                    push_job(t_left_idx, r);
                    pthread_mutex_lock(&gate_mutex);
                    pthread_cond_signal(&thread_gate);
                    pthread_mutex_unlock(&gate_mutex);
                    qsort(l, t_right_idx);
                }
            }
            else
                quick_sort_basic(l, t_right_idx);
        }
    }
    else if(t_left_idx < r)
    {
        if(r - t_left_idx < 1000)
            quick_sort_basic(t_left_idx, r);
        else
            qsort(t_left_idx, r);
    }
}

