#ifndef NETWORKING_HPP
#define NETWORKING_HPP

#include <vector>
#include "objects_container.hpp"


struct network
{
    static std::vector<int> networked_clients;
    static int listen_fd;

    static std::vector<std::pair<objects_container*, int>> host_networked_objects; ///authoratitive for me
    static std::vector<std::pair<objects_container*, int>> slave_networked_objects; ///server authoratitive

    static int global_network_id;

    static int network_state;


    static void host();

    static void join(std::string);


    static void generate_networked_id(objects_container*);

    static void host_object(objects_container*);

    static void slave_object(objects_container*);


    static void broadcast(std::string&);
    static void broadcast(char*, int);

    static void send(int, std::string&);
    static void send(int, const char*, int);

    static std::vector<char> receive();

    static std::vector<char> receive(int);

    static bool any_readable();

    static objects_container* get_object_by_id(int);

    static void tick();
};

#endif // NETWORKING_HPP
