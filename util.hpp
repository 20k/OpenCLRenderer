#include <vector>
#include <string>

///https://stackoverflow.com/questions/236129/split-a-string-in-c
inline
std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

inline
std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}

inline
std::string get_file_name(const std::string& path)
{
    size_t s = path.find_last_of('/\\');

    return path.substr(s+1, std::string::npos);
}
