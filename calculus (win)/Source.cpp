#include <iostream>
#include <stdio.h>
#include <fstream>
#include <string>
#include <windows.h>
#include <vector>

#define TCNT (3)

struct TaskData
{
    unsigned int threads_number;
    unsigned int number;
    unsigned int decomps;
};

struct TaskData get_task_data(std::string file_name);

int putout_task_data(std::string file_name, struct TaskData data);
int putout_time_data(std::string file_name, unsigned int time_counter);

int file_err(const char* function);

DWORD WINAPI thread_entry(void * param);

void count_millisec(HANDLE &thread_to_count);


int main()
{
    unsigned int time_counter = 0;
    struct TaskData task_data = get_task_data("input.txt");
    HANDLE * threads = (HANDLE *)malloc(sizeof(HANDLE) * task_data.threads_number);

    int * param = NULL;

    for(int i = 0; i < TCNT; i++)
    {
        threads[i] = CreateThread(0, 0, thread_entry, (void *)param, 0, NULL);

        if(threads[i] == 0)
        {
            printf("CreateThread failed. GetLastError(): %u\n", GetLastError());
            return -1;
        }
    }

    while(WaitForMultipleObjects(task_data.threads_number, threads, TRUE, 1) != WAIT_OBJECT_0)
        time_counter++;
    printf("Threads finished\n");
    
    for(int i = 0; i < TCNT; i++)
        CloseHandle(threads[i]);
    
    free(threads);
    putout_task_data("output.txt", task_data);
    putout_time_data("time.txt", time_counter);
    return 0;
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
    output.decomps = 0;
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
    output_file << data.threads_number << data.number << data.decomps;

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


DWORD WINAPI thread_entry(void * param)
{
    //casting
    //const char * msg = (const char *)param;
    
    //do smth

    return 0;
}


void count_millisec(HANDLE &thread_to_count)
{
    if(WaitForSingleObject(thread_to_count, 1) == WAIT_OBJECT_0)
        printf("Thread finished");
}