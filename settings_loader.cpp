#include "settings_loader.hpp"
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <vector>
#include "logging.hpp"

void settings::load(const std::string& loc)
{
    std::ifstream f;
    f.open(loc.c_str());

    if(!f.is_open())
    {
        lg::log("Could not find file");
        return;
    }

    std::vector<std::string> content;

    while(f.good())
    {
        std::string str;
        std::getline(f, str);
        content.push_back(str);
    }

    if(content.size() < 6)
    {
        lg::log("invalid settings file");
        return;
    }

    for(auto& i : content)
    {
        size_t it = i.find('#');

        if(it == std::string::npos)
            continue;

        if(it > 0)
        {
            ///also remove the preceding space before the comment
            ///should probably strip all spaces, but this is good enough
            if(i[it-1] == ' ')
            {
                it--;
            }

            i.erase(it, std::string::npos);
        }
    }

    width = atoi(content[0].c_str());
    height = atoi(content[1].c_str());
    ip = content[2];
    quality = atoi(content[3].c_str());
    name = content[4];
    enable_debugging = content[5] == "DEBUG";
    mouse_sens = atof(content[6].c_str());

    if(name.length() == 0)
    {
        name = "No Name";
    }
}
