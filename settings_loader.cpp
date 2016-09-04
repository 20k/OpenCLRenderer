#include "settings_loader.hpp"
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <vector>
#include "logging.hpp"

inline bool exists (const std::string& name) {
    std::ifstream f(name.c_str());
    return f.good();
}

std::vector<std::string> get_content(const std::string& loc)
{
    std::ifstream f;
    f.open(loc.c_str());

    if(!f.is_open())
    {
        lg::log("Could not find file");
        return std::vector<std::string>();
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
        return std::vector<std::string>();
    }

    return content;
}

void settings::load(const std::string& loc)
{
    std::vector<std::string> content = get_content(loc);

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

    if(content.size() < 7)
        return;

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

void settings::save(const std::string& loc)
{
    std::vector<std::string> content = get_content(loc);

    std::vector<std::string> comments;
    comments.resize(content.size());

    for(int n=0; n<content.size(); n++)
    {
        std::string i = content[n];

        size_t it = i.find('#');

        if(it == std::string::npos)
            continue;

        if(it > 0)
        {
            if(i[it-1] == ' ')
            {
                it--;
            }

            comments[n].append(i.begin() + it, i.end());
        }
    }

    std::vector<std::string> to_save;

    int c = 0;
    to_save.push_back(std::to_string(width));
    to_save.push_back(std::to_string(height));
    to_save.push_back(ip);
    to_save.push_back(std::to_string(quality));
    to_save.push_back(name);

    std::string debug_string = enable_debugging == 0 ? "RELEASE" : "DEBUG";
    to_save.push_back(debug_string);

    to_save.push_back(std::to_string(mouse_sens) + "f");


    for(int i=0; i<comments.size() && i < to_save.size(); i++)
    {
        to_save[i].append(comments[i].begin(), comments[i].end());
    }

    std::string fname = loc + ".bak";

    {
        std::ofstream out(fname);

        for(auto& i : to_save)
        {
            out << i << std::endl;
        }
    }

    std::string old = loc + ".old";

    if(exists(old))
    {
        remove(old.c_str());
    }

    ///if new filename exists, probably crash
    if(exists(loc.c_str()))
        rename(loc.c_str(), (loc + ".old").c_str());

    rename(fname.c_str(), loc.c_str());
}
