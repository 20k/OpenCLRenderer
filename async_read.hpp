#ifndef ASYNC_READ_HPP_INCLUDED
#define ASYNC_READ_HPP_INCLUDED

#include <boost/compute/source.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/iota.hpp>
#include <boost/compute/interop/opengl.hpp>
#include <assert.h>

#include "logging.hpp"

template<typename T>
struct async_read
{
    T* buffer = nullptr;
    compute::event ev;
    bool going = false;

    /*
    cl_int clEnqueueReadImage (	cl_command_queue command_queue,
        cl_mem image,
        cl_bool blocking_read,
        const size_t origin[3],
        const size_t region[3],
        size_t row_pitch,
        size_t slice_pitch,
        void *ptr,
        cl_uint num_events_in_wait_list,
        const cl_event *event_wait_list,
        cl_event *event)*/

    ///sigh. This isn't going to work with reallocing the gpu side stuff
    ///maybe I should just give in and make everything a parameter in opencl instead of compile time constants
    ///so we never need to reallocate the gpu side
    void image_read(cl_mem gpu_buffer, int x, int y)
    {
        if(buffer != nullptr)
        {
            delete buffer;
            buffer = nullptr;
        }

        buffer = new T();

        size_t origin[3] = {x, y, 0};
        size_t region[3] = {1, 1, 1};

        cl_event event;

        cl_int ret =
        clEnqueueReadImage(cl::cqueue.get(), gpu_buffer, CL_FALSE, origin, region, 0, 0, buffer, 0, nullptr, &event);

        if(ret != CL_SUCCESS)
        {
            lg::log("Broken read in image_read ", ret);

            return;
        }

        ev = compute::event(event, false);

        going = true;
    }

    /*
    cl_int clEnqueueReadBuffer (	cl_command_queue command_queue,
        cl_mem buffer,
        cl_bool blocking_read,
        size_t offset,
        size_t cb,
        void *ptr,
        cl_uint num_events_in_wait_list,
        const cl_event *event_wait_list,
        cl_event *event)*/

    ///The second argument is NOT a size, infer size of elements from sizeof(T)
    void buffer_read(cl_mem gpu_buffer, int offset_num_elements)
    {
        if(buffer != nullptr)
        {
            delete buffer;
            buffer = nullptr;
        }

        buffer = new T();

        cl_event event;

        cl_int ret =
        clEnqueueReadBuffer(cl::cqueue.get(), gpu_buffer, CL_FALSE, offset_num_elements * sizeof(T), sizeof(T), buffer, 0, nullptr, &event);

        if(ret != CL_SUCCESS)
        {
            lg::log("Broken read in buffer_read ", ret);

            return;
        }

        ev = compute::event(event, false);

        going = true;
    }

    /*cl_int clGetEventInfo (	cl_event event,
 	cl_event_info param_name,
 	size_t param_value_size,
 	void *param_value,
 	size_t *param_value_size_ret)
    */

    bool finished()
    {
        assert(going == true);

        cl_int status;

        cl_int ret = clGetEventInfo(ev.get(), CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(cl_int), &status, nullptr);

        assert(ret == CL_SUCCESS);

        return status == CL_COMPLETE;
    }

    T* get()
    {
        if(finished())
            return buffer;

        return nullptr;
    }

    void destroy()
    {
        delete buffer;
    }

    bool valid()
    {
        return going;
    }
};


#endif // ASYNC_READ_HPP_INCLUDED
