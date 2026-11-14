#pragma once

#include "e3d/render/render_list.hpp"
#include "e3d/render/types.hpp"

#include <string_view>

namespace e3d {

/// Monospace bitmap font atlas. Glyphs are laid out in a grid of cells.
struct Font final {
    TextureHandle texture;
    u16 texture_width = 128;
    u16 texture_height = 128;
    u16 glyph_width = 8;
    u16 glyph_height = 12;
    u16 columns = 16;
    u16 first_char = 32;
};

/// Appends one sprite per visible glyph. Space advances without drawing.
void draw_text(RenderList& list, const Font& font, Screen screen, float x, float y, float scale,
               Color color, std::string_view text);

} // namespace e3d
