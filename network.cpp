#include "network.hpp"
#define _WIN32_WINNT 0x501

#include <ws2tcpip.h>
#include <iostream>
#include <sstream>
#include <memory>
#include <cstdio>

std::vector<int> network::networked_clients;
int network::listen_fd;

std::vector<std::pair<objects_container*, int>> network::host_networked_objects; ///authoratitive for me
std::vector<std::pair<objects_container*, int>> network::slave_networked_objects; ///server authoratitive

std::map<objects_container*, bool> network::active_status;

int network::global_network_id;
int network::network_state;

///mingw being testicles workaround
#ifdef __MINGW32__
template<typename T>
std::string to_str(T i)
{
    std::ostringstream convert;

    convert << i;

    return convert.str();
}
#endif

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void network::host()
{
    std::string port = "6950";

    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2,0), &wsaData) != 0)
    {
        fprintf(stderr, "WSAStartup failed\n");
        exit(1);
    }

    int sockfd, new_fd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    struct sockaddr_storage their_addr;
    int  addr_len;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // set to AF_INET to force IPv4 //currently AF_UNSPEC
    hints.ai_socktype = SOCK_STREAM; //sock_dgram for non stream.
    hints.ai_flags = AI_PASSIVE; // use my IP

    if((rv = getaddrinfo(NULL, port.c_str(), &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(2345);
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next)
    {
        if((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1)
        {
            perror("listener: socket");
            continue;

        }

        if(bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            closesocket(sockfd);
            perror("listener: bind");
            continue;
        }
        break;
    }

    if (p == NULL)
    {
        fprintf(stderr, "listener: failed to bind socket\n");
        exit(5432);
    }

    freeaddrinfo(servinfo);

    addr_len = sizeof(their_addr);

    new_fd=-1;

    listen_fd = sockfd;

    listen(sockfd, 5);

    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_len);

    networked_clients.push_back(new_fd);

    network_state = 1;
}

void network::join(std::string ip)
{
    std::string port = "6950";

    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2,0), &wsaData) != 0)
    {
        fprintf(stderr, "WSAStartup failed.\n");
        exit(1);
    }

    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    //hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(ip.c_str(), port.c_str(), &hints, &servinfo)) != 0)   //77.100.255.158 //85.228.220.111 //66 bubby
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(2345);
    }

    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1)
        {
            perror("talker: socket");
            continue;
        }

        break;
    }

    if (p == NULL)
    {
        fprintf(stderr, "talker: failed to bind socket\n");
        exit(5432);
    }

    connect(sockfd, p->ai_addr, p->ai_addrlen);

    networked_clients.push_back(sockfd);

    network_state = 2;
}


bool is_readable(int clientfd)
{
    fd_set fds;
    struct timeval tmo;

    tmo.tv_sec=0;
    tmo.tv_usec=0;

    FD_ZERO(&fds);
    FD_SET((UINT32)clientfd, &fds);

    select(clientfd+1, &fds, NULL, NULL, &tmo);

    if(FD_ISSET((uint32_t)clientfd, &fds))
    {
        return true;
    }

    return false;
}

bool is_writable(int clientfd)
{
    fd_set fds;
    struct timeval tmo;

    tmo.tv_sec=0;
    tmo.tv_usec=0;

    FD_ZERO(&fds);
    FD_SET((UINT32)clientfd, &fds);

    select(clientfd+1, NULL, &fds, NULL, &tmo);

    if(FD_ISSET((uint32_t)clientfd, &fds))
    {
        return true;
    }

    return false;
}

decltype(send)* send_t = &send;

void network::send(int fd, const std::string& msg)
{
    send(fd, msg.c_str(), msg.length());
}

void network::send(int fd, const char* msg, int len)
{
    bool sent = false;

    while(!sent)
    {
        if(is_writable(fd))
        {
            send_t(fd, msg, len, 0);
            sent = true;
        }
    }
}

void network::broadcast(const std::string& msg)
{
    for(auto& i : networked_clients)
    {
        int fd = i;

        send(fd, msg);
    }
}

void network::broadcast(char* msg, int len)
{
    for(auto& i : networked_clients)
    {
        int fd = i;

        send(fd, msg, len);
    }
}

//std::unique_ptr<char> network::receive(int fd, int& len)
std::vector<char> network::receive(int fd)
{
    char buf[1000] = {0}; ///entire string null terminated
    int len = recv(fd, buf, 1000*sizeof(char), 0);

    if(len <= 0)
        return std::vector<char>();

    std::vector<char> ptr;
    ptr.resize(len);

    for(int i=0; i<len; i++)
        ptr[i] = buf[i];

    return ptr;
}

std::vector<char> network::receive()
{
    for(auto& i : networked_clients)
    {
        if(is_readable(i))
        {
            return receive(i);
        }
    }

    return std::vector<char>();
}

bool network::any_readable()
{
    for(auto& i : networked_clients)
    {
        if(is_readable(i))
            return true;
    }

    return false;
}

void network::host_object(objects_container* obj)
{
    int id = global_network_id++;

    ///do sanity checks later, ie already in list to prevent duplicate listings

    host_networked_objects.push_back(std::pair<objects_container*, int>(obj, id));

    active_status[obj] = obj->isactive;
}

void network::slave_object(objects_container* obj)
{
    int id = global_network_id++;

    ///do sanity checks later, ie already in list to prevent duplicate listings

    slave_networked_objects.push_back(std::pair<objects_container*, int>(obj, id));

    active_status[obj] = obj->isactive;
}

void network::transform_host_object(objects_container* obj)
{
    int id = get_id_by_object(obj);

    if(id < 0)
        return;

    for(int i=0; i<slave_networked_objects.size(); i++)
    {
        if(slave_networked_objects[i].second == id)
        {
            auto it = slave_networked_objects.begin();
            std::advance(it, i);
            slave_networked_objects.erase(it);

            break;
        }
    }

    ///already host
    for(int i=0; i<host_networked_objects.size(); i++)
    {
        if(host_networked_objects[i].second == id)
            return;
    }

    host_networked_objects.push_back(std::pair<objects_container*, int>(obj, id));
}

void network::transform_slave_object(objects_container* obj)
{
    int id = get_id_by_object(obj);

    if(id < 0)
        return;

    for(int i=0; i<host_networked_objects.size(); i++)
    {
        if(host_networked_objects[i].second == id)
        {
            auto it = host_networked_objects.begin();
            std::advance(it, i);
            host_networked_objects.erase(it);

            break;
        }
    }

    ///already slave
    for(int i=0; i<slave_networked_objects.size(); i++)
    {
        if(slave_networked_objects[i].second == id)
            return;
    }

    slave_networked_objects.push_back(std::pair<objects_container*, int>(obj, id));
}

objects_container* network::get_object_by_id(int id)
{
    for(auto& i : slave_networked_objects)
    {
        if(i.second == id)
            return i.first;
    }

    for(auto& i : host_networked_objects)
    {
        if(i.second == id)
            return i.first;
    }

    return NULL;
}

int network::get_id_by_object(objects_container* obj)
{
    for(auto& i : slave_networked_objects)
    {
        if(i.first == obj)
            return i.second;
    }

    for(auto& i : host_networked_objects)
    {
        if(i.first == obj)
            return i.second;
    }

    return -1;
}

struct byte_vector
{
    std::vector<char> ptr;

    template<typename T>
    void push_back(T v)
    {
        char* pv = (char*)&v;

        for(int i=0; i<sizeof(T); i++)
        {
            ptr.push_back(pv[i]);
        }
    }

    std::string data()
    {
        std::string dat;

        dat.append(&ptr[0], ptr.size());

        return dat;
    }
};

struct byte_fetch
{
    std::vector<char> ptr;

    int internal_counter;

    byte_fetch()
    {
        internal_counter = 0;
    }

    template<typename T>
    void push_back(T v)
    {
        char* pv = (char*)&v;

        for(int i=0; i<sizeof(T); i++)
        {
            ptr.push_back(pv[i]);
        }
    }

    void push_back(std::vector<char> v)
    {
        ptr.insert(ptr.end(), v.begin(), v.end());
    }

    template<typename T>
    T get()
    {
        int prev = internal_counter;

        internal_counter += sizeof(T);

        return *(T*)&ptr[prev];
    }
};

const int canary = 0xdeadbeef;
const int end_canary = 0xafaefeae;

void network::communicate(objects_container* obj)
{

}

enum comm_type : unsigned int
{
    POSROT = 0,
    ISACTIVE = 1
};

bool network::process_posrot(byte_fetch& fetch)
{
    int network_id = fetch.get<int>();

    cl_float4 pos = fetch.get<cl_float4>();
    cl_float4 rot = fetch.get<cl_float4>();
    int found_end = fetch.get<int>();

    if(found_end != end_canary)
        return false;

    objects_container* obj = get_object_by_id(network_id);

    ///make this only the bits if they are different
    if(obj != NULL)
    {
        obj->set_pos(pos);
        obj->set_rot(rot);

        obj->g_flush_objects(); ///temporary/permanent
    }
    else
    {
        std::cout << "warning, invalid networked object" << std::endl;
    }

    return true;
}

bool network::process_isactive(byte_fetch& fetch)
{
    int network_id = fetch.get<int>();
    int isactive = fetch.get<int>();
    int found_end = fetch.get<int>();

    if(found_end != end_canary)
        return false;

    objects_container* obj = get_object_by_id(network_id);

    if(obj != nullptr)
    {
        obj->set_active(isactive);
    }
    else
    {
        std::cout <<  "warning, invalid networked objects" << std::endl;
    }

    return true;
}


///this function is literally hitler
///we're gunna need to send different events like is_active ONLY IF THEY CHANGE
///this is so that anyone can have authority over them
///how the hell are we gunna do security like this, this is awful
bool network::tick()
{
    ///enum?
    if(network_state == 0)
    {
        return false;
    }

    for(auto& i : host_networked_objects)
    {
        objects_container* obj = i.first;

        int network_id = i.second;

        cl_float4 pos = obj->pos;
        cl_float4 rot = obj->rot;

        byte_vector vec;

        vec.push_back(canary);
        vec.push_back((comm_type)POSROT);
        vec.push_back(network_id);
        vec.push_back(pos);
        vec.push_back(rot);
        vec.push_back(end_canary);

        broadcast(vec.data());
    }

    for(auto& i : active_status)
    {
        objects_container* obj = i.first;

        int network_id = get_id_by_object(obj);

        int isactive = obj->isactive;

        int think_isactive = i.second;

        if(isactive != think_isactive)
        {
            byte_vector vec;

            vec.push_back(canary);
            vec.push_back(ISACTIVE);
            vec.push_back(network_id);
            vec.push_back(isactive);
            vec.push_back(end_canary);

            broadcast(vec.data());
        }
    }

    bool need_realloc = false;

    while(any_readable())
    {
        auto msg = receive();

        ///remove first 20 charcters, go to here

        while(msg.size() > 0)
        {
            byte_fetch fetch;
            fetch.push_back(msg);

            int recv_canary = fetch.get<int>();

            if(recv_canary != canary)
            {
                msg.erase(msg.begin(), msg.begin() + 1);

                continue;
            }

            comm_type t = fetch.get<comm_type>();


            bool success = false;

            if(t == POSROT && msg.size() >= sizeof(int)*4 + sizeof(cl_float4)*2)
            {
                success = process_posrot(fetch);
            }
            if(t == ISACTIVE)
            {
                success = process_isactive(fetch);

                need_realloc = true;
            }

            auto it = msg.begin();

            if(success)
                std::advance(it, fetch.internal_counter);
            else
                std::advance(it, 1);

            msg.erase(msg.begin(), it);
        }
    }

    for(auto& i : host_networked_objects)
    {
        active_status[i.first] = i.first->isactive;
    }

    for(auto& i : slave_networked_objects)
    {
        active_status[i.first] = i.first->isactive;
    }

    return need_realloc;
}
