/****************************************************************************

    File: DrawRectangle.cpp
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

#include "DrawRectangle.hpp"

#include <SFML/Graphics.hpp>

DrawRectangle::DrawRectangle
	(float x_, float y_, float w_, float h_, sf::Color clr_)
{
	set_position(x_, y_);
	set_size(w_, h_);
	set_color(clr_);
}

void DrawRectangle::set_x(float x_) {
    set_position(x_, y());
}

void DrawRectangle::set_y(float y_) {
    set_position(x(), y_);
}

void DrawRectangle::set_position(float x_, float y_) {
	// save old width and height
	float w = width(), h = height();
	
	// position
    for (sf::Vertex & vtx : m_vertices)
        vtx.position = sf::Vector2f(x_, y_);
	
    // impl detail, must reset size, since we erased it with overwriting each
    // of the Quad's points
    set_width(w);
    set_height(h);
}

void DrawRectangle::set_size(float w, float h) {
	// impl detail, position accessors x() and y() only access the first
	// vertex, which this function does not change
	set_width(w);
	set_height(h);
}

void DrawRectangle::set_width(float w) {
	// clear width, add new width
    m_vertices[1].position.x = x() + w;
    m_vertices[2].position.x = x() + w;
}

void DrawRectangle::set_height(float h) {
	// clear height, and new height
    m_vertices[2].position.y = y() + h;
    m_vertices[3].position.y = y() + h;
}

float DrawRectangle::width() const
    { return m_vertices[1].position.x - m_vertices[0].position.x; }

float DrawRectangle::height() const
    { return m_vertices[2].position.y - m_vertices[0].position.y; }

float DrawRectangle::x() const
    { return m_vertices[0].position.x; }

float DrawRectangle::y() const
    { return m_vertices[0].position.y; }

sf::Color DrawRectangle::color() const
    { return m_vertices[0].color; }

/* virtual protected */ void DrawRectangle::draw
	(sf::RenderTarget & target, sf::RenderStates) const
{
    target.draw(&m_vertices[0], m_vertices.size(), sf::Quads);
}
