#include <iostream>
#include <stdio.h>
#include <fstream>
#include <string>
#include <windows.h>
#include <vector>

struct JobData
{
    struct JobData volatile * next; 
    int n;
    int k;
};

struct JobInfo
{
    struct JobData volatile * first; 
    struct JobData volatile * last;
};

struct TaskData
{
    unsigned int threads_number;
    unsigned int number;
};


CRITICAL_SECTION decomp_section;
CRITICAL_SECTION queue_section;
volatile unsigned int decompositions = 0;
HANDLE sema;
volatile struct JobInfo jobs;


struct JobData pop_job();
void push_job(struct JobData job);
struct JobData * joballoc();
int jobs_size();

void increase_decomps();

struct TaskData get_task_data(std::string file_name);
int putout_task_data(std::string file_name, struct TaskData data);
int putout_time_data(std::string file_name, unsigned int time_counter);

int file_err(const char* function);

void close_threads(HANDLE * threads, unsigned int threads_number);

DWORD WINAPI thread_entry(void * param);

void count_millisec(HANDLE &thread_to_count);


int main()
{
    unsigned int time_counter = 0;
    jobs.first = jobs.last = NULL;    
    DWORD started, finished;
    
    struct TaskData task_data = get_task_data("input.txt");
    HANDLE * threads = (HANDLE *)malloc(sizeof(HANDLE) * task_data.threads_number);

    InitializeCriticalSection(&decomp_section);
    InitializeCriticalSection(&queue_section);
    sema = CreateSemaphore(0, 0, task_data.threads_number, 0);

    for(int i = 0; i < task_data.threads_number; i++)
    {
        threads[i] = CreateThread(0, 0, thread_entry, NULL, 0, NULL);
        if(threads[i] == 0)
        {
            printf("CreateThread failed. GetLastError(): %u\n", GetLastError());
            return -1;
        }
    }

    started = GetTickCount();
    struct JobData first_job;
    first_job.n = task_data.number;
    first_job.k = task_data.number;
    push_job(first_job);
    
    ReleaseSemaphore(sema, 1, NULL);
    WaitForMultipleObjects(task_data.threads_number, threads, TRUE, INFINITE);
    finished = GetTickCount();
    
    printf("Threads finished\n%u.%u seconds\n", (finished - started) / 1000, (finished - started) % 1000);
    
    DeleteCriticalSection(&decomp_section);
    DeleteCriticalSection(&queue_section);
    CloseHandle(sema);
    close_threads(threads, task_data.threads_number);
    free(threads);

    if(putout_task_data("output.txt", task_data) < 0 || putout_time_data("time.txt", finished - started) < 0)
        return -1;

    return 0;
}


struct JobData pop_job()
{
    JobData output;
    output.k = output.n = -1;
    EnterCriticalSection(&queue_section);
    if(jobs.first != NULL)
    {
        output.n = jobs.first->n;
        output.k = jobs.first->k;
        output.next = jobs.first->next;
        free((void *)jobs.first);
        jobs.first = output.next;
    }
    LeaveCriticalSection(&queue_section);
    return output;
}

void push_job(struct JobData job)
{
    struct JobData * new_job = joballoc();
    new_job->k = job.k;
    new_job->n = job.n;
    EnterCriticalSection(&queue_section);
    if(jobs.first == NULL)
    {
        jobs.first = new_job;
        jobs.last = new_job;
        jobs.first->next = NULL;
    }
    else
    {
        jobs.last->next = new_job;
        jobs.last = new_job;
        jobs.last->next = NULL;
    }
    LeaveCriticalSection(&queue_section);
}

int jobs_size()
{
    if(jobs.first == NULL)
        return 0;
    int i = 1;
    volatile struct JobData * temp_job = jobs.first;
    while(temp_job->next != NULL)
    {
        temp_job = temp_job->next;
        i++;
    }
    return i;
}

struct JobData * joballoc()
{
    struct JobData * task = (struct JobData *)malloc(sizeof(struct JobData));
    task->next = NULL;
    task->n = 0;
    task->k = 0;
    return task;
}


void increase_decomps()
{
    EnterCriticalSection(&decomp_section);
    decompositions++;
    LeaveCriticalSection(&decomp_section);
}


struct TaskData get_task_data(std::string file_name)
{
    struct TaskData output;
    std::ifstream input_file;
    input_file.open(file_name);

    if(!input_file.is_open())
    {
        std::cout << "Seems like file doesnt exist\n";
        exit; 
    }

    input_file >> output.threads_number >> output.number;
    input_file.close();
    return output;
}

int putout_task_data(std::string file_name, struct TaskData data)
{
    std::ofstream output_file;
    output_file.open(file_name);

    if(!output_file.is_open())
    {
        std::cout << "File exists already? Cant create\n";
        return file_err("putout_task_datan"); 
    }
    output_file << data.threads_number << '\n' << data.number << '\n' << decompositions;

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
        return file_err("putout_time_data\n"); 
    }
    output_file << time_counter;
    output_file.close();
    return 0;
}


int file_err(const char* function) 
{ 
    fprintf(stderr, "%s: file r/w error : %d\n", function, GetLastError()); 
    return -1; 
}

void close_threads(HANDLE * threads, unsigned int threads_number)
{
    for(int i = 0; i < threads_number; i++)
        CloseHandle(threads[i]);
}

DWORD WINAPI thread_entry(void * param)
{    
    WaitForSingleObject(sema, INFINITE);

    JobData temp_job = pop_job();
    while(temp_job.k >= 0 && temp_job.n >= 0)
    {
        JobData new_job;
        while(temp_job.k > 0)
        {
            if(temp_job.k <= temp_job.n)
            {    
                new_job.n = temp_job.n - temp_job.k;
                new_job.k = temp_job.k;
                push_job(new_job);
                ReleaseSemaphore(sema, 1, NULL);   
                temp_job.k--;
            }
            else
            {
                temp_job.k = temp_job.n;
            }
        }
        if(temp_job.k == 0 && temp_job.n == 0)
            increase_decomps();

        temp_job = pop_job();
    }
    return 0;
}
