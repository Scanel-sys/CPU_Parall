#include <iostream>
#include <stdio.h>
#include <fstream>
#include <string>
#include <windows.h>
#include <vector>


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
    int * data;
    int size;
};

struct MergeJob
{
    int jobs_amount;
    int * data1;
    int size1;
    int * data2;
    int size2;
};

std::vector <int> data{};
std::vector <int *> trash{};
std::vector <JobData> msort_jobs{};
struct JobInfo merge_jobs;

HANDLE job_mutex;
HANDLE thread_gate;

int err_info(const char* function);

void close_handles_free_data(HANDLE * threads, int needable_threads);
void close_and_free_threads(HANDLE * threads, unsigned int threads_number);
int create_threads(HANDLE * threads, int threads_number, _In_ LPTHREAD_START_ROUTINE lpStartAddress);
DWORD WINAPI thread_entry(void * param);
int count_needable_threads(TaskData & task_data);

struct TaskData read_task_data(std::string file_name);
int putout_data(struct TaskData task_data, unsigned int time_counter, int * sorted_data);
int putout_task_data(std::string file_name, struct TaskData task_data, int * sorted_data);
int putout_time_data(std::string file_name, unsigned int time_counter);

void init_mutex_and_event();
void init_jobs(TaskData & task_data, int needable_threads, int job_size);
struct MergeJob pop_2job();
void push_job(int * job, int size);
struct JobData * joballoc();

void msort(int * array, int l, int r);
int * merge(int * a_part, int a_size, int * b_part, int b_size);


int main()
{
    int needable_threads, job_size;
    DWORD started, finished;
    struct TaskData task_data = read_task_data("input.txt");
    
    if(task_data.data_size < 0)
        return -1;
    else
    {
        needable_threads = count_needable_threads(task_data);
        job_size = task_data.data_size / needable_threads;
        msort_jobs.resize(needable_threads + 1);
    }

    HANDLE * threads = (HANDLE *)malloc(sizeof(HANDLE) * needable_threads);

    init_mutex_and_event();
    if(create_threads(threads, needable_threads, thread_entry) < 0)
        return -1;

    started = GetTickCount();
    init_jobs(task_data, needable_threads, job_size);
    SetEvent(thread_gate);
    while(WAIT_OBJECT_0 != WaitForMultipleObjects(needable_threads, threads, TRUE, INFINITE));
    finished = GetTickCount();

    if(putout_data(task_data, finished - started, merge_jobs.first->data) < 0)
        return -1;    

    close_handles_free_data(threads, needable_threads);
    return 0;
}


int err_info(const char* function) 
{ 
    fprintf(stderr, "%s: error : %d\n", function, GetLastError()); 
    return -1; 
}


void close_handles_free_data(HANDLE * threads, int needable_threads)
{
    CloseHandle(job_mutex);
    CloseHandle(thread_gate);
    free(merge_jobs.first->data);
    close_and_free_threads(threads, needable_threads);
}

void close_and_free_threads(HANDLE * threads, unsigned int threads_number)
{
    for(int i = 0; i < threads_number; i++)
        CloseHandle(threads[i]);
    free(threads);
}

int create_threads(HANDLE * threads, int threads_number, _In_ LPTHREAD_START_ROUTINE lpStartAddress)
{
    for(int i = 0; i < threads_number; i++)
    {
        threads[i] = CreateThread(0, 0, lpStartAddress, *((void **)&i), 0, NULL);
        if(threads[i] == 0)
        {
            printf("CreateThread failed. GetLastError(): %u\n", GetLastError());
            return err_info("CreateThread");
        }
    }
    return 0;
}

DWORD WINAPI thread_entry(void * param)
{    
    int idx = *(int *)(&param);
    WaitForSingleObject(thread_gate, INFINITE);

    msort(msort_jobs[idx].data, 0, msort_jobs[idx].size);
    push_job(msort_jobs[idx].data, msort_jobs[idx].size);

    while(1)
    {
        MergeJob temp_job = pop_2job();

        if(temp_job.jobs_amount < 2)
            break;
        
        int * new_job = merge(temp_job.data1, temp_job.size1, temp_job.data2, temp_job.size2);
        free(temp_job.data1);
        free(temp_job.data2);
        push_job(new_job, temp_job.size1 + temp_job.size2);
    }
    return 0;
}

int count_needable_threads(TaskData & task_data)
{
    int output = task_data.threads_number;
    while(output > 1 && task_data.data_size / output < 1000)
        output--;
    return output;
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
    
    data.resize(output.data_size);
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


void init_mutex_and_event()
{
    job_mutex = CreateMutex(NULL, FALSE, NULL);
    thread_gate = CreateEvent(NULL, TRUE, FALSE, NULL);
}

void init_jobs(TaskData & task_data, int needable_threads, int job_size)
{
    int data_start = -job_size; 
    for(int i = 0; i < needable_threads; i++)
    {
        if(i < needable_threads - 2)
            msort_jobs[i].size = job_size;
        else
            msort_jobs[i].size = job_size + task_data.data_size % job_size;

        msort_jobs[i].data = (int *)malloc(sizeof(int) * msort_jobs[i].size);
        data_start += job_size;
        for(int z = 0; z < msort_jobs[i].size; z++)
            msort_jobs[i].data[z] = data[data_start + z];
    }
}


struct MergeJob pop_2job()
{
    MergeJob output;
    output.jobs_amount = 0;
    WaitForSingleObject(job_mutex, INFINITE);
    if(merge_jobs.first != NULL)
    {
        output.jobs_amount = 1;
        if(merge_jobs.first->next != NULL)
        {
            output.jobs_amount = 2;
            output.data1 = merge_jobs.first->data;
            output.size1 = merge_jobs.first->size;
            output.data2 = merge_jobs.first->next->data;
            output.size2 = merge_jobs.first->next->size;
            merge_jobs.first = merge_jobs.first->next->next;
        }
    }
    ReleaseMutex(job_mutex);
    return output;
}

void push_job(int * job, int size)
{
    struct JobData * new_job = joballoc();
    new_job->data = (int *)malloc(sizeof(int) * size);
    memcpy(new_job->data, job, size * sizeof(int));
    new_job->size = size;
    WaitForSingleObject(job_mutex, INFINITE);
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
    ReleaseMutex(job_mutex);
}

struct JobData * joballoc()
{
    struct JobData * task = (struct JobData *)malloc(sizeof(struct JobData));
    task->next = NULL;
    task->data = NULL;
    task->size = 0;
    return task;
}


void msort(int * array, int l, int r)
{
    int l_idx = l;
    int r_idx = r / 2 + l_idx / 2;
    
    if(l + 1 < r)
    {
        msort(array, 0, r_idx);
        msort(array + r_idx, 0, r - r_idx);
    }

    int * new_job = merge(array, r_idx - l_idx, array + r_idx, r - r_idx);
    memcpy(array, new_job, sizeof(int) * r);
    free(new_job);
}

int * merge(int * a_part, int a_size, int * b_part, int b_size)
{
    int * buffer = (int *)malloc(sizeof(int) * (a_size + b_size));
    int a_idx = 0, b_idx = 0, buff_idx = 0;
    while(buff_idx < a_size + b_size)
    {
        if(a_idx == a_size)
            while(b_idx < b_size)
                buffer[buff_idx++] = b_part[b_idx++];
        
        else if(b_idx == b_size)
            while(a_idx < a_size)
                buffer[buff_idx++] = a_part[a_idx++];

        else
        {
            if(a_part[a_idx] <= b_part[b_idx])
                buffer[buff_idx++] = a_part[a_idx++];
            
            else
                buffer[buff_idx++] = b_part[b_idx++];
        }
    }
    return buffer;
}
