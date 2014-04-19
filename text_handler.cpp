#include "text_handler.hpp"

std::vector<text_list> text_handler::text;

sf::Font text_handler::font;

sf::RenderWindow* text_handler::window;

void text_list::set_pos(int px, int py)
{
    x = px;
    y = py;
}

void text_handler::load_font()
{
    font.loadFromFile("VeraMono.ttf");
}

void text_handler::set_render_window(sf::RenderWindow* rw)
{
    window = rw;
}

void text_handler::queue_text_block(text_list tl)
{
    text.push_back(tl);
}

void text_handler::render()
{
    int font_size = 10;

    for(int i=0; i<text.size(); i++)
    {
        text_list& tl = text[i];

        int running_y = tl.y;

        for(int j=0; j<tl.elements.size(); j++)
        {
            std::string& txt = tl.elements[j];

            sf::String str;

            str = txt;

            sf::Text to_draw;

            to_draw.setString(str);
            to_draw.setFont(font);
            to_draw.setPosition(tl.x, running_y);
            to_draw.setCharacterSize(font_size);

            window->draw(to_draw);

            running_y += to_draw.getGlobalBounds().height + 2;
        }
    }

    text.clear();
}
