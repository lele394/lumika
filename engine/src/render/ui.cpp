#include "e3d/render/ui.hpp"

#include "e3d/render/render_list.hpp"
#include "e3d/render/text.hpp"
#include "e3d/scene/components.hpp"
#include "e3d/scene/world.hpp"

namespace e3d {

const UiCanvas* ui_canvas_of(const World& world, const Entity& element) {
    EntityId parent = element.parent;
    while (parent != kInvalidEntity) {
        const Entity* entity = world.entity(parent);
        if (entity == nullptr) return nullptr;
        if (const UiCanvas* canvas = entity->find_component<UiCanvas>(); canvas != nullptr) {
            return canvas;
        }
        parent = entity->parent;
    }
    return nullptr;
}

void build_ui_render_list(const World& world, RenderList& list) {
    for (const auto& slot : world.entities()) {
        if (slot == nullptr) continue;

        if (const UiText* text = slot->find_component<UiText>(); text != nullptr) {
            if (!text->visible || text->text.empty() || text->scale <= 0.0f) continue;
            const UiCanvas* canvas = ui_canvas_of(world, *slot);
            if (canvas == nullptr || !canvas->font_texture.valid()) continue;

            Font font;
            font.texture = canvas->font_texture;
            font.texture_width = static_cast<u16>(canvas->atlas_width);
            font.texture_height = static_cast<u16>(canvas->atlas_height);
            font.glyph_width = static_cast<u16>(canvas->glyph_width);
            font.glyph_height = static_cast<u16>(canvas->glyph_height);
            font.columns = static_cast<u16>(canvas->columns);
            font.first_char = static_cast<u16>(canvas->first_char);

            const Screen screen =
                canvas->screen == ScreenTarget::Top ? Screen::Top : Screen::Bottom;
            draw_text(list, font, screen, text->x, text->y, text->scale, text->color, text->text);
        }

        if (const UiImage* image = slot->find_component<UiImage>(); image != nullptr) {
            if (!image->visible || !image->texture_handle.valid()) continue;
            const UiCanvas* canvas = ui_canvas_of(world, *slot);
            if (canvas == nullptr) continue;

            SpriteItem sprite;
            sprite.screen = canvas->screen == ScreenTarget::Top ? Screen::Top : Screen::Bottom;
            sprite.texture = image->texture_handle;
            sprite.x = image->x;
            sprite.y = image->y;
            sprite.width = image->width;
            sprite.height = image->height;
            sprite.color = image->color;
            list.draw_sprite(sprite);
        }
    }
}

} // namespace e3d
