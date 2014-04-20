#ifndef INCLUDED_INTERACT_MANAGER_HPP
#define INCLUDED_INTERACT_MANAGER_HPP

#include <SFML/Graphics.hpp>
#include <vector>
#include <utility>

struct point
{
    int x, y;
};

struct interact
{
    static sf::RenderWindow* render_window;

    static void draw_pixel(int, int);
    static int draw_rect(int, int, int, int);

    static void set_render_window(sf::RenderWindow*);

    static void load();

    static void deplete_stack();

    static int get_mouse_collision_rect(int, int);

private:
    static sf::Image pixel;
    static sf::Texture texture_pixel;
    static bool is_loaded;
    static std::vector<std::pair<int,int> > pixel_stack;
    static std::vector<std::pair<point, point> > rectangle_stack;
    static int selected;
};

#endif // INCLUDED_INTERACT_MANAGER_HPP
