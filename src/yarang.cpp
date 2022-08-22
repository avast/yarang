#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
#include <string>

#include <yarang/ruleset.hpp>

void collect_files(std::vector<std::string>& result, const std::string& file_path)
{
    if (std::filesystem::is_directory(file_path))
    {
        for (auto path : std::filesystem::directory_iterator{file_path})
        {
            if (std::filesystem::is_directory(path))
                collect_files(result, path.path().native());
            else
                result.push_back(path.path().native());
        }
    }
    else
        result.push_back(std::string{file_path});
}

std::vector<std::string> files_to_scan(const std::vector<std::string>& args)
{
    std::vector<std::string> result;
    for (const auto& arg : args)
        collect_files(result, arg);
    return result;
}

struct Options
{
    Options() : threads(1), ruleset(), input_files() {}

    int threads;
    std::string ruleset;
    std::vector<std::string> input_files;
    std::string cuckoo_file;
};

Options parse_options(std::vector<std::string>& args)
{
    Options result;

    std::size_t to_remove = 0;
    for (auto itr = args.begin(), end = args.end(); itr != end; ++itr)
    {
        auto& opt = *itr;
        if (opt[0] == '-')
            break;

        if (opt == "-t" || opt == "--threads")
        {
            if (itr + 1 == end)
                throw std::runtime_error("Option -t|--threads expects number of threads");

            itr++;
            result.threads = std::stoi(*itr);
        }
        else if (opt == "-c" || opt == "--cuckoo")
        {
            if (itr + 1 == end)
                throw std::runtime_error("Option -c|--cukoo expects path to the Cuckoo JSON");

            itr++;
            result.cuckoo_file = *itr;
        }
    }

    args.erase(args.begin(), args.begin() + to_remove);
    if (args.empty())
        throw std::runtime_error("Ruleset needs to be provided to yarang scanner");

    result.ruleset = args[0];
    args.erase(args.begin());

    if (args.empty())
        throw std::runtime_error("At least one input file/directory needs to be specified");

    result.input_files = files_to_scan(args);
    return result;
}

void print_hit(const char* rule, void* context)
{
    std::cout << (const char*)context << ": " << rule << std::endl;
}

int main(int argc, char* argv[])
{
    std::vector<std::string> args(argv + 1, argv + argc);
    auto options = parse_options(args);

    auto ruleset = Ruleset{options.ruleset};

    std::ifstream in_file(argv[2], std::ios::binary);
    in_file.seekg(0, std::ios::end);
    auto file_size = in_file.tellg();
    in_file.seekg(0, std::ios::beg);

    std::vector<char> data(file_size);
    in_file.read(data.data(), data.size());

    auto scanner = ruleset.new_scanner(print_hit);
    scanner.scan_data(data.data(), data.size(), !options.cuckoo_file.empty() ? options.cuckoo_file.c_str() : nullptr, argv[2]);

    return 0;
}
