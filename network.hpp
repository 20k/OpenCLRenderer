#ifndef NETWORKING_HPP
#define NETWORKING_HPP

#include <vector>
#include <map>
#include "objects_container.hpp"

#include <SFML/System/Clock.hpp>

#include <set>

struct byte_fetch;
struct addrinfo;
struct sockaddr_storage;

struct networked_variable
{
    void* ptr = nullptr;
    size_t size = 0;
};

struct network
{
    ///currently inoperable, pending review
    static int network_update_rate;

    //static sf::Clock timeout_clock;

    //static addrinfo* host_p;

    //static std::vector<int> networked_clients;
    static int socket_descriptor;

    ///now that i know what a map is, i should really, REALLY use one
    static std::map<int, objects_container*> host_networked_objects; ///authoratitive for me
    static std::map<int, objects_container*> slave_networked_objects; ///server authoratitive

    static std::map<int, networked_variable> hosted_var;
    static std::map<int, networked_variable> slaved_var;

    static std::map<int, bool> disconnected_sockets;

    static std::map<objects_container*, bool> active_status;

    //static std::map<std::string, int> ip_map;
    //static std::map<int, addrinfo*> id_to_addrinfo;

    static std::vector<sockaddr_storage*> connections;
    static std::vector<int> connection_length;

    static int global_network_id;

    static int network_state;

    static int join_id; ///init to -1, is a number when we've joined a game
    static bool loaded; ///for external use so we don't have to do bookkeeping. have we done the loading to join a game


    static void host();

    static void join(std::string);


    static void generate_networked_id();
    static void add_new_connection(sockaddr_storage*, int addrlen);

    static void host_object(objects_container*);
    static void slave_object(objects_container*);

    static void transform_host_object(objects_container*);
    static void transform_slave_object(objects_container*);

    template<typename T>
    static void host_var(T*);
    template<typename T>
    static void slave_var(T*);

    static void transform_host_var(void*);
    static void transform_slave_var(void*);

    ///send an update about var as if i am host... maybe this could entirely replace the host/slave system
    static void host_update(void* var);

    static void set_update_rate(int);

    ///broadcast has a 'skip', this is to avoid broadcasting data
    ///to the source which sent us the data!
    static void broadcast(const std::vector<char>&, int address_to_skip = -1);
    static void broadcast(const char*, int, int address_to_skip = -1);
    static void broadcast(networked_variable& v); ///starting to get less complex

    static void send(int id, const std::vector<char>&);
    static void send(int id, const char*, int);

    static std::vector<char> receive(int& ret_address);
    static std::vector<char> receive_any(int& ret_address);

    static bool any_readable();

    static objects_container* get_object_by_id(int);
    static int get_id_by_object(objects_container*);
    static int get_id_by_var(void*);
    static networked_variable* get_variable_by_var(void*);

    ///returns if we need to reallocate memory because somethings changed
    static bool tick();

    ///ping the server with a hello to get a joinresponse
    static void ping();

private:
    static bool process_posrot(byte_fetch& fetch);
    static bool process_isactive(byte_fetch& fetch);
    static bool process_var(byte_fetch& fetch);
    static bool process_joinresponse(byte_fetch& fetch);

    static void send_joinresponse(int id);
};

#endif // NETWORKING_HPP
