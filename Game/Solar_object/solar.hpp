#ifndef INCLUDED_SOLAR_HPP
#define INCLUDED_SOLAR_HPP

#include <boost/compute/source.hpp>
#include <boost/compute/system.hpp>

namespace compute = boost::compute;

struct solar_object
{
    compute::buffer game_pos;
    compute::buffer camera_pos;
};

#endif // INCLUDED_SOLAR_HPP
