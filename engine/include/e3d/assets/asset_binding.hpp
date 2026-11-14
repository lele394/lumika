#pragma once

#include <string>
#include <vector>

namespace e3d {

class World;
class AssetManager;

/// Resolves `MeshRenderer` and `AudioSource` asset references into device handles.
/// Failures are reported as warnings; the affected renderer is left invisible.
void resolve_scene_assets(World& world, AssetManager& assets, std::vector<std::string>& warnings);

} // namespace e3d
