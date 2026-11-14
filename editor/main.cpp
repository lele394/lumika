#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "e3d/e3d.hpp"
#include "project_scripts.hpp"

namespace fs = std::filesystem;
using namespace e3d;

namespace {

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
    EntityId selected = kInvalidEntity;
    bool dirty = false;
    bool project_loaded = false;

    char search[128] = {};
    char new_entity_name[64] = "GameObject";
    char prefab_name[64] = "new_prefab";
    int asset_filter = 0;
    std::string status = "ready";
    std::vector<PrefabFile> prefabs;

    bool load_project(const std::string& dir) {
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
        ImGui::SetNextWindowPos(ImVec2(10, 40), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320, 600), ImGuiCond_FirstUseEver);
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
        ImGui::SetNextWindowPos(ImVec2(340, 40), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(760, 600), ImGuiCond_FirstUseEver);
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
        ImGui::SetNextWindowPos(ImVec2(1110, 40), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(480, 600), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Assets")) {
            ImGui::End();
            return;
        }

        ImGui::InputTextWithHint("##search", "fuzzy search name, path or tag", search,
                                 sizeof(search));

        static const char* const kFilters[] = {"All",      "Mesh",   "Texture", "Material",
                                               "Audio",    "Font",   "Prefab"};
        ImGui::Combo("##filter", &asset_filter, kFilters, IM_ARRAYSIZE(kFilters));

        ImGui::SeparatorText("Indexed assets");
        for (const AssetEntry& entry : database.entries()) {
            const bool filter_ok =
                asset_filter == 0 || std::strcmp(asset_type_name(entry.type), kFilters[asset_filter]) == 0;
            if (!filter_ok) continue;

            std::string haystack = entry.path + " " + asset_type_name(entry.type);
            for (const std::string& tag : entry.tags) haystack += " " + tag;
            if (!fuzzy_match(haystack, search)) continue;

            std::string label = base_name(entry.path) + "  [" + asset_type_name(entry.type) + "]";
            for (const std::string& tag : entry.tags) label += " #" + tag;
            ImGui::Selectable(label.c_str());
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", entry.path.c_str());
        }

        ImGui::SeparatorText("Prefabs");
        for (const PrefabFile& prefab : prefabs) {
            if (asset_filter != 0 && asset_filter != 6) continue;
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

        ImGui::SeparatorText("Create prefab");
        ImGui::InputText("##prefab_name", prefab_name, sizeof(prefab_name));
        ImGui::SameLine();
        if (ImGui::Button("From selection")) create_prefab();

        ImGui::End();
    }

    void draw_console() {
        ImGui::SetNextWindowPos(ImVec2(10, 650), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(1580, 220), ImGuiCond_FirstUseEver);
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

    void draw_menu() {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Save Scene", "Ctrl+S")) save_scene();
                if (ImGui::MenuItem("Reload Project")) load_project(projectDir);
                ImGui::Separator();
                if (ImGui::MenuItem("Quit")) glfwSetWindowShouldClose(glfwGetCurrentContext(), 1);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Build")) {
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
            ImGui::Text("  |  %s%s  |  Playtest: %s", base_name(projectDir).c_str(),
                        dirty ? " *" : "", state);
            ImGui::EndMainMenuBar();
        }
    }

    void draw() {
        draw_menu();
        draw_hierarchy();
        draw_inspector();
        draw_assets();
        draw_console();

        if (ImGui::IsKeyPressed(ImGuiKey_S) && ImGui::GetIO().KeyCtrl) save_scene();
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
