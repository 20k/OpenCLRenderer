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

void network::send(int fd, std::string& msg)
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

void network::broadcast(std::string& msg)
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
}

void network::slave_object(objects_container* obj)
{
    int id = global_network_id++;

    ///do sanity checks later, ie already in list to prevent duplicate listings

    slave_networked_objects.push_back(std::pair<objects_container*, int>(obj, id));
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

///this function is literally hitler
void network::tick()
{
    ///enum?
    if(network_state == 0)
    {
        return;
    }

    for(auto& i : host_networked_objects)
    {
        objects_container* obj = i.first;

        int network_id = i.second;

        cl_float4 pos = obj->pos;
        cl_float4 rot = obj->rot;

        int len = sizeof(cl_float4)*2 + sizeof(int);

        char* mem = (char*)malloc(len + sizeof(char));

        memcpy(mem, &network_id, sizeof(int));
        memcpy((mem + sizeof(int)), &pos, sizeof(cl_float4));
        memcpy((mem + sizeof(int) + sizeof(cl_float4)), &rot, sizeof(cl_float4));

        mem[len] = '\0';

        broadcast(mem, len);

        free(mem);
    }

    while(any_readable())
    {
        auto msg = receive();

        ///remove first 20 charcters, go to here

        while(msg.size() > 0)
        {
            if(msg.size() < sizeof(int) + sizeof(cl_float4)*2)
            {
                std::cout << "error, no msg recieved ?????" << std::endl; ///fucking trigraphs
                break;
            }

            int network_id = *(int*)&msg[0];

            const char* buf = &msg[sizeof(int)];
            const cl_float4* pfloat4 = (const cl_float4*) buf; ///figuratively hitler

            const char* buf2 = &msg[sizeof(int) + sizeof(cl_float4)];
            const cl_float4* rfloat4 = (const cl_float4*) buf2;

            objects_container* obj = get_object_by_id(network_id);

            if(obj != NULL)
            {
                obj->set_pos(*pfloat4);
                obj->set_rot(*rfloat4);
                obj->g_flush_objects(); ///temporary/permanent
            }
            else
            {
                std::cout << "warning, invalid networked object" << std::endl;
            }

            auto it = msg.begin();

            std::advance(it, sizeof(int) + sizeof(cl_float4)*2);

            msg.erase(msg.begin(), it);
        }
    }

}
