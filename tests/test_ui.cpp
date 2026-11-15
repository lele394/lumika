#include <doctest/doctest.h>

#include "e3d/render/render_list.hpp"
#include "e3d/render/ui.hpp"
#include "e3d/scene/components.hpp"
#include "e3d/scene/scene_io.hpp"
#include "e3d/scene/world.hpp"

using namespace e3d;

namespace {

EntityId make_canvas(World& world, const char* name, ScreenTarget screen) {
    const EntityId id = world.create_entity(name);
    UiCanvas* canvas = world.add_component<UiCanvas>(id);
    canvas->screen = screen;
    canvas->font_texture = TextureHandle{0, 1};  // non-zero index; no device needed
    return id;
}

EntityId find_by_name(const World& world, const char* name) {
    for (const auto& slot : world.entities()) {
        if (slot != nullptr && slot->name == name) return slot->id;
    }
    return kInvalidEntity;
}

} // namespace

TEST_CASE("ui text inherits its canvas screen and font") {
    register_builtin_components();

    World world;
    const EntityId top = make_canvas(world, "UiTop", ScreenTarget::Top);
    const EntityId top_text = world.create_entity("Score", top);
    UiText* score = world.add_component<UiText>(top_text);
    score->text = "SCORE";
    score->x = 8.0f;
    score->y = 12.0f;

    const EntityId bottom = make_canvas(world, "UiBottom", ScreenTarget::Bottom);
    const EntityId bottom_text = world.create_entity("Next", bottom);
    UiText* next = world.add_component<UiText>(bottom_text);
    next->text = "NEXT";

    RenderList list;
    build_ui_render_list(world, list);

    // "SCORE" and "NEXT" are five and four glyphs; no sprite for spaces.
    REQUIRE(list.sprites().size() == 9);
    CHECK(list.sprites()[0].screen == Screen::Top);
    CHECK(list.sprites()[0].x == doctest::Approx(8.0f));
    CHECK(list.sprites()[0].y == doctest::Approx(12.0f));
    CHECK(list.sprites().back().screen == Screen::Bottom);
}

TEST_CASE("hidden and element-less ui entities are not drawn") {
    register_builtin_components();

    World world;
    const EntityId canvas = make_canvas(world, "UiTop", ScreenTarget::Top);
    const EntityId hidden = world.create_entity("Hidden", canvas);
    UiText* text = world.add_component<UiText>(hidden);
    text->text = "HIDDEN";
    text->visible = false;

    EntityId orphan = world.create_entity("Orphan");
    UiText* orphan_text = world.add_component<UiText>(orphan);
    orphan_text->text = "ORPHAN";

    RenderList list;
    build_ui_render_list(world, list);
    CHECK(list.sprites().empty());

    text->visible = true;
    build_ui_render_list(world, list);
    CHECK(list.sprites().size() == 6);
}

TEST_CASE("ui canvas without a resolved font draws nothing") {
    register_builtin_components();

    World world;
    const EntityId canvas = world.create_entity("UiTop");
    world.add_component<UiCanvas>(canvas);  // font_texture left invalid
    const EntityId label = world.create_entity("Label", canvas);
    world.add_component<UiText>(label);

    RenderList list;
    build_ui_render_list(world, list);
    CHECK(list.sprites().empty());
}

TEST_CASE("ui text components round-trip through scene json") {
    register_builtin_components();

    World world;
    const EntityId canvas = make_canvas(world, "UiTop", ScreenTarget::Bottom);
    const EntityId label = world.create_entity("Label", canvas);
    UiText* text = world.add_component<UiText>(label);
    text->text = "HELLO";
    text->x = 42.0f;
    text->y = 7.0f;
    text->scale = 2.0f;

    std::string error;
    const std::string document = save_scene(world, error);
    REQUIRE_FALSE(document.empty());

    World loaded;
    const SceneLoadResult result = load_scene(loaded, document);
    REQUIRE(result.ok);

    const EntityId loaded_canvas = find_by_name(loaded, "UiTop");
    REQUIRE(loaded_canvas != kInvalidEntity);
    const UiCanvas* loaded_ui = loaded.find_component<UiCanvas>(loaded_canvas);
    REQUIRE(loaded_ui != nullptr);
    CHECK(loaded_ui->screen == ScreenTarget::Bottom);

    const EntityId loaded_label = find_by_name(loaded, "Label");
    REQUIRE(loaded_label != kInvalidEntity);
    const UiText* loaded_text = loaded.find_component<UiText>(loaded_label);
    REQUIRE(loaded_text != nullptr);
    CHECK(loaded_text->text == "HELLO");
    CHECK(loaded_text->x == doctest::Approx(42.0f));
    CHECK(loaded_text->y == doctest::Approx(7.0f));
    CHECK(loaded_text->scale == doctest::Approx(2.0f));
}
