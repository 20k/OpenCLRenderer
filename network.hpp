#ifndef NETWORKING_HPP
#define NETWORKING_HPP

#include <vector>
#include <map>
#include "objects_container.hpp"

#include <SFML/System/Clock.hpp>

struct byte_fetch;

struct network
{
    static int network_update_rate;

    static sf::Clock timeout_clock;

    static std::vector<int> networked_clients;
    static int listen_fd;

    ///now that i know what a map is, i should really, REALLY use one
    static std::vector<std::pair<objects_container*, int>> host_networked_objects; ///authoratitive for me
    static std::vector<std::pair<objects_container*, int>> slave_networked_objects; ///server authoratitive

    static std::map<int, int*> hosted_var;
    static std::map<int, int*> slaved_var;

    static std::map<int, bool> disconnected_sockets;

    static std::map<objects_container*, bool> active_status;

    static int global_network_id;

    static int network_state;


    static void host();

    static void join(std::string);


    static void generate_networked_id();

    static void host_object(objects_container*);
    static void slave_object(objects_container*);

    static void transform_host_object(objects_container*);
    static void transform_slave_object(objects_container*);

    static void host_var(int*);
    static void slave_var(int*);

    static void set_update_rate(int);

    static void broadcast(const std::string&);
    static void broadcast(char*, int);

    static void send(int, const std::string&);
    static void send(int, const char*, int);

    static std::vector<char> receive(int& len);
    static std::vector<char> receive(int, int& len);

    static bool any_readable();

    static objects_container* get_object_by_id(int);
    static int get_id_by_object(objects_container*);

    ///returns if we need to reallocate memory because somethings changed
    static bool tick();


private:
    static bool process_posrot(byte_fetch& fetch);
    static bool process_isactive(byte_fetch& fetch);
    static bool process_var(byte_fetch& fetch);
};

#endif // NETWORKING_HPP
