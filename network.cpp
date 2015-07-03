#include "network.hpp"
#define _WIN32_WINNT 0x501

#include <ws2tcpip.h>
#include <iostream>
#include <sstream>
#include <memory>
#include <cstdio>

sf::Clock network::timeout_clock;

std::vector<int> network::networked_clients;
int network::listen_fd;

std::vector<std::pair<objects_container*, int>> network::host_networked_objects; ///authoratitive for me
std::vector<std::pair<objects_container*, int>> network::slave_networked_objects; ///server authoratitive

std::map<int, int*> network::hosted_var;
std::map<int, int*> network::slaved_var;

std::map<objects_container*, bool> network::active_status;

std::map<int, bool> network::disconnected_sockets;

int network::global_network_id;
int network::network_state;

int network::network_update_rate = 60;

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
    disconnected_sockets[new_fd] = false;

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

///only true if the socket can be read from AND IT ISNT DISCONNECTED
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
        return !network::disconnected_sockets[clientfd];
    }

    return false;
}

namespace write_status
{
    enum writable_status : unsigned int
    {
        NO,
        YES,
        DISCONNECTED
    };
}


write_status::writable_status is_writable(int clientfd)
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
        if(network::disconnected_sockets[clientfd])
            return write_status::DISCONNECTED;

        return write_status::YES;
    }

    return write_status::NO;
}

decltype(send)* send_t = &send;

void network::send(int fd, const std::string& msg)
{
    send(fd, msg.c_str(), msg.length());
}

void network::send(int fd, const char* msg, int len)
{
    if(len == 0)
        return;

    bool sent = false;

    while(!sent)
    {
        auto status = is_writable(fd);

        if(status == write_status::YES)
        {
            send_t(fd, msg, len, 0);
            sent = true;
        }

        if(status == write_status::DISCONNECTED)
            return;

        printf("%i ", sent);
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
std::vector<char> network::receive(int fd, int& len)
{
    std::vector<char> ptr;
    ptr.resize(100);

    len = recv(fd, &ptr[0], 100*sizeof(char), 0);

    if(len == 0)
        return std::vector<char>();

    if(len < 0)
    {
        disconnected_sockets[fd] = true;

        return std::vector<char>();
    }

    ptr.resize(len);

    return ptr;
}

std::vector<char> network::receive(int& len)
{
    for(auto& i : networked_clients)
    {
        if(is_readable(i))
        {
            return receive(i, len);
        }
    }

    len = 0;

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


void network::host_var(int* v)
{
    if(v == nullptr)
        return;

    int id = global_network_id++;

    hosted_var[id] = v;
}

void network::slave_var(int* v)
{
    if(v == nullptr)
        return;

    int id = global_network_id++;

    slaved_var[id] = v;
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

void network::set_update_rate(int rate)
{
    network_update_rate = rate;
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

    void push_back(const std::vector<char>& v)
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

enum comm_type : unsigned int
{
    POSROT = 0,
    ISACTIVE = 1,
    VAR = 2
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

bool network::process_var(byte_fetch& fetch)
{
    int network_id = fetch.get<int>();
    int val = fetch.get<int>();
    int found_end = fetch.get<int>();

    if(found_end != end_canary)
        return false;

    if(slaved_var[network_id] == nullptr)
        return false;

    *slaved_var[network_id] = val;
}


///this function is literally hitler
///we're gunna need to send different events like is_active ONLY IF THEY CHANGE
///this is so that anyone can have authority over them
///how the hell are we gunna do security like this, this is awful
bool network::tick()
{
    ///enum?
    if(network_state == 0)
        return false;

    byte_vector vec;

    for(auto& i : host_networked_objects)
    {
        objects_container* obj = i.first;

        int network_id = i.second;

        cl_float4 pos = obj->pos;
        cl_float4 rot = obj->rot;

        vec.push_back(canary);
        vec.push_back((comm_type)POSROT);
        vec.push_back(network_id);
        vec.push_back(pos);
        vec.push_back(rot);
        vec.push_back(end_canary);
    }

    for(auto& i : hosted_var)
    {
        ///cant be null
        int* var = i.second;

        int network_id = i.first;

        int val = *var;

        vec.push_back(canary);
        vec.push_back(VAR);
        vec.push_back(network_id);
        vec.push_back(val);
        vec.push_back(end_canary);
    }

    for(auto& i : active_status)
    {
        objects_container* obj = i.first;

        int network_id = get_id_by_object(obj);

        int isactive = obj->isactive;

        int think_isactive = i.second;

        if(isactive != think_isactive)
        {
            vec.push_back(canary);
            vec.push_back(ISACTIVE);
            vec.push_back(network_id);
            vec.push_back(isactive);
            vec.push_back(end_canary);
        }
    }

    broadcast(vec.data());

    sf::Clock c;


    bool need_realloc = false;

    while(any_readable())
    {
        int len;
        auto msg = receive(len);

        byte_fetch fetch;
        fetch.ptr.swap(msg);

        while(fetch.internal_counter < fetch.ptr.size())
        {
            int recv_canary = fetch.get<int>();

            if(recv_canary != canary)
                continue;

            int byte_backup = fetch.internal_counter;

            comm_type t = fetch.get<comm_type>();

            bool success = false;

            if(t == POSROT && fetch.ptr.size() >= sizeof(int)*4 + sizeof(cl_float4)*2)
            {
                success = process_posrot(fetch);
            }
            if(t == ISACTIVE)
            {
                success = process_isactive(fetch);

                need_realloc = true;
            }
            if(t == VAR)
            {
                success = process_var(fetch);
            }

            if(!success)
            {
                fetch.internal_counter = byte_backup;
            }
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
