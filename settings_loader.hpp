#ifndef SETTINGS_LOADER_HPP_INCLUDED
#define SETTINGS_LOADER_HPP_INCLUDED

#include <string>

struct settings
{
    int width = 1500;
    int height = 900;
    std::string ip = "192.168.1.2";
    int quality = 0;
    std::string name = "Lumberjack";
    bool enable_debugging;
    float mouse_sens = 1.f;

    void load(const std::string& loc);
    void save(const std::string& loc);
    ///we need a save function as well, increasingly urgently
};

#endif // SETTINGS_LOADER_HPP_INCLUDED
