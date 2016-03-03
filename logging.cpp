#include "logging.hpp"

std::string lg::logfile;
std::ofstream lg::output;

void lg::set_logfile(const std::string& file)
{
    output.open(file.c_str(), std::ofstream::out | std::ofstream::trunc);

    logfile = file;
}

/*void lg::log(const std::string& str)
{
    output << str << std::endl;
}*/
