/****************************************************************************

    File: DrawRectangle.hpp
    Author: Andrew Janke
    License: GPLv3

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*****************************************************************************/

#ifndef DRAW_RECTANGLE_HPP
#define DRAW_RECTANGLE_HPP

#include <SFML/Graphics/Drawable.hpp>
#include <SFML/Graphics/Vertex.hpp>

#include <array>

class DrawRectangle final : public sf::Drawable {
public:

    DrawRectangle();

	DrawRectangle
        (float x_, float y_, float w_ = 0.f, float h_ = 0.f,
		 sf::Color clr_ = sf::Color::White);
	
	virtual ~DrawRectangle(){}

    void set_x(float x_);
	
    void set_y(float y_);
	
    void set_width(float w);
	
    void set_height(float h);
	
    void set_position(float x_, float y_);
	
    void set_size(float w, float h);
	
    float width() const;
	
    float height() const;
	
    float x() const;
	
    float y() const;
	
    sf::Color color() const;
	
    void set_color(sf::Color clr)
        { for (sf::Vertex & vtx : m_vertices) vtx.color = clr; }

protected:

    void draw(sf::RenderTarget & target, sf::RenderStates) const override;

private:

    std::array<sf::Vertex, 4> m_vertices;
};

#endif
