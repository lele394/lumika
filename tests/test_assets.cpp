#include <doctest/doctest.h>

#include "e3d/assets/asset_db.hpp"
#include "e3d/assets/material_loader.hpp"
#include "e3d/audio/wav.hpp"
#include "e3d/core/guid.hpp"

#include <vector>

namespace {

void put_u16(std::vector<e3d::u8>& bytes, e3d::u16 value) {
    bytes.push_back(static_cast<e3d::u8>(value & 0xFF));
    bytes.push_back(static_cast<e3d::u8>((value >> 8) & 0xFF));
}

void put_u32(std::vector<e3d::u8>& bytes, e3d::u32 value) {
    bytes.push_back(static_cast<e3d::u8>(value & 0xFF));
    bytes.push_back(static_cast<e3d::u8>((value >> 8) & 0xFF));
    bytes.push_back(static_cast<e3d::u8>((value >> 16) & 0xFF));
    bytes.push_back(static_cast<e3d::u8>((value >> 24) & 0xFF));
}

void put_tag(std::vector<e3d::u8>& bytes, const char* tag) {
    for (int i = 0; i < 4; ++i) bytes.push_back(static_cast<e3d::u8>(tag[i]));
}

std::vector<e3d::u8> make_wav() {
    std::vector<e3d::u8> bytes;
    put_tag(bytes, "RIFF");
    put_u32(bytes, 36 + 8);
    put_tag(bytes, "WAVE");
    put_tag(bytes, "fmt ");
    put_u32(bytes, 16);
    put_u16(bytes, 1);       // PCM
    put_u16(bytes, 1);       // mono
    put_u32(bytes, 8000);    // rate
    put_u32(bytes, 16000);   // byte rate
    put_u16(bytes, 2);       // block align
    put_u16(bytes, 16);      // bits
    put_tag(bytes, "data");
    put_u32(bytes, 8);
    for (int i = 0; i < 4; ++i) put_u16(bytes, static_cast<e3d::u16>(1000 + i));
    return bytes;
}

} // namespace

TEST_CASE("wav parser reads PCM16 metadata") {
    const std::vector<e3d::u8> bytes = make_wav();
    e3d::wav::WavInfo info;
    REQUIRE(e3d::wav::parse_wav(bytes.data(), bytes.size(), info));

    CHECK(info.channels == 1);
    CHECK(info.sample_rate == 8000);
    CHECK(info.sample_frames == 4);
    CHECK(info.samples[0] == 1000);
    CHECK(info.samples[3] == 1003);
}

TEST_CASE("wav parser rejects malformed files") {
    std::vector<e3d::u8> bytes = make_wav();
    e3d::wav::WavInfo info;

    CHECK_FALSE(e3d::wav::parse_wav(bytes.data(), 8, info));

    bytes[0] = 'X';
    CHECK_FALSE(e3d::wav::parse_wav(bytes.data(), bytes.size(), info));
}

TEST_CASE("material parser reads shader, tint and texture reference") {
    const char* text = R"({
        "format_version": 1,
        "shader": "lit_textured",
        "tint": [1.0, 0.5, 0.25, 1.0],
        "ambient": 0.2,
        "diffuse": 0.8,
        "light_direction": [0, 1, 0],
        "texture": { "$asset": "8b2c4d6e-1f3a-4b5c-9d7e-2a3b4c5d6e7f" }
    })";

    e3d::MaterialDesc material;
    e3d::Guid texture;
    std::string error;
    REQUIRE(e3d::parse_material(text, material, texture, error));

    CHECK(material.shader == e3d::ShaderId::LitTextured);
    CHECK(material.tint.g == doctest::Approx(0.5f));
    CHECK(material.ambient == doctest::Approx(0.2f));
    CHECK(material.diffuse == doctest::Approx(0.8f));
    CHECK(material.light_direction.y == doctest::Approx(1.0f));
    CHECK(e3d::guid_to_string(texture) == "8b2c4d6e-1f3a-4b5c-9d7e-2a3b4c5d6e7f");
}

TEST_CASE("material parser rejects unknown shaders") {
    const char* text = R"({"format_version": 1, "shader": "pbr"})";

    e3d::MaterialDesc material;
    e3d::Guid texture;
    std::string error;
    CHECK_FALSE(e3d::parse_material(text, material, texture, error));
    CHECK(error.find("unknown shader") != std::string::npos);
}

TEST_CASE("asset database resolves manifest guids") {
    const char* manifest = R"({
        "format_version": 1,
        "assets": [
            { "guid": "2f0a3c4d-5e6f-4a7b-8c9d-0e1f2a3b4c5d", "type": "mesh", "path": "models/cube.cmesh" },
            { "guid": "8b2c4d6e-1f3a-4b5c-9d7e-2a3b4c5d6e7f", "type": "texture", "path": "textures/checker.t3x" }
        ]
    })";

    e3d::AssetDb database;
    std::string error;
    REQUIRE(database.load_manifest(manifest, error));

    e3d::Guid guid;
    REQUIRE(e3d::guid_from_string("2f0a3c4d-5e6f-4a7b-8c9d-0e1f2a3b4c5d", guid));
    const e3d::AssetEntry* entry = database.find(guid);
    REQUIRE(entry != nullptr);
    CHECK(entry->type == e3d::AssetType::Mesh);
    CHECK(entry->path == "models/cube.cmesh");

    REQUIRE(e3d::guid_from_string("8b2c4d6e-1f3a-4b5c-9d7e-2a3b4c5d6e7f", guid));
    CHECK(database.find(guid)->type == e3d::AssetType::Texture);
}

TEST_CASE("asset database rejects invalid manifests") {
    e3d::AssetDb database;
    std::string error;

    CHECK_FALSE(database.load_manifest(R"({"format_version": 1})", error));
    CHECK_FALSE(database.load_manifest(
        R"({"format_version": 1, "assets": [{"guid": "nope", "type": "mesh", "path": "x"}]})",
        error));
    CHECK_FALSE(database.load_manifest(
        R"({"format_version": 1, "assets": [{"guid": "2f0a3c4d-5e6f-4a7b-8c9d-0e1f2a3b4c5d", "type": "wat", "path": "x"}]})",
        error));
}
