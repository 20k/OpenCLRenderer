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
    bool enable_debugging = false;
    float mouse_sens = 1.f;
    float motion_blur_strength = 1.f;
    float motion_blur_camera_contribution = 1.f;
    bool use_post_aa = true;
    bool use_raw_input = true;
    int frames_of_input_lag = 1;
    float horizontal_fov_degrees = 90;
    int use_frametime_management = 0;
    int is_fullscreen = 0;

    void load(const std::string& loc);
    void save(const std::string& loc);
    ///we need a save function as well, increasingly urgently
};

#endif // SETTINGS_LOADER_HPP_INCLUDED
