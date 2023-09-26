#include <windows.h>
#include <string>
#include <fstream>
#include <iostream>

// 1 - placed gifure from the beginnin
// 2 - blocked squares
// 3 - temp figure placed
// 0 - free

struct TaskData
{
    int threads_number;
    int figures_placed;
    int figures_to_place;
    int desk_size;
    int solutions;
};

struct FigureXY
{
    int x;
    int y;
};

CRITICAL_SECTION desk_CS;

int** desk;
int last_idx;
int jobs_for_last_thread = 0;
int total_free_space = 0;
int temp_placed_figures = 0;
TaskData task_data;
FigureXY* tasks = NULL;
int job_size;

int err_info(const char* function);
void place_queen_attack(struct FigureXY& figure, int number, int fig_number);
int count_taken_space(int x, int y);

DWORD WINAPI thread_entry(void* param);
int create_threads(HANDLE* threads, int threads_number, _In_ LPTHREAD_START_ROUTINE lpStartAddress);

struct TaskData read_task_data(std::string file_name);
int putout_data(struct TaskData task_data, unsigned int time_counter);
int putout_task_data(std::string file_name);
int putout_time_data(std::string file_name, unsigned int time_counter);

FigureXY get_start_coords(int start_zero);
int count_free_space();
void count_solutions(int y);
int isSafe(int y, int x);

int main()
{
    DWORD started, finished;

    task_data = read_task_data("input.txt");
    task_data.solutions = 0;
    if (task_data.desk_size <= 0)
        return -1;
    HANDLE* threads;
    total_free_space = count_free_space();
    if (task_data.figures_to_place > total_free_space)
        if (putout_data(task_data, 0) < 0)
            return -1;
        else
            return 0;

    if (total_free_space >= task_data.threads_number)
    {
        job_size = total_free_space / task_data.threads_number;
        jobs_for_last_thread = job_size + total_free_space % task_data.threads_number;
        last_idx = task_data.threads_number - 1;
        threads = (HANDLE*)malloc(sizeof(HANDLE) * task_data.threads_number);
    }
    else
    {
        jobs_for_last_thread = job_size = 1;
        last_idx = total_free_space - 1;
        threads = (HANDLE*)malloc(sizeof(HANDLE) * total_free_space);
    }

    InitializeCriticalSection(&desk_CS);
    if (create_threads(threads, task_data.threads_number, thread_entry) < 0)
        return -1;

    started = GetTickCount();
    while (WAIT_OBJECT_0 != WaitForMultipleObjects(task_data.threads_number, threads, TRUE, INFINITE));
    count_solutions(0);
    finished = GetTickCount();

    if (putout_data(task_data, finished - started) < 0)
        return -1;

    DeleteCriticalSection(&desk_CS);
    free(desk);
    free(threads);
    return 0;
}


int err_info(const char* function)
{
    fprintf(stderr, "%s: error : %d\n", function, GetLastError());
    return -1;
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
        output.desk_size = -1;
        output.threads_number = -1;
        return output;
    }

    input_file >> output.threads_number >> output.desk_size >> output.figures_to_place >> output.figures_placed;

    desk = (int**)malloc(sizeof(int*) * output.desk_size);
    for (int i = 0; i < output.desk_size; i++)
        desk[i] = (int*)calloc(output.desk_size, sizeof(int));


    FigureXY temp;
    for (int i = 0; i < output.figures_placed; i++)
    {
        input_file >> temp.y >> temp.x;
        //desk[temp.y][temp.x] = 1;
        place_queen_attack(temp, 2, 1);
    }

    input_file.close();
    return output;
}

void place_queen_attack(struct FigureXY& figure, int number, int fig_number)
{
    int i;

    desk[figure.y][figure.x] = fig_number;

    //right
    i = figure.x + 1;
    while (i < task_data.desk_size && desk[figure.y][i] == 0)
    {
        desk[figure.y][i] = number;
        i++;
    }
    //left
    i = figure.x - 1;
    while (i >= 0 && desk[figure.y][i] == 0)
    {
        desk[figure.y][i] = number;
        i--;
    }
    //up
    i = figure.y + 1;
    while (i < task_data.desk_size && desk[i][figure.x] == 0)
    {
        desk[i][figure.x] = number;
        i++;
    }
    //down
    i = figure.y - 1;
    while (i >= 0 && desk[i][figure.x] == 0)
    {
        desk[i][figure.x] = number;
        i--;
    }

    //right up
    i = 0;
    while (figure.y - i >= 0 && figure.x + i < task_data.desk_size && desk[figure.y - i][figure.x + i] == 0)
    {
        desk[figure.y - i][figure.x + i] = number;
        i++;
    }

    //left down
    i = 0;
    while (figure.y + i < task_data.desk_size && figure.x - i >= 0 && desk[figure.y + i][figure.x - i] == 0)
    {
        desk[figure.y + i][figure.x - i] = number;
        i++;
    }

    //left up
    i = 0;
    while (figure.y - i >= 0 && figure.x - i >= 0 && desk[figure.y - i][figure.x - i] == 0)
    {
        desk[figure.y - i][figure.x - i] = number;
        i++;
    }

    //right down
    i = 0;
    while (figure.y + i < task_data.desk_size && figure.x + i < task_data.desk_size && desk[figure.y + i][figure.x + i] == 0)
    {
        desk[figure.y + i][figure.x + i] = number;
        i++;
    }
}

int count_taken_space(int x, int y)
{
    int i, output = 1;

    //right
    i = x + 1;
    while (i < task_data.desk_size && (desk[y][i] == 0 || desk[y][i] == 2))
    {
        output++;
        i++;
    }
    //left
    i = x - 1;
    while (i >= 0 && (desk[y][i] == 0 || desk[y][i] == 2))
    {
        output++;
        i--;
    }
    //up
    i = y + 1;
    while (i < task_data.desk_size && (desk[i][x] == 0 || desk[i][x] == 2))
    {
        output++;
        i++;
    }
    //down
    i = y - 1;
    while (i >= 0 && (desk[i][x] == 0 || desk[i][x] == 2))
    {
        output++;
        i--;
    }

    //right up
    i = 0;
    while (y - i >= 0 && x + i < task_data.desk_size && (desk[y - i][x + i] == 0 || desk[y - i][x + i] == 2))
    {
        output++;
        i++;
    }

    //left down
    i = 0;
    while (y + i < task_data.desk_size && x - i >= 0 && (desk[y + i][x - i] == 0 || desk[y + i][x - i] == 2))
    {
        output++;
        i++;
    }

    //left up
    i = 0;
    while (y - i >= 0 && x - i >= 0 && (desk[y - i][x - i] == 0 || desk[y - i][x - i] == 2))
    {
        output++;
        i++;
    }

    //right down
    i = 0;
    while (y + i < task_data.desk_size && x + i < task_data.desk_size && (desk[y + i][x + i] == 0 || desk[y + i][x + i] == 2))
    {
        output++;
        i++;
    }
    return output;
}

DWORD WINAPI thread_entry(void* param)
{
    int idx = *(int*)(&param);
    int start_zero = idx * job_size;
    int zeros_checked = 0;
    FigureXY start_coords = get_start_coords(start_zero);

    if (idx != last_idx)
    {
        for (int i = start_coords.y; i < task_data.desk_size && zeros_checked < job_size; i++)
        {
            for (int j = start_coords.x; j < task_data.desk_size && zeros_checked < job_size; j++)
            {
                if (desk[i][j] == 0)
                {
                    if (total_free_space - count_taken_space(i, j) < task_data.figures_to_place - 1)
                    {
                        EnterCriticalSection(&desk_CS);
                        desk[i][j] = 2;
                        LeaveCriticalSection(&desk_CS);
                    }
                    zeros_checked++;
                }
            }
        }
    }
    else
    {
        for (int i = start_coords.y; i < task_data.desk_size && zeros_checked < jobs_for_last_thread; i++)
        {
            for (int j = start_coords.x; j < task_data.desk_size && zeros_checked < jobs_for_last_thread; j++)
            {
                if (desk[i][j] == 0)
                {
                    if (total_free_space - count_taken_space(i, j) < task_data.figures_to_place - 1)
                    {
                        EnterCriticalSection(&desk_CS);
                        desk[i][j] = 2;
                        LeaveCriticalSection(&desk_CS);
                    }
                    zeros_checked++;
                }
            }
        }
    }
    return 0;
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

int putout_data(struct TaskData task_data, unsigned int time_counter)
{
    if (putout_task_data("output.txt") < 0 || putout_time_data("time.txt", time_counter) < 0)
        return err_info("putout_data");
    return 0;
}

int putout_task_data(std::string file_name)
{
    std::ofstream output_file;
    output_file.open(file_name);
    if (!output_file.is_open())
    {
        std::cout << "File exists already? Cant create\n";
        return err_info("putout_task_datan");
    }

    output_file << task_data.solutions;

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

int count_free_space()
{
    int temp = 0;
    for (int i = 0; i < task_data.desk_size; i++)
        for (int j = 0; j < task_data.desk_size; j++)
            if (desk[i][j] == 0)
                temp++;
    return temp;
}

FigureXY get_start_coords(int start_zero)
{
    FigureXY output;
    int counter = 0;
    for (int i = 0; i < task_data.desk_size; i++)
    {
        for (int j = 0; j < task_data.desk_size; j++)
        {
            if (desk[i][j] == 0)
            {
                counter++;
                if (counter == start_zero)
                {
                    output.x = j;
                    output.y = i;
                    return output;
                }
            }
        }
    }
}


void count_solutions(int y)
{
    // если `figures_to_place` ферзей расставлены успешно, выводим решение

    if (y == task_data.desk_size || temp_placed_figures == task_data.figures_to_place)
    {
        if(temp_placed_figures == task_data.figures_to_place)
            task_data.solutions++;
        return;
    }
    // помещаем ферзя на каждую клетку в текущем ряду `r`
    // и повторяться для каждого допустимого движения
    bool if_placed = false;
    for (int i = 0; i < task_data.desk_size; i++)
    {
        if (isSafe(y, i) == 1)
        {
            if_placed = true;
            // ставим ферзя на текущую клетку
            desk[y][i] = 3;
            temp_placed_figures++;

            // повторить для следующей строки
            count_solutions(y + 1);

            // возвращаемся назад и удаляем ферзя с текущей клетки
            desk[y][i] = 0;
            temp_placed_figures--;
        }
    }
    count_solutions(y + 1);
}

int isSafe(int y, int x)
{
    // возвращаем 0, если два ферзя делят один и тот же столбец
    for (int i = 0; i < task_data.desk_size; i++)
    {
        if (desk[i][x] == 3 || desk[i][x] == 1) {
            return 0;
        }
    }

    for (int i = 0; i < task_data.desk_size; i++)
    {
        if (desk[y][i] == 3 || desk[y][i] == 1) {
            return 0;
        }
    }

    // вернуть 0, если два ферзя делят одну и ту же диагональ
    for (int i = y, j = x; i >= 0 && j >= 0; i--, j--)
    {
        if (desk[i][j] == 3 || desk[i][j] == 1) {
            return 0;
        }
    }

    // вернуть 0, если два ферзя делят одну и ту же диагональ `/`
    for (int i = y, j = x; i >= 0 && j < task_data.desk_size; i--, j++)
    {
        if (desk[i][j] == 3 || desk[i][j] == 1) {
            return 0;
        }
    }

    return 1;
}