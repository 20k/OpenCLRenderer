#include "network.hpp"
#define _WIN32_WINNT 0x601

#include <ws2tcpip.h>
#include <iostream>
#include <sstream>
#include <memory>
#include <cstdio>

//sf::Clock network::timeout_clock;

//addrinfo* network::host_p;

//std::vector<int> network::networked_clients;
int network::socket_descriptor;

std::map<int, objects_container*> network::host_networked_objects; ///authoratitive for me
std::map<int, objects_container*> network::slave_networked_objects; ///server authoratitive

std::map<int, networked_variable> network::hosted_var;
std::map<int, networked_variable> network::slaved_var;

std::map<objects_container*, bool> network::active_status;

std::map<int, bool> network::disconnected_sockets;

int network::global_network_id;
int network::network_state;

int network::network_update_rate = 60;

//std::map<std::string, int> network::ip_map;

//std::map<int, addrinfo*> network::id_to_addrinfo;

std::vector<sockaddr_storage*> network::connections;
std::vector<int> network::connection_length;

std::vector<std::pair<int, int>> network::signals;

int network::join_id = -1;
bool network::loaded = false;


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

struct s
{
    int fd;

    s() : fd(-1) {}
};

/*int address_to_socket(const std::string& ip)
{
    static std::map<std::string, s> socket_map;

    if(socket_map[ip].fd != -1)
        return socket_map[ip].fd;

    std::string port = "6950";

    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0)
    {
        fprintf(stderr, "WSAStartup failed.\n");
        exit(1);
    }

    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    //hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(ip.c_str(), port.c_str(), &hints, &servinfo)) != 0)
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

    network::id_to_addrinfo[sockfd] = p;

    //network::host_p = p;

    //connect(sockfd, p->ai_addr, p->ai_addrlen);

    s b;
    b.fd = sockfd;

    socket_map[ip] = b;

    network::ip_map[ip] = sockfd;

    return sockfd;
}*/

bool operator==(sockaddr_storage s1, sockaddr_storage s2)
{
    char* ip1 = (char*)get_in_addr((sockaddr*)&s1);

    char* ip2 = (char*)get_in_addr((sockaddr*)&s2);

    if(strcmp(ip1, ip2) == 0)
        return true;

    return false;
}

void network::host()
{
    std::string port = "6950";

    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0)
    {
        fprintf(stderr, "WSAStartup failed\n");
        exit(1);
    }

    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // set to AF_INET to force IPv4 //currently AF_UNSPEC
    hints.ai_socktype = SOCK_DGRAM; //sock_dgram for non stream.
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

        if(bind(sockfd, p->ai_addr, p->ai_addrlen) != 0)
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

    socket_descriptor = sockfd;

    disconnected_sockets[sockfd] = false;

    network_state = 1;
}

void network::join(std::string ip)
{
    std::string port = "6950";

    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0)
    {
        fprintf(stderr, "WSAStartup failed.\n");
        exit(1);
    }

    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    //hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(ip.c_str(), port.c_str(), &hints, &servinfo)) != 0)
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

    //int len = sendto(sockfd, "bum", strlen("bum"), 0, p->ai_addr, p->ai_addrlen);

    //printf("%i\n", len);


    connections.push_back((sockaddr_storage*)p->ai_addr);
    connection_length.push_back(p->ai_addrlen);

    socket_descriptor = sockfd;

    disconnected_sockets[sockfd] = false;

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

void network::send(int id, const std::vector<char>& msg)
{
    send(id, &msg[0], msg.size());
}

///will crash if disco while send
void network::send(int id, const char* msg, int len)
{
    if(len == 0)
        return;

    bool sent = false;

    while(!sent)
    {
        auto status = is_writable(socket_descriptor);

        if(status == write_status::YES)
        {
            sockaddr_storage* dest = connections[id];
            int size = connection_length[id];

            int nl = sendto(socket_descriptor, msg, len, 0, (sockaddr*)dest, size);

            sent = true;
        }

        if(status == write_status::DISCONNECTED)
            return;
    }
}

void network::broadcast(const std::vector<char>& msg, int address_to_skip)
{
    broadcast(&msg[0], msg.size(), address_to_skip);
}

void network::broadcast(const char* msg, int len, int address_to_skip)
{
    for(int i=0; i<connections.size(); i++)
    {
        if(i == address_to_skip)
            continue;

        send(i, msg, len);
    }
}

void network::add_new_connection(sockaddr_storage* their_addr, int len)
{
    connections.push_back(their_addr);
    connection_length.push_back(len);

    send_joinresponse(connections.size()-1);
}

const int canary = 0xdeadbeef;
const int end_canary = 0xafaefead;

const char end_ar[4] = {0xaf, 0xae, 0xfe, 0xad};

std::vector<char> network::receive_any(int& ret_address)
{
    constexpr int l = 2000;

    static std::vector<char> recv_buffer(l);

    recv_buffer.resize(l);

    int len;

    if(network_state == 2)
    {
        struct sockaddr_storage their_addr;
        int fromlen = sizeof(sockaddr_storage);

        len = recvfrom(socket_descriptor, &recv_buffer[0], l*sizeof(char), 0, (sockaddr*)&their_addr, &fromlen);

        ret_address = 0;
    }

    if(network_state == 1)
    {
        struct sockaddr_storage* their_addr = new sockaddr_storage;
        int fromlen = sizeof(sockaddr_storage);

        len = recvfrom(socket_descriptor, &recv_buffer[0], l*sizeof(char), 0, (sockaddr*)their_addr, &fromlen);

        /*char s[INET6_ADDRSTRLEN];

        printf("listener: got packet from %s\n",
        inet_ntop(their_addr->ss_family,
            get_in_addr((struct sockaddr *)their_addr),
            s, sizeof s));

        free(their_addr);*/

        //printf("Received %i", len);

        int num = 0;
        bool add = true;

        for(auto& i : connections)
        {
            if(*i == *their_addr)
            {
                add = false;
                ret_address = num;
            }

            num++;
        }

        if(add)
        {
            add_new_connection(their_addr, fromlen);
            ret_address = connections.size() - 1;
        }
        else
            delete their_addr;
    }

    if(len == -1)
    {
        printf("%i\n", WSAGetLastError());
    }

    if(len < 0)
        return std::vector<char>();

    if(len == 0)
        return std::vector<char>();


    recv_buffer.resize(len);

    return recv_buffer;
}

std::vector<char> network::receive(int& ret_address)
{
    if(is_readable(socket_descriptor))
    {
        return receive_any(ret_address);
    }

    return std::vector<char>();
}

bool network::any_readable()
{
    if(is_readable(socket_descriptor))
        return true;

    return false;
}

void network::host_object(objects_container* obj)
{
    int id = global_network_id++;

    ///do sanity checks later, ie already in list to prevent duplicate listings

    host_networked_objects[id] = obj;

    active_status[obj] = obj->isactive;
}

void network::slave_object(objects_container* obj)
{
    int id = global_network_id++;

    ///do sanity checks later, ie already in list to prevent duplicate listings

    slave_networked_objects[id] = obj;

    active_status[obj] = obj->isactive;
}

template<typename T>
void network::host_var(T* v)
{
    if(v == nullptr)
        return;

    for(auto& i : hosted_var)
        if(i.second.ptr == v)
            return;

    for(auto& i : slaved_var)
        if(i.second.ptr == v)
            return;

    int id = global_network_id++;

    networked_variable var;
    var.ptr = v;
    var.size = sizeof(T);

    hosted_var[id] = var;
}

template<typename T>
void network::slave_var(T* v)
{
    if(v == nullptr)
        return;

    for(auto& i : hosted_var)
        if(i.second.ptr == v)
            return;

    for(auto& i : slaved_var)
        if(i.second.ptr == v)
            return;


    int id = global_network_id++;

    networked_variable var;
    var.ptr = v;
    var.size = sizeof(T);

    slaved_var[id] = var;
}

template void network::host_var<int>(int*);
template void network::slave_var<int>(int*);

template void network::host_var<float>(float*);
template void network::slave_var<float>(float*);

void network::transform_host_object(objects_container* obj)
{
    int id = get_id_by_object(obj);

    if(id < 0)
        return;

    bool found = false;

    for(auto& i : slave_networked_objects)
    {
        if(i.second == obj)
            found = true;
    }

    if(!found)
        return;

    host_networked_objects[id] = slave_networked_objects[id];

    slave_networked_objects.erase(id);
}

void network::transform_slave_object(objects_container* obj)
{
    int id = get_id_by_object(obj);

    if(id < 0)
        return;


    bool found = false;

    for(auto& i : host_networked_objects)
    {
        if(i.second == obj)
            found = true;
    }

    if(!found)
        return;

    slave_networked_objects[id] = host_networked_objects[id];

    host_networked_objects.erase(id);
}

void network::transform_host_var(void* var)
{
    int id = get_id_by_var(var);

    if(id < 0)
        return;

    bool found = false;

    for(auto& i : slaved_var)
    {
        if(i.second.ptr == var)
            found = true;
    }

    if(!found)
        return;

    hosted_var[id] = slaved_var[id];

    slaved_var.erase(id);
}

void network::transform_slave_var(void* var)
{
    int id = get_id_by_var(var);

    if(id < 0)
        return;

    bool found = false;

    for(auto& i : hosted_var)
    {
        if(i.second.ptr == var)
            found = true;
    }

    if(!found)
        return;

    slaved_var[id] = hosted_var[id];

    hosted_var.erase(id);
}

int network::get_id_by_var(void* var)
{
    for(auto& i : slaved_var)
    {
        if(i.second.ptr == var)
            return i.first;
    }

    for(auto& i : hosted_var)
    {
        if(i.second.ptr == var)
            return i.first;
    }

    return -1;
}

networked_variable* network::get_variable_by_var(void* var)
{
    for(auto& i : slaved_var)
    {
        if(i.second.ptr == var)
            return &i.second;
    }

    for(auto& i : hosted_var)
    {
        if(i.second.ptr == var)
            return &i.second;
    }

    return nullptr;
}

objects_container* network::get_object_by_id(int id)
{
    for(auto& i : slave_networked_objects)
    {
        if(i.first == id)
            return i.second;
    }

    for(auto& i : host_networked_objects)
    {
        if(i.first == id)
            return i.second;
    }

    return nullptr;
}

int network::get_id_by_object(objects_container* obj)
{
    for(auto& i : slave_networked_objects)
    {
        if(i.second == obj)
            return i.first;
    }

    for(auto& i : host_networked_objects)
    {
        if(i.second == obj)
            return i.first;
    }

    return -1;
}

void network::host_update(void* var)
{
    if(network_state == 0)
        return;

    networked_variable* v = get_variable_by_var(var);

    if(v == nullptr)
    {
        printf("Variable not found\n");
        return;
    }

    broadcast(*v);
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

        for(uint32_t i=0; i<sizeof(T); i++)
        {
            ptr.push_back(pv[i]);
        }
    }

    void push_back(const networked_variable& var)
    {
        char* pv = (char*)var.ptr;

        for(uint32_t i=0; i<var.size; i++)
        {
            ptr.push_back(pv[i]);
        }
    }

    std::vector<char> data()
    {
       /* std::string dat;

        dat.append(&ptr[0], ptr.size());

        return dat;*/

        return ptr;
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

    void* get(int size)
    {
        int prev = internal_counter;

        internal_counter += size;

        return (void*)&ptr[prev];
    }
};

void network::ping()
{
    if(network_state == 1)
    {
        printf("Server cannot ping itself\n");
        return;
    }

    byte_vector vec;
    vec.push_back(canary);
    vec.push_back(end_canary);

    broadcast(vec.data());
}


enum comm_type : unsigned int
{
    POSROT = 0,
    ISACTIVE = 1,
    VAR = 2,
    JOINRESPONSE = 3,
    AUDIO = 4
};

void network::broadcast(networked_variable& v)
{
    networked_variable var = v;

    int network_id = get_id_by_var(v.ptr);

    if(network_id < 0)
    {
        printf("Cannot broadcast var, not found\n");
        return;
    }

    if(v.ptr == nullptr)
        return;


    byte_vector vec;

    vec.push_back(canary);
    vec.push_back(VAR);
    vec.push_back(network_id);
    vec.push_back(var);
    vec.push_back(end_canary);

    broadcast(vec.data());
}


/*void network::send_signal(int type, int data)
{
    byte_vector vec;
    vec.push_back(canary);
    vec.push_back(SIGNAL);
    vec.push_back(type);
    vec.push_back(data);
    vec.push_back(end_canary);

    broadcast(vec.data());
}

int network::find_signal(int type)
{
    for(int i=0; i<signals.size(); i++)
    {
        if(signals[i].first == type)
        {
            int dat = signals[i].second;

            signals.erase(signals.begin() + i);

            return dat;
        }
    }
}*/


void network::send_joinresponse(int id)
{
    byte_vector vec;
    vec.push_back(canary);
    vec.push_back(JOINRESPONSE);
    vec.push_back((int)connections.size());
    vec.push_back(end_canary);

    printf("To client: %i\n", connections.size());

    send(id, vec.data());
}

///if server, do nothing
bool network::process_joinresponse(byte_fetch& fetch)
{
    int connection_num = fetch.get<int>();

    int found_end = fetch.get<int>();

    if(found_end != end_canary)
        return false;

    printf("From server: %i\n", connection_num);

    join_id = connection_num;

    return true;
}

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
        printf("%i\n", network_id);

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

    bool found = false;

    for(auto& i : slaved_var)
        if(i.first == network_id)
            found = true;

    if(!found)
        return false;


    int size = slaved_var[network_id].size;

    void* vptr = fetch.get(size);

    //int val = fetch.get<int>();

    int found_end = fetch.get<int>();

    if(found_end != end_canary)
        return false;

    memcpy(slaved_var[network_id].ptr, vptr, size);

    //*slaved_var[network_id] = val;

    return true;
}

/*bool network::process_signal(byte_fetch& fetch)
{
    int type = fetch.get<int>();
    int data = fetch.get<int>();

    int found_end = fetch.get<int>();

    if(found_end != end_canary)
        return false;

    signals.push_back({type, data});
}*/


///this function is literally hitler
///we're gunna need to send different events like is_active ONLY IF THEY CHANGE
///this is so that anyone can have authority over them
///how the hell are we gunna do security like this, this is awful
bool network::tick()
{
    ///enum?
    if(network_state == 0)
        return false;

    ///remove everything since the last network tick
    signals.clear();

    //static sf::Clock t;

    //float update_time = 16.f;

    ///to seconds
    //float change_time = t.getElapsedTime().asMicroseconds() / 1000.f;

    //if(change_time > update_time)
    {
        for(auto& i : host_networked_objects)
        {
            objects_container* obj = i.second;

            ///really should remove
            if(obj == nullptr)
                continue;

            int network_id = i.first;

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

        for(auto& i : hosted_var)
        {
            ///cant be null
            networked_variable var = i.second;

            if(var.ptr == nullptr)
                continue;

            broadcast(var);
        }

        for(auto& i : active_status)
        {
            objects_container* obj = i.first;

            if(obj == nullptr)
                continue;

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

        //t.restart();
    }

    bool need_realloc = false;

    while(any_readable())
    {
        int received_from;
        std::vector<char> msg = receive(received_from);

        if(msg.size() > 0 && network_state == 1)
            broadcast(&msg[0], msg.size(), received_from); ///skip the address that sent this to me, and retransmit IF I AM HOST to all others

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
            if(t == JOINRESPONSE)
            {
                success = process_joinresponse(fetch);
            }
            if(t == AUDIO)
            {
                //success = process_signal(fetch);
            }

            if(!success)
            {
                fetch.internal_counter = byte_backup;
            }
        }
    }

    for(auto& i : host_networked_objects)
    {
        active_status[i.second] = i.second->isactive;
    }

    for(auto& i : slave_networked_objects)
    {
        active_status[i.second] = i.second->isactive;
    }

    return need_realloc;
}
