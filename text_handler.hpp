#ifndef INCLUDED_TEXT_HANDLER_HPP
#define INCLUDED_TEXT_HANDLER_HPP

#include <vector>
#include <string>

#include <SFML/Graphics.hpp>

struct text_list
{
    int x, y;
    std::vector<std::string> elements;
    std::vector<sf::Color> colours;

    void set_pos(int, int);
};

struct text_handler
{
    static std::vector<text_list> text;

    static sf::Font font;

    static sf::RenderWindow* window;

    static void load_font();

    static void set_render_window(sf::RenderWindow*);

    static void queue_text_block(text_list);

    static void render();
};


#endif // INCLUDED_TEXT_HANDLER_HPP
