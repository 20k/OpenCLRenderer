#include "interact_manager.hpp"
#include <SFML/Graphics.hpp>
#include "engine.hpp"

bool interact::is_loaded = false;
sf::Image interact::pixel;
sf::Texture interact::texture_pixel;
sf::RenderWindow* interact::render_window;
std::vector<std::pair<int,int> > interact::pixel_stack;

void interact::set_render_window(sf::RenderWindow* win)
{
    render_window = win;
    if(!is_loaded)
    {
        load();
    }
}

void interact::load()
{
    if(is_loaded)
        return;

    pixel.create(1,1, sf::Color(255,0,0));
    texture_pixel.loadFromImage(pixel);

    is_loaded = true;
}

void interact::draw_pixel(int x, int y)
{
    pixel_stack.push_back(std::make_pair(x, engine::height - y));
}

void interact::deplete_stack()
{
    sf::Sprite spr;

    spr.setTexture(texture_pixel);

    for(int i=0; i<pixel_stack.size(); i++)
    {
        int x = pixel_stack[i].first;
        int y = pixel_stack[i].second;

        spr.setPosition(x, y);

        render_window->draw(spr);
    }

    pixel_stack.clear();
}
