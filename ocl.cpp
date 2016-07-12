#include "ocl.h"

std::map<std::string, compute::buffer*> automatic_argument_map;

inline
bool expect(char* text, const std::string& str, int& pos)
{
    for(int i=0; i<str.size(); i++)
    {
        if(text[pos + i] == '\0')
            return false;

        if(text[pos + i] != str[i])
            return false;
    }

    //lg::log(pos, " ", str.size());

    pos += str.size();

    return true;
}

inline
std::string until(char* text, const std::vector<char>& terminator_list, int& pos)
{
    std::string ret;

    while(text[pos] != '\0')
    {
        //if(text[pos] == terminator)
        //    return ret;

        for(auto& c : terminator_list)
        {
            if(c == text[pos])
                return ret;
        }

        ret.push_back(text[pos]);

        pos++;
    }
}

inline void discard_spaces(char* text, int& pos)
{
    while(text[pos] != '\0' && text[pos] == ' ')
    {
        pos++;
    }
}


void discard_whitespace(char* text, int& pos)
{
    while(text[pos] != '\0' && isspace(text[pos]))
    {
        pos++;
    }
}

std::vector<automatic_argment_identifiers> parse_automatic_arguments(char* text)
{
    if(text == nullptr)
        return {};

    lg::log("auto argument pass phase");

    ///need to identify kernel name too ;[
    std::vector<automatic_argment_identifiers> ident;

    std::string identifier = "AUTOMATIC";

    int desired_len = identifier.length();

    int current_character_looking_for = 0;

    int pos = 0;

    std::string current_kernel_name = "";

    while(text[pos] != '\0')
    {
        if(expect(text, "#define ", pos))
        {
            discard_spaces(text, pos);

            ///definition of the thing
            if(expect(text, identifier, pos))
            {

            }

            continue;
        }

        if(expect(text, identifier, pos))
        {
            if(expect(text, "(", pos))
            {
                std::string type = until(text, {','}, pos);

                if(!expect(text, ",", pos))
                {
                    lg::log("syntex error in automatic declaration");
                }

                discard_spaces(text, pos);

                std::string argument_name = until(text, {')'}, pos);

                lg::log("Automatic argument name ", type, " ", argument_name);
            }

            continue;
        }

        if(expect(text, "__kernel", pos) || expect(text, "kernel", pos))
        {
            lg::log("Found kernel identifier");

            discard_whitespace(text, pos);

            if(expect(text, "void", pos))
            {
                lg::log("found kernel void");

                discard_whitespace(text, pos);

                std::string kernel_name = until(text, {'('}, pos);

                while(kernel_name.size() > 0 && isspace(kernel_name[kernel_name.size()-1]))
                {
                    kernel_name.pop_back();
                }

                current_kernel_name = kernel_name;

                lg::log("found kernel ", current_kernel_name);
            }

            continue;
        }

        pos++;
    }

    return {};
}
