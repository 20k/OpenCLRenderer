#ifndef LOGGING_HPP_INCLUDED
#define LOGGING_HPP_INCLUDED

#include <string>
#include <fstream>
#include <iostream>

namespace lg
{
    extern std::ofstream output;

    extern std::string logfile;

    void set_logfile(const std::string& file);

    //void log(const std::string& txt);

    /*template<typename T>
    void logn(T val);*/

    template<typename T>
    inline
    void
    log_b(const T& dat)
    {
        output << std::to_string(dat);
    }

    ///I think there's a better sfinae solution to this, but
    template<>
    inline
    void
    log_b(const std::string& dat)
    {
        output << dat;
    }

    template<>
    inline
    void
    log_b(const char* const& dat)
    {
        log_b(std::string(dat));
    }

    template<typename T>
    inline
    void
    log_d(T&& param)
    {
        using decay_t = typename std::decay<T>::type;

        log_b<decay_t>(param);
    }

    //template<typename T>
    inline
    void
    log_r()
    {

    }

    template<typename T, typename... U>
    inline
    void
    log_r(T&& param, U&&... params)
    {
        log_d(param);
        log_r(params...);
    }


    template<typename... T>
    inline
    void
    log(T&&... param)
    {
        log_r(param...);

        output << std::endl;
    }
};

#endif // LOGGING_HPP_INCLUDED
