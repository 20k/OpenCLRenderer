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

    if(content.size() < 5)
    {
        lg::log("invalid settings file");
        return;
    }

    width = atoi(content[0].c_str());
    height = atoi(content[1].c_str());
    ip = content[2];
    quality = atoi(content[3].c_str());
    name = content[4];
    enable_debugging = content[5] == "DEBUG";

    if(name.length() == 0)
    {
        name = "No Name";
    }
}
