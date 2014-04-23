#ifndef INCLUDED_INTERACT_MANAGER_HPP
#define INCLUDED_INTERACT_MANAGER_HPP

#include <SFML/Graphics.hpp>
#include <vector>
#include <utility>
#include <set>

struct point
{
    int x, y;
};

struct rect
{
    point tl, br;
};

struct rect_descriptor
{
    int identifier;
    //int selected;

    rect_descriptor();
};

struct interact
{
    static sf::RenderWindow* render_window;

    static void draw_pixel(int, int);
    static int draw_rect(int, int, int, int, int);

    static void set_render_window(sf::RenderWindow*);

    static void load();

    static void deplete_stack();

    static int get_mouse_collision_rect(int, int); ///rectangle id

    static int get_identifier_from_rect(int);

    static bool get_is_selected(int);

    static void set_selected(int);
    static void unset_selected(int);

private:
    static sf::Image pixel;
    static sf::Texture texture_pixel;
    static bool is_loaded;
    static std::vector<std::pair<int,int> > pixel_stack;
    static std::vector<std::pair<rect, rect_descriptor> > rectangle_stack;
    static std::set<int> ids_selected;
    //static int selected;
};

#endif // INCLUDED_INTERACT_MANAGER_HPP
