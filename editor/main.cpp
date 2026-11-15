#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "e3d/e3d.hpp"
#include "e3d/assets/asset_manager.hpp"
#include "e3d/assets/directory_file_system.hpp"
#include "e3d/platform/desktop/render_device_gl.hpp"
#include "project_scripts.hpp"

namespace fs = std::filesystem;
using namespace e3d;

namespace {

/// Offscreen color+depth target displayed in an ImGui window.
struct RenderTarget final {
    GLuint framebuffer = 0;
    GLuint color = 0;
    GLuint depth = 0;
    int width = 0;
    int height = 0;

    void ensure(int w, int h) {
        if (framebuffer == 0) {
            glGenFramebuffers(1, &framebuffer);
            glGenTextures(1, &color);
            glGenRenderbuffers(1, &depth);
        }
        if (w == width && h == height) return;
        width = w;
        height = h;

        glBindTexture(GL_TEXTURE_2D, color);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindRenderbuffer(GL_RENDERBUFFER, depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);

        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    [[nodiscard]] ImTextureID texture_id() const noexcept {
        return static_cast<ImTextureID>(static_cast<uintptr_t>(color));
    }
};

std::vector<std::string>& log_lines() {
    static std::vector<std::string> lines;
    return lines;
}

void log_sink(LogLevel level, const char* text) {
    static const char* const names[] = {"D", "I", "W", "E"};
    std::string line = "[";
    line += names[static_cast<int>(level)];
    line += "] ";
    line += text;
    log_lines().push_back(std::move(line));
    if (log_lines().size() > 400) log_lines().erase(log_lines().begin());
}

std::string read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool write_file(const std::string& path, const std::string& text) {
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;
    file << text;
    return true;
}

std::string base_name(const std::string& path) {
    return fs::path(path).filename().string();
}

bool fuzzy_match(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) return true;
    std::size_t j = 0;
    for (char c : haystack) {
        if (std::tolower(static_cast<unsigned char>(c)) ==
            std::tolower(static_cast<unsigned char>(needle[j]))) {
            if (++j == needle.size()) return true;
        }
    }
    return false;
}

const char* asset_type_name(AssetType type) {
    switch (type) {
        case AssetType::Mesh: return "mesh";
        case AssetType::Texture: return "texture";
        case AssetType::Material: return "material";
        case AssetType::Audio: return "audio";
        case AssetType::Font: return "font";
        case AssetType::Prefab: return "prefab";
        default: return "unknown";
    }
}

std::atomic<int> g_play_state{0};  // 0 idle, 1 building, 2 launching, 3 ok, 4 failed
std::string g_play_error;

bool asset_type_matches(AssetType type, const char* filter) {
    if (filter == nullptr) return true;
    return std::strcmp(asset_type_name(type), filter) == 0;
}

struct PrefabFile final {
    std::string name;
    std::string path;
};

struct Editor final {
    std::string root = LUMIKA_ROOT;
    std::string projectDir;
    std::string scenePath;
    std::string assetsDir;
    std::string prefabsDir;

    World world;
    AssetDb database;
    AssetDb renderDatabase;
    std::unique_ptr<DirectoryFileSystem> assetFiles;
    std::unique_ptr<AssetManager> assetManager;
    std::unique_ptr<RenderDevice> render;

    EntityId selected = kInvalidEntity;
    bool dirty = false;
    bool project_loaded = false;
    bool layout_built = false;
    bool layout_from_ini = false;

    Vec3 cameraPosition{0.0f, 2.0f, 28.0f};
    float cameraYaw = -90.0f;
    float cameraPitch = 0.0f;

    RenderTarget scene_target;
    RenderTarget top_target;
    RenderTarget bottom_target;
    RenderTarget ui_top_target;
    RenderTarget ui_bottom_target;

    MeshHandle grid_mesh;
    MaterialHandle grid_material;
    bool grid_ready = false;
    bool show_camera_gizmos = true;

    ScreenTarget ui_canvas = ScreenTarget::Top;
    EntityId ui_drag_entity = kInvalidEntity;
    Vec2 ui_drag_mouse{};
    float ui_drag_x = 0.0f;
    float ui_drag_y = 0.0f;

    // Play-in-editor: a serialized copy of the scene with scripts running.
    World play_world;
    InputState play_input;
    RenderList play_list;
    std::unique_ptr<Runtime> play_runtime;
    bool playing = false;

    int dragAxis = -1;
    Vec3 dragStart{};
    ImVec2 dragMouse{};

    char search[128] = {};
    char new_entity_name[64] = "GameObject";
    char prefab_name[64] = "new_prefab";
    int asset_filter = 0;
    std::string tag_filter = "all tags";
    std::string status = "ready";
    std::vector<PrefabFile> prefabs;

    bool load_project(const std::string& dir) {
        stop_play();
        projectDir = dir;
        scenePath = (fs::path(dir) / "scenes" / "main.scene.json").string();
        assetsDir = (fs::path(dir) / "assets").string();
        prefabsDir = (fs::path(assetsDir) / "prefabs").string();

        world.clear();
        selected = kInvalidEntity;
        dirty = false;

        database = AssetDb{};
        const std::string manifest = read_file((fs::path(assetsDir) / "manifest.json").string());
        std::string error;
        if (!manifest.empty()) database.load_manifest(manifest, error);

        const std::string scene = read_file(scenePath);
        const SceneLoadResult loaded = load_scene(world, scene);
        project_loaded = loaded.ok;
        if (!loaded.ok) {
            status = "load failed: " + loaded.error;
            return false;
        }

        if (render) {
            const std::string cookedDir = (fs::path(assetsDir) / "cooked").string();
            const std::string cookedManifest =
                read_file((fs::path(cookedDir) / "manifest.json").string());
            renderDatabase = AssetDb{};
            if (!cookedManifest.empty()) renderDatabase.load_manifest(cookedManifest, error);

            assetFiles = std::make_unique<DirectoryFileSystem>(cookedDir);
            assetManager = std::make_unique<AssetManager>(renderDatabase, *assetFiles, *render, nullptr);

            std::vector<std::string> warnings;
            resolve_scene_assets(world, *assetManager, warnings);
            for (const std::string& warning : warnings) log_lines().push_back("[asset] " + warning);
        }

        selected = loaded.root;
        status = "loaded " + base_name(scenePath);
        scan_prefabs();
        return true;
    }

    void scan_prefabs() {
        prefabs.clear();
        std::error_code code;
        if (!fs::is_directory(prefabsDir, code)) return;
        for (const auto& entry : fs::directory_iterator(prefabsDir, code)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() != ".json") continue;
            PrefabFile prefab;
            prefab.name = entry.path().stem().string();
            const std::string suffix = ".prefab";
            if (prefab.name.size() > suffix.size() &&
                prefab.name.compare(prefab.name.size() - suffix.size(), suffix.size(), suffix) == 0) {
                prefab.name.resize(prefab.name.size() - suffix.size());
            }
            prefab.path = entry.path().string();
            prefabs.push_back(std::move(prefab));
        }
    }

    bool save_scene() {
        std::string error;
        const std::string text = e3d::save_scene(world, error);
        if (text.empty()) {
            status = "save failed: " + error;
            return false;
        }
        if (!write_file(scenePath, text)) {
            status = "cannot write " + scenePath;
            return false;
        }
        dirty = false;
        status = "saved " + base_name(scenePath);
        return true;
    }

    void create_prefab() {
        if (selected == kInvalidEntity) {
            status = "select an entity first";
            return;
        }
        const std::string text = save_entity_json(world, selected);
        if (text.empty()) {
            status = "prefab serialization failed";
            return;
        }
        std::error_code code;
        fs::create_directories(prefabsDir, code);
        const std::string path =
            (fs::path(prefabsDir) / (std::string(prefab_name) + ".prefab.json")).string();
        if (!write_file(path, text)) {
            status = "cannot write prefab";
            return;
        }
        scan_prefabs();
        status = "created prefab " + std::string(prefab_name);
    }

    void instantiate(const PrefabFile& prefab) {
        const std::string text = read_file(prefab.path);
        if (text.empty()) {
            status = "cannot read prefab";
            return;
        }
        std::string error;
        const EntityId parent = selected != kInvalidEntity ? selected : world.root();
        const EntityId created = e3d::instantiate_prefab(world, text, parent, error);
        if (created == kInvalidEntity) {
            status = "prefab failed: " + error;
            return;
        }
        selected = created;
        dirty = true;
        status = "instantiated " + prefab.name;
    }

    void new_entity() {
        const EntityId parent = selected != kInvalidEntity ? selected : world.root();
        selected = world.create_entity(new_entity_name, parent);
        dirty = true;
        status = "created " + std::string(new_entity_name);
    }

    void delete_entity() {
        if (selected == kInvalidEntity) return;
        world.destroy_entity(selected);
        selected = kInvalidEntity;
        dirty = true;
        status = "entity deleted";
    }

    void start_playtest() {
        if (g_play_state.load() == 1 || g_play_state.load() == 2) return;
        g_play_state = 1;
        const std::string rootDir = root;
        std::thread([rootDir]() {
            const std::string build =
                "powershell -NoProfile -ExecutionPolicy Bypass -File \"" + rootDir +
                "\\tools\\build.ps1\" -Target 3ds";
            if (std::system(build.c_str()) != 0) {
                g_play_state = 4;
                return;
            }
            g_play_state = 2;
            const std::string run =
                "powershell -NoProfile -ExecutionPolicy Bypass -File \"" + rootDir +
                "\\tools\\run_azahar.ps1\" -Rom \"" + rootDir +
                "\\build\\3ds\\projects\\demo_project\\demo3d.3dsx\"";
            std::system(run.c_str());
            g_play_state = 3;
        }).detach();
    }

    void draw_entity_node(EntityId id) {
        Entity* entity = world.entity(id);
        if (entity == nullptr) return;

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                   ImGuiTreeNodeFlags_SpanAvailWidth |
                                   ImGuiTreeNodeFlags_DefaultOpen;
        if (entity->children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
        if (id == selected) flags |= ImGuiTreeNodeFlags_Selected;

        const bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uintptr_t>(id)),
                                            flags, "%s", entity->name.c_str());
        if (ImGui::IsItemClicked()) selected = id;

        if (open) {
            for (EntityId child : entity->children) draw_entity_node(child);
            ImGui::TreePop();
        }
    }

    void draw_hierarchy() {
        if (!ImGui::Begin("Hierarchy")) {
            ImGui::End();
            return;
        }

        ImGui::InputText("##new_name", new_entity_name, sizeof(new_entity_name));
        ImGui::SameLine();
        if (ImGui::Button("New")) new_entity();
        ImGui::SameLine();
        if (ImGui::Button("Delete")) delete_entity();

        ImGui::Separator();
        if (world.entities().empty()) {
            ImGui::TextDisabled("(no entities)");
        } else {
            for (const auto& slot : world.entities()) {
                if (slot != nullptr && slot->parent == kInvalidEntity) {
                    draw_entity_node(slot->id);
                }
            }
        }
        ImGui::End();
    }

    void draw_property(Component& component, const PropDesc& prop) {
        void* memory = prop.get(&component);
        if (memory == nullptr) return;

        bool changed = false;
        ImGui::PushID(prop.name);

        switch (prop.type) {
            case PropType::Bool:
                changed = ImGui::Checkbox(prop.name, static_cast<bool*>(memory));
                break;
            case PropType::Int:
                changed = ImGui::DragInt(prop.name, static_cast<int*>(memory), 1.0f,
                                         static_cast<int>(prop.meta.min),
                                         static_cast<int>(prop.meta.max));
                break;
            case PropType::Float:
                changed = ImGui::DragFloat(prop.name, static_cast<float*>(memory), prop.meta.step,
                                           prop.meta.min, prop.meta.max);
                break;
            case PropType::Vec2:
                changed = ImGui::DragFloat2(prop.name, &static_cast<Vec2*>(memory)->x,
                                            prop.meta.step, prop.meta.min, prop.meta.max);
                break;
            case PropType::Vec3:
                changed = ImGui::DragFloat3(prop.name, &static_cast<Vec3*>(memory)->x,
                                            prop.meta.step, prop.meta.min, prop.meta.max);
                break;
            case PropType::Vec4:
                changed = ImGui::DragFloat4(prop.name, &static_cast<Vec4*>(memory)->x,
                                            prop.meta.step, prop.meta.min, prop.meta.max);
                break;
            case PropType::Color:
                changed = ImGui::ColorEdit4(prop.name, &static_cast<Color*>(memory)->r);
                break;
            case PropType::Enum: {
                int value = 0;
                std::memcpy(&value, memory, prop.size);
                if (prop.meta.enum_names != nullptr) {
                    if (ImGui::BeginCombo(prop.name, prop.meta.enum_names[value])) {
                        for (int i = 0; prop.meta.enum_names[i] != nullptr; ++i) {
                            if (ImGui::Selectable(prop.meta.enum_names[i], i == value)) {
                                value = i;
                                std::memcpy(memory, &value, prop.size);
                                changed = true;
                            }
                        }
                        ImGui::EndCombo();
                    }
                } else {
                    changed = ImGui::DragInt(prop.name, &value);
                    if (changed) std::memcpy(memory, &value, prop.size);
                }
                break;
            }
            case PropType::String: {
                auto* text = static_cast<std::string*>(memory);
                char buffer[256];
                std::strncpy(buffer, text->c_str(), sizeof(buffer) - 1);
                buffer[sizeof(buffer) - 1] = '\0';
                if (ImGui::InputText(prop.name, buffer, sizeof(buffer))) {
                    *text = buffer;
                    changed = true;
                }
                break;
            }
            case PropType::AssetRef: {
                auto* reference = static_cast<AssetRef*>(memory);
                const std::string current =
                    reference->guid.valid() ? guid_to_string(reference->guid) : "(none)";
                if (ImGui::BeginCombo(prop.name, current.c_str())) {
                    for (const AssetEntry& entry : database.entries()) {
                        if (!asset_type_matches(entry.type, prop.meta.asset_type)) continue;
                        const std::string label =
                            base_name(entry.path) + "  [" + asset_type_name(entry.type) + "]";
                        if (ImGui::Selectable(label.c_str())) {
                            reference->guid = entry.guid;
                            changed = true;
                        }
                    }
                    ImGui::EndCombo();
                }
                break;
            }
            case PropType::EntityRef: {
                auto* reference = static_cast<EntityRef*>(memory);
                const std::string current =
                    reference->guid.valid() ? guid_to_string(reference->guid) : "(none)";
                if (ImGui::BeginCombo(prop.name, current.c_str())) {
                    for (const auto& slot : world.entities()) {
                        if (slot == nullptr) continue;
                        if (ImGui::Selectable(slot->name.c_str())) {
                            reference->guid = slot->guid;
                            reference->target = slot->id;
                            changed = true;
                        }
                    }
                    ImGui::EndCombo();
                }
                break;
            }
        }

        if (prop.meta.tooltip != nullptr && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", prop.meta.tooltip);
        }

        ImGui::PopID();
        if (changed) dirty = true;
    }

    void draw_inspector() {
        if (!ImGui::Begin("Inspector")) {
            ImGui::End();
            return;
        }

        Entity* entity = world.entity(selected);
        if (entity == nullptr) {
            ImGui::TextDisabled("Select an entity");
            ImGui::End();
            return;
        }

        char name[128];
        std::strncpy(name, entity->name.c_str(), sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        if (ImGui::InputText("Name", name, sizeof(name))) {
            entity->name = name;
            dirty = true;
        }

        ImGui::SeparatorText("Transform");
        if (ImGui::DragFloat3("Position", &entity->transform.position.x, 0.05f)) dirty = true;
        if (ImGui::DragFloat3("Rotation", &entity->transform.rotation.x, 0.5f)) dirty = true;
        if (ImGui::DragFloat3("Scale", &entity->transform.scale.x, 0.05f)) dirty = true;

        ImGui::SeparatorText("Components");
        int index = 0;
        for (Component* component : entity->components) {
            const ComponentDesc* descriptor = component->descriptor();
            if (descriptor == nullptr) continue;

            ImGui::PushID(index++);
            if (ImGui::CollapsingHeader(descriptor->name, ImGuiTreeNodeFlags_DefaultOpen)) {
                for (u32 i = 0; i < descriptor->prop_count; ++i) {
                    draw_property(*component, descriptor->props[i]);
                }
                if (ImGui::SmallButton("Remove component")) {
                    world.remove_component(component);
                    dirty = true;
                    ImGui::PopID();
                    break;
                }
            }
            ImGui::PopID();
        }

        if (ImGui::BeginCombo("Add Component", "choose...")) {
            for (const ComponentDesc* descriptor : all_components()) {
                if (entity->find_component(descriptor->id) != nullptr) continue;
                if (ImGui::Selectable(descriptor->name)) {
                    world.add_component(selected, *descriptor);
                    dirty = true;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::End();
    }

    void draw_assets() {
        if (!ImGui::Begin("Assets")) {
            ImGui::End();
            return;
        }

        ImGui::InputTextWithHint("##search", "fuzzy search name, path or tag", search,
                                 sizeof(search));

        static const char* const kFilters[] = {"all",     "mesh",  "texture", "material",
                                               "audio",   "font",  "prefab"};
        ImGui::Combo("##filter", &asset_filter, kFilters, IM_ARRAYSIZE(kFilters));

        ImGui::SameLine();
        if (ImGui::BeginCombo("##tag", tag_filter.c_str())) {
            if (ImGui::Selectable("all tags", tag_filter == "all tags")) tag_filter = "all tags";
            std::vector<std::string> tags;
            for (const AssetEntry& entry : database.entries()) {
                for (const std::string& tag : entry.tags) {
                    if (std::find(tags.begin(), tags.end(), tag) == tags.end()) tags.push_back(tag);
                }
            }
            std::sort(tags.begin(), tags.end());
            for (const std::string& tag : tags) {
                if (ImGui::Selectable(("#" + tag).c_str(), tag_filter == tag)) tag_filter = tag;
            }
            ImGui::EndCombo();
        }

        ImGui::SeparatorText("Indexed assets");
        for (const AssetEntry& entry : database.entries()) {
            const bool type_ok =
                asset_filter == 0 || std::strcmp(asset_type_name(entry.type), kFilters[asset_filter]) == 0;
            if (!type_ok) continue;

            if (tag_filter != "all tags") {
                const bool has_tag =
                    std::find(entry.tags.begin(), entry.tags.end(), tag_filter) != entry.tags.end();
                if (!has_tag) continue;
            }

            std::string haystack = entry.path + " " + asset_type_name(entry.type);
            for (const std::string& tag : entry.tags) haystack += " " + tag;
            if (!fuzzy_match(haystack, search)) continue;

            std::string label = base_name(entry.path) + "  [" + asset_type_name(entry.type) + "]";
            for (const std::string& tag : entry.tags) label += " #" + tag;
            ImGui::Selectable(label.c_str());
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", entry.path.c_str());
        }

        ImGui::SeparatorText("Prefabs");
        if (asset_filter == 0 || asset_filter == 6) {
            for (const PrefabFile& prefab : prefabs) {
                if (!fuzzy_match(prefab.name, search) && !fuzzy_match(prefab.path, search)) continue;
                ImGui::PushID(prefab.path.c_str());
                ImGui::Selectable(prefab.name.c_str());
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) instantiate(prefab);
                if (ImGui::BeginPopupContextItem("prefab_menu")) {
                    if (ImGui::MenuItem("Instantiate")) instantiate(prefab);
                    ImGui::EndPopup();
                }
                ImGui::PopID();
            }
        }

        ImGui::SeparatorText("Create prefab");
        ImGui::InputText("##prefab_name", prefab_name, sizeof(prefab_name));
        ImGui::SameLine();
        if (ImGui::Button("From selection")) create_prefab();

        ImGui::End();
    }

    void draw_console() {
        if (!ImGui::Begin("Console")) {
            ImGui::End();
            return;
        }
        ImGui::Text("Status: %s", status.c_str());
        if (ImGui::BeginChild("##log", ImVec2(0, 0), ImGuiChildFlags_Borders)) {
            for (const std::string& line : log_lines()) ImGui::TextUnformatted(line.c_str());
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();
        }
        ImGui::End();
    }

    Vec3 forward() const {
        const float yaw = cameraYaw * kDegToRad;
        const float pitch = cameraPitch * kDegToRad;
        return Vec3{std::cos(pitch) * std::cos(yaw), std::sin(pitch),
                    std::cos(pitch) * std::sin(yaw)};
    }

    bool project(Vec3 world, ImVec2 origin, ImVec2 size, ImVec2& out) const {
        const Vec3 f = forward();
        const Vec3 r = normalize(cross(f, Vec3{0, 1, 0}));
        const Vec3 u = cross(r, f);
        const Vec3 rel = world - cameraPosition;
        const float depth = dot(rel, f);
        if (depth <= 0.05f) return false;

        const float aspect = size.x / size.y;
        const float tan_half = std::tan(55.0f * kDegToRad * 0.5f);
        const float ndc_x = (dot(rel, r) / depth) / (tan_half * aspect);
        const float ndc_y = (dot(rel, u) / depth) / tan_half;

        out.x = origin.x + (ndc_x * 0.5f + 0.5f) * size.x;
        out.y = origin.y + (0.5f - ndc_y * 0.5f) * size.y;
        return true;
    }

    void pick_at(ImVec2 mouse, ImVec2 origin, ImVec2 size) {
        EntityId best = kInvalidEntity;
        float best_distance = 36.0f;

        for (const auto& slot : world.entities()) {
            if (slot == nullptr) continue;
            const MeshRenderer* renderer = slot->find_component<MeshRenderer>();
            const bool drawable = renderer != nullptr && renderer->mesh_handle.valid();
            const bool camera = slot->find_component<Camera>() != nullptr;
            if (!drawable && !camera) continue;

            ImVec2 screen;
            const Vec3 position{slot->world.m[12], slot->world.m[13], slot->world.m[14]};
            if (!project(position, origin, size, screen)) continue;
            const float dx = screen.x - mouse.x;
            const float dy = screen.y - mouse.y;
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance < best_distance) {
                best_distance = distance;
                best = slot->id;
            }
        }

        selected = best;
        status = best != kInvalidEntity ? "selected " + world.entity(best)->name
                                        : std::string("selection cleared");
    }

    MeshData build_grid() const {
        MeshData data;
        const float extent = 60.0f;
        const float half_width = 0.012f;

        auto add_quad = [&data](Vec3 a, Vec3 b, Vec3 c, Vec3 d, Color color) {
            const u16 base = static_cast<u16>(data.vertices.size());
            data.vertices.push_back(MeshVertex{a, color, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}});
            data.vertices.push_back(MeshVertex{b, color, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}});
            data.vertices.push_back(MeshVertex{c, color, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}});
            data.vertices.push_back(MeshVertex{d, color, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}});
            data.indices.insert(data.indices.end(),
                                {base, static_cast<u16>(base + 1), static_cast<u16>(base + 2),
                                 base, static_cast<u16>(base + 2), static_cast<u16>(base + 3)});
        };

        for (int i = -60; i <= 60; ++i) {
            if (i == 0) continue;
            const float p = static_cast<float>(i);
            const Color color = (i % 10 == 0) ? Color{0.32f, 0.34f, 0.42f, 0.85f}
                                              : Color{0.22f, 0.23f, 0.29f, 0.6f};
            add_quad(Vec3{p - half_width, 0.0f, -extent}, Vec3{p + half_width, 0.0f, -extent},
                     Vec3{p + half_width, 0.0f, extent}, Vec3{p - half_width, 0.0f, extent},
                     color);
            add_quad(Vec3{-extent, 0.0f, p - half_width}, Vec3{extent, 0.0f, p - half_width},
                     Vec3{extent, 0.0f, p + half_width}, Vec3{-extent, 0.0f, p + half_width},
                     color);
        }

        // World axes: X red, Z blue, slightly brighter than the grid.
        add_quad(Vec3{-extent, 0.0f, -half_width * 2.0f}, Vec3{extent, 0.0f, -half_width * 2.0f},
                 Vec3{extent, 0.0f, half_width * 2.0f}, Vec3{-extent, 0.0f, half_width * 2.0f},
                 Color{0.72f, 0.24f, 0.26f, 0.9f});
        add_quad(Vec3{-half_width * 2.0f, 0.0f, -extent}, Vec3{half_width * 2.0f, 0.0f, -extent},
                 Vec3{half_width * 2.0f, 0.0f, extent}, Vec3{-half_width * 2.0f, 0.0f, extent},
                 Color{0.26f, 0.42f, 0.78f, 0.9f});
        return data;
    }

    void ensure_grid() {
        if (grid_ready) return;
        grid_ready = true;
        grid_mesh = render->create_mesh(build_grid());
        MaterialDesc material;
        material.shader = ShaderId::UnlitColor;
        grid_material = render->create_material(material);
    }

    void start_play() {
        if (playing) return;

        std::string error;
        const std::string text = e3d::save_scene(world, error);
        if (text.empty()) {
            status = "play failed: " + error;
            return;
        }

        play_world.clear();
        const SceneLoadResult loaded = load_scene(play_world, text);
        if (!loaded.ok) {
            status = "play failed: " + loaded.error;
            return;
        }

        if (assetManager) {
            std::vector<std::string> warnings;
            resolve_scene_assets(play_world, *assetManager, warnings);
            for (const std::string& warning : warnings) log_lines().push_back("[play] " + warning);
        }

        play_input = InputState{};
        play_list.clear();

        ScriptContext& context = script_context();
        context.world = &play_world;
        context.input = &play_input;
        context.audio = nullptr;
        context.render_list = &play_list;
        context.render_device = render.get();
        context.delta_time = 0.0f;
        context.fixed_delta_time = 1.0f / 60.0f;

        play_runtime = std::make_unique<Runtime>(play_world, nullptr);
        play_runtime->set_render_list(&play_list);
        play_runtime->set_render_device(render.get());

        playing = true;
        status = "play: scripts running";
    }

    void stop_play() {
        if (!playing) return;
        shutdown_scripts(play_world);
        play_runtime.reset();
        play_world.clear();
        play_list.clear();
        playing = false;

        ScriptContext& context = script_context();
        context.world = &world;
        context.input = nullptr;
        context.audio = nullptr;
        context.render_list = nullptr;
        context.render_device = render.get();
        status = "play stopped";
    }

    /// Builds a render list for one screen camera of `source`, matching the
    /// runtime's camera/layer filtering so the preview matches the console.
    void build_screen_list(const World& source, ScreenTarget screen, RenderList& list) const {
        list.clear();

        const Environment* environment = nullptr;
        for (const auto& slot : source.entities()) {
            if (slot == nullptr) continue;
            if (const Environment* found = slot->find_component<Environment>(); found != nullptr) {
                environment = found;
                break;
            }
        }
        const Color clear = environment != nullptr ? environment->clear_color
                                                   : Color{0.04f, 0.05f, 0.08f, 1.0f};

        const Entity* camera_entity = nullptr;
        const Camera* camera = nullptr;
        for (const auto& slot : source.entities()) {
            if (slot == nullptr) continue;
            const Camera* found = slot->find_component<Camera>();
            if (found != nullptr && found->screen == screen) {
                camera_entity = slot.get();
                camera = found;
                break;
            }
        }
        if (camera == nullptr) return;

        CameraDesc desc;
        desc.screen = camera->screen == ScreenTarget::Top ? Screen::Top : Screen::Bottom;
        desc.field_of_view = camera->field_of_view;
        desc.near_plane = camera->near_plane;
        desc.far_plane = camera->far_plane;
        desc.stereo = camera->stereo;
        desc.cull_mask = static_cast<u32>(camera->cull_mask);
        desc.clear = clear;

        const Mat4& matrix = camera_entity->world;
        desc.position = Vec3{matrix.m[12], matrix.m[13], matrix.m[14]};
        desc.up = normalize(Vec3{matrix.m[4], matrix.m[5], matrix.m[6]});
        desc.target = desc.position - normalize(Vec3{matrix.m[8], matrix.m[9], matrix.m[10]});

        const u8 index = list.add_camera(desc);
        const u32 mask = desc.cull_mask;

        for (const auto& slot : source.entities()) {
            if (slot == nullptr) continue;
            const MeshRenderer* renderer = slot->find_component<MeshRenderer>();
            if (renderer == nullptr) continue;
            if (!renderer->mesh_handle.valid() || !renderer->material_handle.valid()) continue;

            const u32 layer = static_cast<u32>(renderer->layer) & 31u;
            if ((mask & (1u << layer)) == 0) continue;
            list.draw(index, renderer->mesh_handle, renderer->material_handle, slot->world);
        }
    }

    void draw_camera_gizmos(ImDrawList* draw_list, ImVec2 origin, ImVec2 size) {
        for (const auto& slot : world.entities()) {
            if (slot == nullptr) continue;
            const Camera* camera = slot->find_component<Camera>();
            if (camera == nullptr) continue;

            const Mat4& matrix = slot->world;
            const Vec3 position{matrix.m[12], matrix.m[13], matrix.m[14]};
            const Vec3 right = normalize(Vec3{matrix.m[0], matrix.m[1], matrix.m[2]});
            const Vec3 up = normalize(Vec3{matrix.m[4], matrix.m[5], matrix.m[6]});
            const Vec3 forward = normalize(Vec3{matrix.m[8], matrix.m[9], matrix.m[10]}) * -1.0f;

            const float aspect =
                camera->screen == ScreenTarget::Top ? 400.0f / 240.0f : 320.0f / 240.0f;
            const float near_half_h = std::tan(camera->field_of_view * kDegToRad * 0.5f) *
                                      camera->near_plane;
            const float near_half_w = near_half_h * aspect;
            const float far_half_h = std::tan(camera->field_of_view * kDegToRad * 0.5f) *
                                     camera->far_plane;
            const float far_half_w = far_half_h * aspect;

            Vec3 near_corners[4];
            Vec3 far_corners[4];
            const Vec3 near_center = position + forward * camera->near_plane;
            const Vec3 far_center = position + forward * camera->far_plane;
            const int signs[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
            for (int i = 0; i < 4; ++i) {
                const Vec3 offset = right * (signs[i][0] * near_half_w) +
                                    up * (signs[i][1] * near_half_h);
                near_corners[i] = near_center + offset;
                far_corners[i] = far_center +
                                 right * (signs[i][0] * far_half_w) +
                                 up * (signs[i][1] * far_half_h);
            }

            const bool is_selected = slot->id == selected;
            const ImU32 color = is_selected ? IM_COL32(255, 208, 80, 220)
                                            : IM_COL32(140, 155, 180, 120);
            const ImU32 far_color = is_selected ? IM_COL32(255, 208, 80, 120)
                                                : IM_COL32(140, 155, 180, 55);

            auto line = [&](Vec3 a, Vec3 b, ImU32 line_color, float thickness) {
                ImVec2 pa;
                ImVec2 pb;
                if (!project(a, origin, size, pa) || !project(b, origin, size, pb)) return;
                draw_list->AddLine(pa, pb, line_color, thickness);
            };

            // Near rectangle, far rectangle and the four frustum edges.
            for (int i = 0; i < 4; ++i) {
                line(near_corners[i], near_corners[(i + 1) % 4], color, 1.6f);
                line(far_corners[i], far_corners[(i + 1) % 4], far_color, 1.2f);
                line(near_corners[i], far_corners[i], far_color, 1.0f);
            }

            // Small camera body: a square facing the lens plus an up marker.
            const float body = 0.55f;
            const Vec3 body_center = position + forward * -0.35f;
            Vec3 body_corners[4];
            for (int i = 0; i < 4; ++i) {
                body_corners[i] = body_center + right * (signs[i][0] * body * 0.5f) +
                                  up * (signs[i][1] * body * 0.5f);
            }
            for (int i = 0; i < 4; ++i) {
                line(body_corners[i], body_corners[(i + 1) % 4], color, 1.6f);
                line(body_corners[i], near_corners[(i + 1) % 4], color, 1.0f);
            }
            line(body_center + up * 0.5f, body_center + up * 1.1f, color, 1.6f);

            ImVec2 label_pos;
            if (project(body_center + up * 1.35f, origin, size, label_pos)) {
                const ImVec2 text_size = ImGui::CalcTextSize(slot->name.c_str());
                draw_list->AddRectFilled(
                    ImVec2(label_pos.x - 3.0f, label_pos.y - 2.0f),
                    ImVec2(label_pos.x + text_size.x + 3.0f, label_pos.y + text_size.y + 2.0f),
                    IM_COL32(10, 12, 18, 190), 3.0f);
                draw_list->AddText(label_pos, color, slot->name.c_str());
            }
        }
    }

    void draw_scene() {
        if (!ImGui::Begin("Scene")) {
            ImGui::End();
            return;
        }

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const int width = std::max(64, static_cast<int>(avail.x));
        const int height = std::max(64, static_cast<int>(avail.y));

        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##scene_canvas", ImVec2(static_cast<float>(width),
                                                        static_cast<float>(height)),
                               ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

        const bool hovered = ImGui::IsItemHovered();
        ImGuiIO& io = ImGui::GetIO();
        const ImVec2 size(static_cast<float>(width), static_cast<float>(height));

        static bool looking = false;
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            looking = true;
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
            looking = false;
        }
        if (looking) {
            cameraYaw += io.MouseDelta.x * 0.16f;
            cameraPitch -= io.MouseDelta.y * 0.16f;
            if (cameraPitch > 89.0f) cameraPitch = 89.0f;
            if (cameraPitch < -89.0f) cameraPitch = -89.0f;
            ImGui::SetMouseCursor(ImGuiMouseCursor_None);
        }

        const bool can_move = (hovered || looking) && !io.WantTextInput;
        if (can_move) {
            const float speed = (io.KeyShift ? 36.0f : 12.0f) * io.DeltaTime;
            const Vec3 f = forward();
            const Vec3 r = normalize(cross(f, Vec3{0, 1, 0}));
            if (ImGui::IsKeyDown(ImGuiKey_W) || ImGui::IsKeyDown(ImGuiKey_UpArrow)) cameraPosition += f * speed;
            if (ImGui::IsKeyDown(ImGuiKey_S) || ImGui::IsKeyDown(ImGuiKey_DownArrow)) cameraPosition = cameraPosition - f * speed;
            if (ImGui::IsKeyDown(ImGuiKey_A) || ImGui::IsKeyDown(ImGuiKey_LeftArrow)) cameraPosition = cameraPosition - r * speed;
            if (ImGui::IsKeyDown(ImGuiKey_D) || ImGui::IsKeyDown(ImGuiKey_RightArrow)) cameraPosition += r * speed;
            if (ImGui::IsKeyDown(ImGuiKey_Q)) cameraPosition.y -= speed;
            if (ImGui::IsKeyDown(ImGuiKey_E)) cameraPosition.y += speed;
            if (io.MouseWheel != 0.0f) cameraPosition += f * (io.MouseWheel * 2.0f);

            if (ImGui::IsKeyPressed(ImGuiKey_F)) {
                Entity* target = world.entity(selected);
                if (target != nullptr) {
                    const Vec3 center{target->world.m[12], target->world.m[13], target->world.m[14]};
                    cameraPosition = center - forward() * 10.0f;
                    status = "focused " + target->name;
                }
            }
        }

        scene_target.ensure(width, height);
        ensure_grid();

        RenderList list;
        CameraDesc camera;
        camera.screen = Screen::Top;
        camera.position = cameraPosition;
        camera.target = cameraPosition + forward();
        camera.up = Vec3{0, 1, 0};
        camera.field_of_view = 55.0f;
        camera.near_plane = 0.05f;
        camera.far_plane = 500.0f;
        camera.clear = Color{0.055f, 0.06f, 0.085f, 1.0f};
        const u8 camera_index = list.add_camera(camera);

        if (grid_mesh.valid() && grid_material.valid()) {
            list.draw(camera_index, grid_mesh, grid_material, mat4_identity());
        }

        const World& view_world = playing ? play_world : world;
        for (const auto& slot : view_world.entities()) {
            if (slot == nullptr) continue;
            MeshRenderer* renderer = slot->find_component<MeshRenderer>();
            if (renderer == nullptr) continue;
            if (!renderer->mesh_handle.valid() || !renderer->material_handle.valid()) continue;
            list.draw(camera_index, renderer->mesh_handle, renderer->material_handle, slot->world);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, scene_target.framebuffer);
        render->set_viewport(width, height);
        render->execute(list);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddImage(scene_target.texture_id(), origin,
                            ImVec2(origin.x + size.x, origin.y + size.y), ImVec2(0, 1),
                            ImVec2(1, 0));
        draw_list->AddText(ImVec2(origin.x + 10.0f, origin.y + size.y - 22.0f),
                           IM_COL32(150, 160, 180, 170),
                           "RMB look  |  WASD / arrows move  |  Shift speed  |  F focus  |  LMB select + drag gizmo");

        if (show_camera_gizmos && !playing) {
            draw_camera_gizmos(draw_list, origin, size);
        }

        // Gizmo + click selection.
        Entity* selected_entity = playing ? nullptr : world.entity(selected);
        if (selected_entity != nullptr) {
            const Vec3 origin_world{selected_entity->world.m[12], selected_entity->world.m[13],
                                    selected_entity->world.m[14]};
            const Vec3 axes[3] = {Vec3{1, 0, 0}, Vec3{0, 1, 0}, Vec3{0, 0, 1}};
            const ImU32 colors[3] = {IM_COL32(230, 70, 70, 255), IM_COL32(80, 220, 100, 255),
                                     IM_COL32(80, 140, 255, 255)};
            ImVec2 points[3];

            for (int axis = 0; axis < 3; ++axis) {
                ImVec2 a;
                ImVec2 b;
                if (!project(origin_world, origin, size, a)) continue;
                if (!project(origin_world + axes[axis] * 1.6f, origin, size, b)) continue;
                points[axis] = b;
                draw_list->AddLine(a, b, colors[axis], 2.5f);
                draw_list->AddCircleFilled(b, 5.0f, colors[axis]);
            }

            if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && dragAxis < 0) {
                float best = 8.0f;
                for (int axis = 0; axis < 3; ++axis) {
                    ImVec2 a;
                    if (!project(origin_world, origin, size, a)) continue;
                    const ImVec2 ab(points[axis].x - a.x, points[axis].y - a.y);
                    const ImVec2 ap(io.MousePos.x - a.x, io.MousePos.y - a.y);
                    const float length2 = ab.x * ab.x + ab.y * ab.y;
                    if (length2 < 1.0f) continue;
                    float t = (ap.x * ab.x + ap.y * ab.y) / length2;
                    if (t < 0.0f) t = 0.0f;
                    if (t > 1.0f) t = 1.0f;
                    const float dx = a.x + ab.x * t - io.MousePos.x;
                    const float dy = a.y + ab.y * t - io.MousePos.y;
                    const float distance = std::sqrt(dx * dx + dy * dy);
                    if (distance < best) {
                        best = distance;
                        dragAxis = axis;
                        dragStart = selected_entity->transform.position;
                        dragMouse = io.MousePos;
                    }
                }
            }

            if (dragAxis >= 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                ImVec2 a;
                if (project(origin_world, origin, size, a)) {
                    const ImVec2 ab(points[dragAxis].x - a.x, points[dragAxis].y - a.y);
                    const float length2 = ab.x * ab.x + ab.y * ab.y;
                    if (length2 > 1.0f) {
                        const float delta =
                            ((io.MousePos.x - dragMouse.x) * ab.x +
                             (io.MousePos.y - dragMouse.y) * ab.y) /
                            length2;
                        selected_entity->transform.position =
                            dragStart + axes[dragAxis] * (delta * 1.6f);
                        dirty = true;
                    }
                }
            }
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) dragAxis = -1;
        }

        if (!playing && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && dragAxis < 0) {
            pick_at(io.MousePos, origin, size);
        }

        ImGui::End();
    }

    void draw_preview(RenderTarget& target, const World& source, ScreenTarget screen,
                      float available_width, float available_height) {
        constexpr int kScale = 2;
        const int native_width = screen == ScreenTarget::Top ? 400 : 320;
        const int native_height = 240;
        target.ensure(native_width * kScale, native_height * kScale);

        RenderList list;
        build_screen_list(source, screen, list);
        build_ui_render_list(source, list);

        if (!list.cameras().empty()) {
            glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
            render->set_viewport(target.width, target.height);
            render->execute(list);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        const float aspect = static_cast<float>(native_width) / static_cast<float>(native_height);
        float width = available_width;
        float height = width / aspect;
        if (height > available_height) {
            height = available_height;
            width = height * aspect;
        }
        if (width < 1.0f || height < 1.0f) return;

        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        const float x = cursor.x + (available_width - width) * 0.5f;
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRectFilled(ImVec2(x, cursor.y), ImVec2(x + width, cursor.y + height),
                                 IM_COL32(12, 14, 20, 255));

        ImGui::SetCursorScreenPos(ImVec2(x, cursor.y));
        ImGui::Image(target.texture_id(), ImVec2(width, height), ImVec2(0, 1), ImVec2(1, 0));
    }

    void draw_game_view() {
        if (!ImGui::Begin("Game")) {
            ImGui::End();
            return;
        }

        if (playing) {
            if (ImGui::Button("Stop")) stop_play();
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.45f, 0.9f, 0.55f, 1.0f), "PLAYING");
            ImGui::SameLine();
            ImGui::TextDisabled("scripts running on the editor copy");
        } else {
            if (ImGui::Button("Play")) start_play();
            ImGui::SameLine();
            ImGui::TextDisabled("authored scene - press Play to run scripts");
        }

        ImGui::Separator();

        const World& source = playing ? play_world : world;
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float label_height = ImGui::GetTextLineHeightWithSpacing();
        const float half =
            std::max(40.0f, (avail.y - label_height * 2.0f - 12.0f) * 0.5f - 6.0f);

        ImGui::Text("Top screen  400x240");
        draw_preview(top_target, source, ScreenTarget::Top, avail.x, half);
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::Text("Bottom screen  320x240");
        draw_preview(bottom_target, source, ScreenTarget::Bottom, avail.x, half);

        ImGui::End();
    }

    EntityId ui_canvas_entity(ScreenTarget screen) const {
        const char* name = screen == ScreenTarget::Top ? "UiTop" : "UiBottom";
        for (const auto& slot : world.entities()) {
            if (slot != nullptr && slot->name == name) return slot->id;
        }
        return kInvalidEntity;
    }

    void add_ui_text() {
        const EntityId canvas_id = ui_canvas_entity(ui_canvas);
        if (canvas_id == kInvalidEntity) {
            status = "add UiTop/UiBottom canvas first";
            return;
        }
        const EntityId entity = world.create_entity("Text", canvas_id);
        UiText* text = world.add_component<UiText>(entity);
        if (text == nullptr) return;
        text->text = "Text";
        text->x = 16.0f;
        text->y = 16.0f;
        selected = entity;
        dirty = true;
        status = "added UI text";
    }

    EntityId hit_ui_element(float x, float y, ScreenTarget screen) const {
        const auto& entities = world.entities();
        for (auto it = entities.rbegin(); it != entities.rend(); ++it) {
            const auto& slot = *it;
            if (slot == nullptr) continue;

            const UiCanvas* canvas = ui_canvas_of(world, *slot);
            if (canvas == nullptr || canvas->screen != screen) continue;

            float left = 0.0f;
            float top = 0.0f;
            float right = 0.0f;
            float bottom = 0.0f;

            if (const UiText* text = slot->find_component<UiText>(); text != nullptr) {
                if (!text->visible) continue;
                left = text->x;
                top = text->y;
                right = text->x + static_cast<float>(text->text.size()) *
                                      static_cast<float>(canvas->glyph_width) * text->scale;
                bottom = text->y + static_cast<float>(canvas->glyph_height) * text->scale;
            } else if (const UiImage* image = slot->find_component<UiImage>(); image != nullptr) {
                if (!image->visible) continue;
                left = image->x;
                top = image->y;
                right = image->x + image->width;
                bottom = image->y + image->height;
            } else {
                continue;
            }

            if (x >= left && x <= right && y >= top && y <= bottom) return slot->id;
        }
        return kInvalidEntity;
    }

    struct UiCanvasView final {
        ImVec2 origin;
        float scale = 1.0f;
        float native_width = 400.0f;
    };

    UiCanvasView draw_ui_canvas_image(RenderTarget& target, ScreenTarget screen,
                                      float available_width, float available_height) {
        const float native_width = screen == ScreenTarget::Top ? 400.0f : 320.0f;
        const float native_height = 240.0f;
        target.ensure(static_cast<int>(native_width) * 2, static_cast<int>(native_height) * 2);

        RenderList list;
        CameraDesc camera;
        camera.screen = screen == ScreenTarget::Top ? Screen::Top : Screen::Bottom;
        camera.position = Vec3{0.0f, 0.0f, 1.0f};
        camera.target = Vec3{0.0f, 0.0f, 0.0f};
        camera.field_of_view = 50.0f;
        camera.clear = Color{0.075f, 0.08f, 0.105f, 1.0f};
        list.add_camera(camera);
        build_ui_render_list(world, list);

        glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
        render->set_viewport(target.width, target.height);
        render->execute(list);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        float width = available_width;
        float height = width * native_height / native_width;
        if (height > available_height) {
            height = available_height;
            width = height * native_width / native_height;
        }

        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        const float x = cursor.x + (available_width - width) * 0.5f;
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRectFilled(ImVec2(x, cursor.y), ImVec2(x + width, cursor.y + height),
                                 IM_COL32(12, 14, 20, 255));
        ImGui::SetCursorScreenPos(ImVec2(x, cursor.y));
        ImGui::Image(target.texture_id(), ImVec2(width, height), ImVec2(0, 1), ImVec2(1, 0));
        draw_list->AddRect(ImVec2(x, cursor.y), ImVec2(x + width, cursor.y + height),
                           IM_COL32(70, 80, 100, 190));

        return UiCanvasView{ImVec2(x, cursor.y), width / native_width, native_width};
    }

    void ui_canvas_interaction(const UiCanvasView& view, ScreenTarget screen) {
        ImGuiIO& io = ImGui::GetIO();
        const ImVec2 canvas_end(view.origin.x + view.native_width * view.scale,
                                view.origin.y + 240.0f * view.scale);
        const bool hovered = io.MousePos.x >= view.origin.x && io.MousePos.x <= canvas_end.x &&
                             io.MousePos.y >= view.origin.y && io.MousePos.y <= canvas_end.y;

        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            const float x = (io.MousePos.x - view.origin.x) / view.scale;
            const float y = (io.MousePos.y - view.origin.y) / view.scale;
            const EntityId hit = hit_ui_element(x, y, screen);
            if (hit != kInvalidEntity) {
                selected = hit;
                ui_drag_entity = hit;
                ui_drag_mouse = Vec2{io.MousePos.x, io.MousePos.y};
                if (UiText* text = world.entity(hit)->find_component<UiText>(); text != nullptr) {
                    ui_drag_x = text->x;
                    ui_drag_y = text->y;
                    status = "selected UI text";
                } else if (UiImage* image = world.entity(hit)->find_component<UiImage>();
                           image != nullptr) {
                    ui_drag_x = image->x;
                    ui_drag_y = image->y;
                    status = "selected UI image";
                }
            }
        }

        if (ui_drag_entity != kInvalidEntity && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            Entity* entity = world.entity(ui_drag_entity);
            if (entity != nullptr) {
                const float dx = (io.MousePos.x - ui_drag_mouse.x) / view.scale;
                const float dy = (io.MousePos.y - ui_drag_mouse.y) / view.scale;
                if (UiText* text = entity->find_component<UiText>(); text != nullptr) {
                    text->x = std::round(ui_drag_x + dx);
                    text->y = std::round(ui_drag_y + dy);
                    dirty = true;
                } else if (UiImage* image = entity->find_component<UiImage>(); image != nullptr) {
                    image->x = std::round(ui_drag_x + dx);
                    image->y = std::round(ui_drag_y + dy);
                    dirty = true;
                }
            }
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) ui_drag_entity = kInvalidEntity;

        // Outline the selected element on this canvas.
        Entity* selected_entity = world.entity(selected);
        if (selected_entity != nullptr) {
            const UiCanvas* canvas = ui_canvas_of(world, *selected_entity);
            if (canvas != nullptr && canvas->screen == screen) {
                if (const UiText* text = selected_entity->find_component<UiText>();
                    text != nullptr && text->visible) {
                    const ImVec2 a(view.origin.x + text->x * view.scale,
                                   view.origin.y + text->y * view.scale);
                    const ImVec2 b(
                        a.x + static_cast<float>(text->text.size()) *
                                  static_cast<float>(canvas->glyph_width) * text->scale *
                                  view.scale,
                        a.y + static_cast<float>(canvas->glyph_height) * text->scale * view.scale);
                    ImGui::GetWindowDrawList()->AddRect(a, b, IM_COL32(255, 200, 80, 220), 0.0f,
                                                        0, 1.5f);
                } else if (const UiImage* image = selected_entity->find_component<UiImage>();
                           image != nullptr && image->visible) {
                    const ImVec2 a(view.origin.x + image->x * view.scale,
                                   view.origin.y + image->y * view.scale);
                    const ImVec2 b(a.x + image->width * view.scale,
                                   a.y + image->height * view.scale);
                    ImGui::GetWindowDrawList()->AddRect(a, b, IM_COL32(255, 200, 80, 220), 0.0f,
                                                        0, 1.5f);
                }
            }
        }
    }

    void draw_ui_designer() {
        if (!ImGui::Begin("UI")) {
            ImGui::End();
            return;
        }

        if (ImGui::Button("Add Text")) add_ui_text();
        ImGui::SameLine();

        const char* const canvas_names[] = {"Top", "Bottom"};
        int canvas_index = ui_canvas == ScreenTarget::Top ? 0 : 1;
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::Combo("##canvas", &canvas_index, canvas_names, 2)) {
            ui_canvas = canvas_index == 0 ? ScreenTarget::Top : ScreenTarget::Bottom;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("LMB select / drag  |  edit text and colors in Inspector");

        ImGui::Separator();

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float label_height = ImGui::GetTextLineHeightWithSpacing();
        const float half = std::max(52.0f, (avail.y - label_height * 2.0f - 16.0f) * 0.5f - 6.0f);

        ImGui::Text("Top screen  400x240");
        UiCanvasView top = draw_ui_canvas_image(ui_top_target, ScreenTarget::Top, avail.x, half);
        ui_canvas_interaction(top, ScreenTarget::Top);

        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::Text("Bottom screen  320x240");
        UiCanvasView bottom =
            draw_ui_canvas_image(ui_bottom_target, ScreenTarget::Bottom, avail.x, half);
        ui_canvas_interaction(bottom, ScreenTarget::Bottom);

        ImGui::End();
    }

    void draw_menu() {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Save Scene", "Ctrl+S")) save_scene();
                if (ImGui::MenuItem("Reload Project")) load_project(projectDir);
                ImGui::Separator();
                if (ImGui::MenuItem("Quit")) glfwSetWindowShouldClose(glfwGetCurrentContext(), 1);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                if (ImGui::MenuItem("Reset Layout")) {
                    layout_built = false;
                    layout_from_ini = false;
                }
                if (ImGui::MenuItem("Reset Camera")) {
                    cameraPosition = Vec3{0.0f, 2.0f, 28.0f};
                    cameraYaw = -90.0f;
                    cameraPitch = 0.0f;
                }
                if (ImGui::MenuItem("Frame Selected", "F")) {
                    Entity* target = world.entity(selected);
                    if (target != nullptr) {
                        const Vec3 center{target->world.m[12], target->world.m[13],
                                          target->world.m[14]};
                        cameraPosition = center - forward() * 10.0f;
                    }
                }
                ImGui::Separator();
                ImGui::MenuItem("Camera Gizmos", nullptr, &show_camera_gizmos);
                if (ImGui::MenuItem("Focus Scene View", "F7")) ImGui::SetWindowFocus("Scene");
                if (ImGui::MenuItem("Focus UI Designer", "F8")) ImGui::SetWindowFocus("UI");
                if (ImGui::MenuItem("Focus Game View", "F6")) ImGui::SetWindowFocus("Game");
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Build")) {
                if (ImGui::MenuItem(playing ? "Stop" : "Play (in editor)", "F5")) {
                    if (playing) {
                        stop_play();
                    } else {
                        start_play();
                    }
                }
                ImGui::Separator();
                const bool busy = g_play_state == 1 || g_play_state == 2;
                if (ImGui::MenuItem("Playtest (build + Azahar)", nullptr, false, !busy)) {
                    start_playtest();
                }
                ImGui::EndMenu();
            }

            const char* state = "idle";
            switch (g_play_state.load()) {
                case 1: state = "building..."; break;
                case 2: state = "launching Azahar..."; break;
                case 3: state = "launched"; break;
                case 4: state = "build failed"; break;
                default: state = "idle"; break;
            }
            ImGui::Text("  |  %s%s  |  Playtest: %s  |  %s", base_name(projectDir).c_str(),
                        dirty ? " *" : "", state, playing ? "PLAYING" : "editing");
            ImGui::EndMainMenuBar();
        }
    }

    void build_layout(ImGuiID dock) {
        ImGui::DockBuilderRemoveNode(dock);
        ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dock, ImGui::GetMainViewport()->WorkSize);

        ImGuiID center = dock;
        const ImGuiID left =
            ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.18f, nullptr, &center);
        const ImGuiID right =
            ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.24f, nullptr, &center);
        const ImGuiID bottom =
            ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.24f, nullptr, &center);

        ImGuiID left_top = 0;
        const ImGuiID left_bottom =
            ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.45f, nullptr, &left_top);

        ImGui::DockBuilderDockWindow("Scene", center);
        ImGui::DockBuilderDockWindow("UI", center);
        ImGui::DockBuilderDockWindow("Game", center);
        ImGui::DockBuilderDockWindow("Hierarchy", left_top);
        ImGui::DockBuilderDockWindow("Assets", left_bottom);
        ImGui::DockBuilderDockWindow("Inspector", right);
        ImGui::DockBuilderDockWindow("Console", bottom);
        ImGui::DockBuilderFinish(dock);
    }

    void draw() {
        world.update_transforms();

        if (playing) {
            if (play_runtime) {
                play_runtime->update(play_input, ImGui::GetIO().DeltaTime);
            }
            play_world.update_transforms();
            play_input.pressed = 0;
            play_input.released = 0;
            play_input.held = 0;
        }

        draw_menu();

        const ImGuiID dock = ImGui::DockSpaceOverViewport(
            0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
        if (!layout_built) {
            if (!layout_from_ini) build_layout(dock);
            layout_built = true;
        }

        draw_scene();
        draw_ui_designer();
        draw_game_view();
        draw_hierarchy();
        draw_inspector();
        draw_assets();
        draw_console();

        if (ImGui::IsKeyPressed(ImGuiKey_S) && ImGui::GetIO().KeyCtrl) save_scene();
        if (ImGui::IsKeyPressed(ImGuiKey_F5) && !ImGui::GetIO().WantTextInput) {
            if (playing) {
                stop_play();
            } else {
                start_play();
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F6) && !ImGui::GetIO().WantTextInput) {
            ImGui::SetWindowFocus("Game");
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F7) && !ImGui::GetIO().WantTextInput) {
            ImGui::SetWindowFocus("Scene");
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F8) && !ImGui::GetIO().WantTextInput) {
            ImGui::SetWindowFocus("UI");
        }
    }
};

} // namespace

int main() {
    if (glfwInit() == GLFW_FALSE) return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1600, 900, "Lumika Editor", nullptr, nullptr);
    if (window == nullptr) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) return 1;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.FontGlobalScale = 1.15f;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    set_log_sink(&log_sink);
    register_builtin_components();
    register_project_components();

    Editor editor;
    editor.layout_from_ini = fs::exists("imgui.ini");
    editor.render = create_render_device_gl();
    if (!editor.render->init()) {
        std::fprintf(stderr, "Lumika: failed to initialise the OpenGL renderer\n");
        return 1;
    }
    editor.load_project(std::string(LUMIKA_ROOT) + "/projects/demo_project");

    while (glfwWindowShouldClose(window) == GLFW_FALSE) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        editor.draw();

        ImGui::Render();
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.06f, 0.065f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
