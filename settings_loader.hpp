#ifndef SETTINGS_LOADER_HPP_INCLUDED
#define SETTINGS_LOADER_HPP_INCLUDED

#include <string>

struct settings
{
    int width = 1500;
    int height = 900;
    std::string ip = "192.168.1.2";
    int quality = 0;

    void load(const std::string& loc);
};

#endif // SETTINGS_LOADER_HPP_INCLUDED
