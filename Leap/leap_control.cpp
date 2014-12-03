#include "leap_control.hpp"
#include <windows.h>
#include <iostream>

/*static Leap::Controller control;

void init()
{
    control.setPolicyFlags(Leap::Controller::POLICY_BACKGROUND_FRAMES);
}

std::vector<std::pair<cl_float4, int>> get_finger_positions()
{
    static bool initialised;

    if(!initialised)
    {
        init();
        initialised = true;
    }

    Leap::Frame frame = control.frame();
    Leap::FingerList fingers = frame.fingers();
    Leap::ToolList tools = frame.tools();
    Leap::HandList hands = frame.hands();

    float xmin = frame.interactionBox().width()*1.7;
    float ymin = 0;


    std::vector<std::pair<cl_float4, int>> positions;

    for(int i=0; i<hands.count(); i++)
    {
        const Leap::Hand hand = hands[i];
        Leap::FingerList h_fingers = hand.fingers();

        for(int j=0; j<h_fingers.count(); j++)
        {
            const Leap::Finger finger = h_fingers[j];

            float mfingerposx = finger.stabilizedTipPosition().x;
            float mfingerposy = finger.stabilizedTipPosition().y;
            float mfingerposz = finger.stabilizedTipPosition().z;

            cl_float4 ps = {mfingerposx, mfingerposy, mfingerposz, 0.0f};

            int id = finger.id();

            positions.push_back(std::pair<cl_float4, int>(ps, id));

        }
    }

}*/

#define PNAME "\\\\.\\pipe\\LogPipe"
/*#define PNAME "Local\\test"

finger_data get_finger_positions()
{
    static int init = 0;

    static HANDLE hMapFile;

    DWORD numRead;

    //float data[10*4];

    int len = sizeof(float)*10*4;

    volatile char* buf;

    if(init == 0)
    {
        hMapFile = OpenFileMapping(
                   FILE_MAP_ALL_ACCESS,   // read/write access
                   FALSE,                 // do not inherit the name
                   PNAME);               // name of mapping object

        if(hMapFile == NULL)
        {
            std::cout << "wub" << std::endl;
        }


        buf = (LPTSTR) MapViewOfFile(hMapFile, // handle to map object
        FILE_MAP_ALL_ACCESS,  // read/write permission
        0,
        0,
        len);

        if(buf == NULL)
        {
            std::cout << "oh my" << std::endl;
        }

        init = 1;
    }

    //ReadFile(pipe, data, sizeof(float)*10*4, &numRead, NULL);

    volatile float* data;

    //memcpy(data, buf, len);

    data = (volatile float*)buf;


    finger_data fdata;

    for(int i=0; i<10; i++)
    {
        fdata.fingers[i] = {0,0,0,0};
    }

    //if(numRead > 0)
    {
        for(int i=0; i<10; i++)
            fdata.fingers[i] = (cl_float4){data[i*4 + 0], data[i*4 + 1], data[i*4 + 2], data[i*4 + 3]};

        std::cout << fdata.fingers[0].x << std::endl;
    }

    return fdata;
}*/

finger_data get_finger_positions()
{
    static int init = 0;

    static HANDLE pipe;

    DWORD numRead;


    if(init == 0)
    {
        printf("trying to connect\n");

        pipe = CreateNamedPipe(PNAME, PIPE_ACCESS_INBOUND | PIPE_ACCESS_OUTBOUND , PIPE_WAIT, 1, 1024, 1024, 120 * 1000, NULL);

        ConnectNamedPipe(pipe, NULL);

        printf("connected\n");

        init = 1;
    }

    constexpr int read_chunk = 5;

    //float data[10*4*read_chunk];

    finger_data data[read_chunk] = {0};

    ReadFile(pipe, data, sizeof(finger_data)*read_chunk, &numRead, NULL);

    /*finger_data fdata;

    for(int i=0; i<10; i++)
    {
        fdata.fingers[i] = {0,0,0,0};
    }

    ///discard all extra data afterwards, might this break?
    if(numRead > 0)
    {
        for(int i=0; i<10; i++)
            fdata.fingers[i] = (cl_float4){data[i*4 + 0], data[i*4 + 1], data[i*4 + 2], data[i*4 + 3]};

    }*/

    return data[0];
}
