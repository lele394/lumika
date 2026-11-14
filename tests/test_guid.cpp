#include <doctest/doctest.h>

#include "e3d/core/guid.hpp"

TEST_CASE("guid formats and parses") {
    e3d::Guid guid;
    REQUIRE(e3d::guid_from_string("0f8fad5b-d9cb-469f-a165-70867728950e", guid));
    CHECK(e3d::guid_to_string(guid) == "0f8fad5b-d9cb-469f-a165-70867728950e");

    e3d::Guid upper;
    REQUIRE(e3d::guid_from_string("0F8FAD5BD9CB469FA16570867728950E", upper));
    CHECK(upper == guid);
    CHECK(e3d::guid_to_string(upper) == "0f8fad5b-d9cb-469f-a165-70867728950e");
}

TEST_CASE("guid rejects malformed input") {
    e3d::Guid guid;
    CHECK_FALSE(e3d::guid_from_string("", guid));
    CHECK_FALSE(e3d::guid_from_string("0f8fad5b", guid));
    CHECK_FALSE(e3d::guid_from_string("zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz", guid));
    CHECK_FALSE(e3d::guid_from_string("00000000-0000-0000-0000-000000000000", guid));
    CHECK_FALSE(guid.valid());
}

TEST_CASE("generated guids are valid and unique") {
    const e3d::Guid a = e3d::generate_guid();
    const e3d::Guid b = e3d::generate_guid();
    CHECK(a.valid());
    CHECK(b.valid());
    CHECK(a != b);
}

TEST_CASE("guid round-trips through strings") {
    for (int i = 0; i < 16; ++i) {
        const e3d::Guid original = e3d::generate_guid();
        e3d::Guid parsed;
        REQUIRE(e3d::guid_from_string(e3d::guid_to_string(original), parsed));
        CHECK(parsed == original);
    }
}
