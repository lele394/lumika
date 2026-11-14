#include <doctest/doctest.h>

#include "e3d/core/json.hpp"

using e3d::json::ParseError;
using e3d::json::Value;

TEST_CASE("json parses nested structures") {
    const char* text = R"({"a":1,"b":[true,null,"x\n\u00e9"],"c":{"d":-2.5e3},"e":1.0e-3})";

    Value value;
    ParseError error;
    REQUIRE(e3d::json::parse(text, value, error));

    REQUIRE(value.is_object());
    CHECK(value.find("a")->as_int64() == 1);

    const Value* array = value.find("b");
    REQUIRE(array != nullptr);
    REQUIRE(array->is_array());
    CHECK(array->size() == 3);
    CHECK((*array)[0].as_bool());
    CHECK((*array)[1].is_null());
    CHECK((*array)[2].as_string() == std::string("x\n\xc3\xa9"));

    CHECK(value.find("c")->find("d")->as_number() == doctest::Approx(-2500.0));
    CHECK(value.find("e")->as_number() == doctest::Approx(0.001));
}

TEST_CASE("json dump is deterministic and round-trips") {
    const char* text = R"({"z":1,"a":{"b":[1,2,3]},"s":"quote \" and \\","n":0.1})";

    Value first;
    ParseError error;
    REQUIRE(e3d::json::parse(text, first, error));

    const std::string dumped = e3d::json::dump(first);
    CHECK(dumped == e3d::json::dump(first));

    Value second;
    REQUIRE(e3d::json::parse(dumped, second, error));
    CHECK(e3d::json::dump(second) == dumped);

    // Object key order is insertion order, not sorted.
    CHECK(dumped.find("\"z\"") < dumped.find("\"a\""));
}

TEST_CASE("json rejects malformed documents") {
    const char* cases[] = {
        "{\"a\":1,}",
        "[1,2",
        "{\"a\" 1}",
        "tru",
        "1 2",
        "\"unterminated",
        "{\"a\":\"\\q\"}",
        "{\"a\":1e}",
    };

    for (const char* text : cases) {
        Value value;
        ParseError error;
        CHECK_FALSE(e3d::json::parse(text, value, error));
        CHECK_FALSE(error.message.empty());
    }
}

TEST_CASE("json reports error offsets") {
    Value value;
    ParseError error;
    CHECK_FALSE(e3d::json::parse("{\"a\": }", value, error));
    CHECK(error.offset >= 5);
}

TEST_CASE("json writes escapes and integers") {
    Value object = Value::object();
    object.set("text", Value::string("tab\there \"quoted\""));
    object.set("count", Value::number(42.0));
    object.set("ratio", Value::number(0.5));

    const std::string dumped = e3d::json::dump(object, false);
    CHECK(dumped == R"({"text":"tab\there \"quoted\"","count":42,"ratio":0.5})");
}
