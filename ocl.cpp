#include "ocl.h"

std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}

bool supports_extension(const std::string& ext_name)
{
    size_t rsize;

    clGetDeviceInfo(cl::device.get(), CL_DEVICE_EXTENSIONS, 0, nullptr, &rsize);

    char* dat = new char[rsize];

    clGetDeviceInfo(cl::device.get(), CL_DEVICE_EXTENSIONS, rsize, dat, nullptr);

    std::vector<std::string> elements = split(dat, ' ');

    delete [] dat;

    for(auto& i : elements)
    {
        if(i == ext_name)
            return true;
    }

    return false;
}

std::map<std::string, driver_blacklist_info> build_driver_blacklist()
{
    std::map<std::string, driver_blacklist_info> ret;

    ///of course, the formal opencl version is invalid in this driver, but hopefully it is be consistent
    ///I actually don't know what the map key be, so for the moment this is temporary
    ret["21.19.134.1"].is_blacklisted = 1;
    ret["21.19.134.1"].friendly_name = "16.9.1";
    ret["21.19.134.1"].vendor = "AMD"; ///the vendor is only important if the driver crashes before we clGetDeviceIDs
    ret["21.19.134.1"].driver_name = "21.19.134.1";

    return ret;
}

std::map<std::string, driver_blacklist_info> driver_blacklist = build_driver_blacklist();

bool is_driver_blacklisted(const std::string& version)
{
    return driver_blacklist[version].is_blacklisted;
}

void print_blacklist_error_info()
{
    lg::log("Please check that your driver is not blacklisted (its too early in device init to be able to check unfortunately)");
    lg::log("These are the blacklisted drivers");

    for(auto& i : driver_blacklist)
    {
        driver_blacklist_info& blacklist_info = i.second;

        if(blacklist_info.is_blacklisted)
        {
            lg::log(blacklist_info.get_blacklist_string());
        }
    }
}

cl_int oclGetPlatformID(cl_platform_id* clSelectedPlatformID)
{
    char chBuffer[1024];
    cl_uint num_platforms;
    cl_platform_id* clPlatformIDs;
    cl_int ciErrNum;
    *clSelectedPlatformID = NULL;
    cl_uint i = 0;

    // Get OpenCL platform count
    ciErrNum = clGetPlatformIDs(0, NULL, &num_platforms);

    if(ciErrNum != CL_SUCCESS)
    {
        //printf(" Error %i in clGetPlatformIDs Call !!!\n\n", ciErrNum);

        lg::log("Error ", ciErrNum, " in clGetPlatformIDs");

        return -1000;
    }
    else
    {
        if(num_platforms == 0)
        {
            //printf("No OpenCL platform found!\n\n");

            lg::log("Could not find valid opencl platform, num_platforms == 0");

            return -2000;
        }
        else
        {
            // if there's a platform or more, make space for ID's
            if((clPlatformIDs = (cl_platform_id*)malloc(num_platforms * sizeof(cl_platform_id))) == NULL)
            {
                //printf("Failed to allocate memory for cl_platform ID's!\n\n");
                lg::log("Malloc error for allocating platform ids");

                return -3000;
            }

            // get platform info for each platform and trap the NVIDIA platform if found

            ciErrNum = clGetPlatformIDs(num_platforms, clPlatformIDs, NULL);
            //printf("Available platforms:\n");
            lg::log("Available platforms:");

            for(i = 0; i < num_platforms; ++i)
            {
                ciErrNum = clGetPlatformInfo(clPlatformIDs[i], CL_PLATFORM_NAME, 1024, &chBuffer, NULL);

                if(ciErrNum == CL_SUCCESS)
                {
                    //printf("platform %d: %s\n", i, chBuffer);

                    lg::log("platform ", i, " ", chBuffer);

                    /*std::string name(chBuffer);

                    if(name.find("CPU") != std::string::npos)
                    {
                        continue;
                    }*/

                    if(strstr(chBuffer, "NVIDIA") != NULL || strstr(chBuffer, "AMD") != NULL)// || strstr(chBuffer, "Intel") != NULL)
                    {
                        //printf("selected platform %d\n", i);
                        lg::log("selected platform ", i);
                        *clSelectedPlatformID = clPlatformIDs[i];
                        break;
                    }
                }
            }

            // default to zeroeth platform if NVIDIA not found
            if(*clSelectedPlatformID == NULL)
            {
                //printf("selected platform: %d\n", num_platforms-1);
                lg::log("selected platform ", num_platforms-1);
                *clSelectedPlatformID = clPlatformIDs[num_platforms-1];
            }

            free(clPlatformIDs);
        }
    }

    return CL_SUCCESS;
}

char* file_contents(const char *filename, int *length)
{
    FILE *f = fopen(filename, "r");
    void *buffer;

    if(!f)
    {
        lg::log("Unable to open ", filename, " for reading");
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    *length = ftell(f);
    fseek(f, 0, SEEK_SET);

    buffer = malloc(*length+1);
    *length = fread(buffer, 1, *length, f);
    fclose(f);
    ((char*)buffer)[*length] = '\0';

    return (char*)buffer;
}


std::map<std::string, void*> registered_automatic_argument_map;
std::vector<automatic_argument_identifiers> parsed_automatic_arguments;

std::thread build_thread;

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

    return ret;
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

std::vector<automatic_argument_identifiers> parse_automatic_arguments(char* text)
{
    if(text == nullptr)
        return {};

    lg::log("auto argument pass phase");

    std::map<std::string, automatic_argument_identifiers> kernel_arg_map;


    std::string identifier = "AUTOMATIC";

    int desired_len = identifier.length();

    int current_character_looking_for = 0;

    int pos = 0;

    std::string current_kernel_name = "";

    int argc = 0;

    bool in_kernel_arg_list = false;

    while(text[pos] != '\0')
    {
        /*if(expect(text, "#define ", pos))
        {
            discard_spaces(text, pos);

            ///definition of the thing
            if(expect(text, identifier, pos))
            {

            }

            continue;
        }*/

        if(in_kernel_arg_list && expect(text, identifier, pos))
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

                lg::log("Automatic argument name ", type, " ", argument_name, " for kernel ", current_kernel_name, " at argument number ", argc);

                automatic_argument_identifiers& aargs = kernel_arg_map[current_kernel_name];

                aargs.kernel_name = current_kernel_name;
                aargs.args.push_back({argc, type, argument_name});

                expect(text, ")", pos);
            }

            continue;
        }

        if(expect(text, "__kernel", pos) || expect(text, "kernel", pos))
        {
            discard_whitespace(text, pos);

            ///hmm. kernel with a name voidsomething would break
            if(expect(text, "void", pos))
            {
                discard_whitespace(text, pos);

                std::string kernel_name = until(text, {'('}, pos);

                while(kernel_name.size() > 0 && isspace(kernel_name[kernel_name.size()-1]))
                {
                    kernel_name.pop_back();
                }

                current_kernel_name = kernel_name;

                argc = 0;

                discard_whitespace(text, pos);

                if(!expect(text, "(", pos))
                    lg::log("parse error, ( expected after kernel declaration");
                else
                    in_kernel_arg_list = true;
            }

            continue;
        }

        if(in_kernel_arg_list && expect(text, ")", pos))
        {
            in_kernel_arg_list = false;

            continue;
        }

        if(in_kernel_arg_list && expect(text, ",", pos))
        {
            argc++;

            continue;
        }

        pos++;
    }

    std::vector<automatic_argument_identifiers> ret;

    for(auto& i : kernel_arg_map)
    {
        ret.push_back(i.second);
    }

    return ret;
}
