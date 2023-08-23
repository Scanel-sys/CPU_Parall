#include <iostream>
#include <fstream>
#include <string>

struct TaskData
{
    unsigned int threads_number;
    unsigned int number;
    unsigned int decomps;
};

struct TaskData get_task_data(std::string file_name);

void putout_task_data(std::string file_name, struct TaskData data);


int main()
{
    struct TaskData task_data = get_task_data("input.txt");

    

    putout_task_data("output.txt", task_data);
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


void putout_task_data(std::string file_name, struct TaskData data)
{
    std::ofstream output_file;
    output_file.open(file_name);

    if(!output_file.is_open())
    {
        std::cout << "File exists already? Cant create\n";
        exit; 
    }
    output_file << data.threads_number << data.number << data.decomps;

    output_file.close();
}
