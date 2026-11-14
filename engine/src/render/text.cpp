#include "e3d/render/text.hpp"

namespace e3d {

void draw_text(RenderList& list, const Font& font, Screen screen, float x, float y, float scale,
               Color color, std::string_view text) {
    if (!font.texture.valid() || scale <= 0.0f) return;

    const float advance = static_cast<float>(font.glyph_width) * scale;
    const float width = advance;
    const float height = static_cast<float>(font.glyph_height) * scale;
    const float atlas_width = static_cast<float>(font.texture_width);
    const float atlas_height = static_cast<float>(font.texture_height);

    for (char character : text) {
        const unsigned code = static_cast<unsigned char>(character);
        if (code >= font.first_char) {
            const unsigned index = code - font.first_char;
            const unsigned column = index % font.columns;
            const unsigned row = index / font.columns;

            if (code != ' ') {
                SpriteItem sprite;
                sprite.screen = screen;
                sprite.texture = font.texture;
                sprite.x = x;
                sprite.y = y;
                sprite.width = width;
                sprite.height = height;
                sprite.u0 = static_cast<float>(column * font.glyph_width) / atlas_width;
                sprite.u1 = static_cast<float>((column + 1) * font.glyph_width) / atlas_width;
                sprite.v0 = 1.0f -
                            static_cast<float>((row + 1) * font.glyph_height) / atlas_height;
                sprite.v1 =
                    1.0f - static_cast<float>(row * font.glyph_height) / atlas_height;
                sprite.color = color;
                list.draw_sprite(sprite);
            }
        }

        x += advance;
    }
}

} // namespace e3d
