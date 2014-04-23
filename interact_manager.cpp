#include "interact_manager.hpp"
#include <SFML/Graphics.hpp>
#include "engine.hpp"

bool interact::is_loaded = false;
sf::Image interact::pixel;
sf::Texture interact::texture_pixel;
sf::RenderWindow* interact::render_window;
std::vector<std::pair<int,int> > interact::pixel_stack;
std::vector<std::pair<rect, rect_descriptor> > interact::rectangle_stack;
std::set<int> interact::ids_selected;
//int interact::selected = -1;

rect_descriptor::rect_descriptor() : attached_ship_id(-1)
{

}

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

int interact::draw_rect(int x1, int y1, int x2, int y2, int id)
{
    point one = {x1, engine::height - y1};
    point two = {x2, engine::height - y2};

    rect r;
    r.tl = one;
    r.br = two;

    rect_descriptor r_d;
    r_d.attached_ship_id = id;
    //r_d.selected = 0;

    rectangle_stack.push_back(std::make_pair<rect, rect_descriptor>(r, r_d));

    return rectangle_stack.size()-1;
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

    for(int i=0; i<rectangle_stack.size(); i++)
    {
        point one, two;
        one = rectangle_stack[i].first.tl;
        two = rectangle_stack[i].first.br;

        rect_descriptor r_d = rectangle_stack[i].second;

        ///must be specified tl tr

        sf::RectangleShape rshape;

        if(ids_selected.find(i)!=ids_selected.end())
            rshape.setFillColor(sf::Color(50, 255, 50));

        rshape.setPosition(one.x, one.y);
        rshape.setSize(sf::Vector2f(1, 5));
        render_window->draw(rshape);

        rshape.setPosition(two.x, one.y);
        render_window->draw(rshape);

        rshape.setPosition(one.x, two.y - 4);
        render_window->draw(rshape);

        rshape.setPosition(two.x, two.y - 4);
        render_window->draw(rshape);

        rshape.setSize(sf::Vector2f(5, 1));
        rshape.setPosition(one.x, one.y);
        render_window->draw(rshape);

        rshape.setPosition(two.x - 4, one.y);
        render_window->draw(rshape);

        rshape.setPosition(one.x, two.y);
        render_window->draw(rshape);

        rshape.setPosition(two.x - 4, two.y);
        render_window->draw(rshape);
    }


    pixel_stack.clear();
    rectangle_stack.clear();
}

int interact::get_mouse_collision_rect(int x, int y)
{
    for(int i=0; i<rectangle_stack.size(); i++)
    {
        if(x > rectangle_stack[i].first.tl.x && y > rectangle_stack[i].first.tl.y && x < rectangle_stack[i].first.br.x -1 && y < rectangle_stack[i].first.br.y -1)
        {
            return i;
        }
    }

    return -1;
}

int interact::get_collision_id(int id)
{
    return rectangle_stack[id].second.attached_ship_id;
}

bool interact::get_is_selected(int id)
{
    return ids_selected.find(id)!=ids_selected.end();
}

void interact::set_selected(int id)
{
    //rectangle_stack[id].second.selected = 1;
    ids_selected.insert(id);
}

void interact::unset_selected(int id)
{
    ids_selected.erase(id);
}
