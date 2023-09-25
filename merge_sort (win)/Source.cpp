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
    struct JobData volatile* first;
    struct JobData volatile* last;
};

struct JobData
{
    volatile struct JobData* next;
    int data_start_index;
    int data_end_index;
    int size;
    int idx;
};

struct MergeJob
{
    int jobs_amount;
    int data1_start;
    int size1;
    int data2_start;
    int size2;
};

int* data = NULL;
int* buffer = NULL;

JobData* msort_first_jobs = NULL;
struct JobInfo merge_jobs;

HANDLE job_mutex;
HANDLE thread_gate;

int err_info(const char* function);

void close_handles_free_data(HANDLE* threads, int needable_threads);
void close_and_free_threads(HANDLE* threads, unsigned int threads_number);
int create_threads(HANDLE* threads, int threads_number, _In_ LPTHREAD_START_ROUTINE lpStartAddress);
DWORD WINAPI thread_entry(void* param);
int count_needable_threads(TaskData& task_data);

struct TaskData read_task_data(std::string file_name);
int putout_data(struct TaskData task_data, unsigned int time_counter, int* sorted_data);
int putout_task_data(std::string file_name, struct TaskData task_data, int* sorted_data);
int putout_time_data(std::string file_name, unsigned int time_counter);

void init_mutex_and_event();
void init_jobs(TaskData& task_data, int needable_threads, int job_size);
struct MergeJob pop_2job();
void push_job(int job_start, int size);
struct JobData* joballoc();

void msort(int l, int r);
void merge(int a_start, int a_size, int b_start, int b_size);

struct TaskData task_data;

int main()
{
    int needable_threads, job_size;
    DWORD started, finished;
    task_data = read_task_data("input.txt");

    if (task_data.data_size <= 0)
        return -1;
    else
    {
        needable_threads = count_needable_threads(task_data);
        job_size = task_data.data_size / needable_threads;
        msort_first_jobs = (JobData*)malloc(sizeof(JobData) * needable_threads);
    }

    HANDLE* threads = (HANDLE*)malloc(sizeof(HANDLE) * needable_threads);

    init_mutex_and_event();
    if (create_threads(threads, needable_threads, thread_entry) < 0)
        return -1;

    started = GetTickCount();
    init_jobs(task_data, needable_threads, job_size);
    SetEvent(thread_gate);
    while (WAIT_OBJECT_0 != WaitForMultipleObjects(needable_threads, threads, TRUE, INFINITE));
    finished = GetTickCount();

    if (putout_data(task_data, finished - started, data) < 0)
        return -1;

    close_handles_free_data(threads, needable_threads);
    return 0;
}


int err_info(const char* function)
{
    fprintf(stderr, "%s: error : %d\n", function, GetLastError());
    return -1;
}


void close_handles_free_data(HANDLE* threads, int needable_threads)
{
    CloseHandle(job_mutex);
    CloseHandle(thread_gate);
    free(data);
    free(buffer);
    close_and_free_threads(threads, needable_threads);
}

void close_and_free_threads(HANDLE* threads, unsigned int threads_number)
{
    for (unsigned int i = 0; i < threads_number; i++)
        CloseHandle(threads[i]);
    free(threads);
}

int create_threads(HANDLE* threads, int threads_number, _In_ LPTHREAD_START_ROUTINE lpStartAddress)
{
    for (int i = 0; i < threads_number; i++)
    {
        threads[i] = CreateThread(0, 0, lpStartAddress, *((void**)&i), 0, NULL);
        if (threads[i] == 0)
        {
            printf("CreateThread failed. GetLastError(): %u\n", GetLastError());
            return err_info("CreateThread");
        }
    }
    return 0;
}

DWORD WINAPI thread_entry(void* param)
{
    int idx = *(int*)(&param);
    WaitForSingleObject(thread_gate, INFINITE);

    msort(msort_first_jobs[idx].data_start_index, msort_first_jobs[idx].size);
    push_job(msort_first_jobs[idx].data_start_index, msort_first_jobs[idx].size);

    while (1)
    {
        MergeJob temp_job = pop_2job();

        if (temp_job.jobs_amount < 2)
            break;

        merge(temp_job.data1_start, temp_job.size1, temp_job.data2_start, temp_job.size2);
        push_job(temp_job.data1_start, temp_job.size1 + temp_job.size2);
    }
    return 0;
}

int count_needable_threads(TaskData& task_data)
{
    int output = task_data.threads_number;
    while (output > 1 && task_data.data_size / output < 1000)
        output--;
    return output;
}


struct TaskData read_task_data(std::string file_name)
{
    struct TaskData output;
    std::ifstream input_file;
    input_file.open(file_name);

    if (!input_file.is_open())
    {
        std::cout << "Seems like file doesnt exist\n";
        err_info("read_task_data");
        output.data_size = -1;
        output.threads_number = -1;
        return output;
    }

    input_file >> output.threads_number >> output.data_size;

    data = (int*)malloc(output.data_size * sizeof(int));
    buffer = (int*)malloc(output.data_size * sizeof(int));
    for (int i = 0; i < output.data_size; i++)
        input_file >> data[i];

    input_file.close();
    return output;
}

int putout_data(struct TaskData task_data, unsigned int time_counter, int* sorted_data)
{
    if (putout_task_data("output.txt", task_data, sorted_data) < 0 || putout_time_data("time.txt", time_counter) < 0)
        return err_info("putout_data");
    return 0;
}

int putout_task_data(std::string file_name, struct TaskData task_data, int* sorted_data)
{
    std::ofstream output_file;
    output_file.open(file_name);
    if (!output_file.is_open())
    {
        std::cout << "File exists already? Cant create\n";
        return err_info("putout_task_datan");
    }

    output_file << task_data.threads_number << '\n' << task_data.data_size << '\n';
    for (int i = 0; i < task_data.data_size; i++)
        output_file << sorted_data[i] << ' ';
    output_file << '\n';

    output_file.close();
    return 0;
}

int putout_time_data(std::string file_name, unsigned int time_counter)
{
    std::ofstream output_file;
    output_file.open(file_name);
    if (!output_file.is_open())
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

void init_jobs(TaskData& task_data, int needable_threads, int job_size)
{
    int data_start = -job_size;
    for (int i = 0; i < needable_threads; i++)
    {
        data_start += job_size;
        if (i < needable_threads - 2)
            msort_first_jobs[i].size = job_size;
        else
            msort_first_jobs[i].size = job_size + task_data.data_size % job_size;

        msort_first_jobs[i].data_start_index = data_start;
    }
}


struct MergeJob pop_2job()
{
    MergeJob output;
    output.jobs_amount = 0;
    volatile JobData* temp_job_data1 = NULL;
    volatile JobData* temp_job_data2 = NULL;
    volatile JobData* prev_data1 = NULL;
    volatile JobData* prev_data2 = NULL;
    WaitForSingleObject(job_mutex, INFINITE);
    temp_job_data1 = merge_jobs.first;
    if (temp_job_data1 != NULL)
    {
        output.jobs_amount = 1;
        bool jobs_checked = false;
        while (!jobs_checked)
        {
            prev_data2 = temp_job_data2;
            temp_job_data2 = temp_job_data1->next;
            while (temp_job_data2 != NULL && temp_job_data1->data_end_index != temp_job_data2->data_start_index)
                temp_job_data2 = temp_job_data2->next;

            if (temp_job_data2 != NULL)
            {
                output.jobs_amount = 2;
                output.data1_start = temp_job_data1->data_start_index;
                output.size1 = temp_job_data1->size;
                output.data2_start = temp_job_data2->data_start_index;
                output.size2 = temp_job_data2->size;
                merge_jobs.first = merge_jobs.first->next->next;

                prev_data1->next = prev_data1->next->next;
                prev_data2->next = prev_data2->next->next;
                free((void*)temp_job_data1);
                free((void*)temp_job_data2);

                jobs_checked = true;
            }
            else
            {
                prev_data1 = temp_job_data1;
                temp_job_data1 = temp_job_data1->next;
                if (temp_job_data1 == NULL)
                    jobs_checked = true;
            }
        }
    }
    ReleaseMutex(job_mutex);
    return output;
}

void push_job(int job_start, int size)
{
    struct JobData* new_job = joballoc();
    new_job->data_start_index = job_start;
    new_job->size = size;
    new_job->data_end_index = new_job->data_start_index + new_job->size;
    WaitForSingleObject(job_mutex, INFINITE);
    if (merge_jobs.first == NULL)
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

struct JobData* joballoc()
{
    struct JobData* task = (struct JobData*)malloc(sizeof(struct JobData));
    task->next = NULL;
    task->data_start_index = 0;
    task->size = 0;
    task->idx = 0;
    return task;
}


void msort(int l, int r)
{
    int r_idx = r / 2 + l / 2 + (r % 2 + l % 2) / 2;

    if (l + 1 < r)
    {
        msort(l, r_idx);
        msort(r_idx, r);
    }

    merge(l, r_idx - l, r_idx, r - r_idx);
    memcpy(data + l, buffer + l, sizeof(int) * (r - l));
}

void merge(int a_start, int a_size, int b_start, int b_size)
{
    int a_idx = 0, b_idx = 0, buff_idx = 0;
    while (buff_idx < a_size + b_size)
    {
        if (a_idx == a_size)
            while (b_idx < b_size)
                buffer[a_start + buff_idx++] = data[b_start + b_idx++];

        else if (b_idx == b_size)
            while (a_idx < a_size)
                buffer[a_start + buff_idx++] = data[a_start + a_idx++];

        else
        {
            if (data[a_start + a_idx] <= data[b_start + b_idx])
                buffer[a_start + buff_idx++] = data[a_start + a_idx++];

            else
                buffer[a_start + buff_idx++] = data[b_start + b_idx++];
        }
    }
}