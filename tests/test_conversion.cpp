#include <gtest/gtest.h>

#include <yarangc/conversion.hpp>
#include <yaramod/builder/yara_hex_string_builder.h>

using namespace ::testing;
using namespace yaramod;

class ConversionTest : public Test {};

TEST_F(ConversionTest,
EmptyHexString) {
    YaraHexStringBuilder builder;

    auto pattern = hex_string_to_pattern("rule", *builder.get());

    EXPECT_TRUE(pattern->is_literal());
    EXPECT_EQ(pattern->get_pattern(), "");
}

TEST_F(ConversionTest,
SingleByteHexString) {
    YaraHexStringBuilder builder(0xAB);

    auto pattern = hex_string_to_pattern("rule", *builder.get());

    EXPECT_TRUE(pattern->is_literal());
    EXPECT_EQ(pattern->get_pattern(), R"(\xAB)");
}

TEST_F(ConversionTest,
SimpleHexString) {
    YaraHexStringBuilder builder(0xAB);
    builder.add(0xCD);
    builder.add(0xFF);

    auto pattern = hex_string_to_pattern("rule", *builder.get());

    EXPECT_TRUE(pattern->is_literal());
    EXPECT_EQ(pattern->get_pattern(), R"(\xAB\xCD\xFF)");
}

TEST_F(ConversionTest,
HexStringWithFixedJump) {
    YaraHexStringBuilder builder(0xAB);
    builder.add(jumpFixed(5));
    builder.add(0xCD);

    auto pattern = hex_string_to_pattern("rule", *builder.get());

    EXPECT_TRUE(pattern->is_regex());
    EXPECT_EQ(pattern->get_pattern(), R"(\xAB.{5}\xCD)");
}

TEST_F(ConversionTest,
HexStringWithRangeJump) {
    YaraHexStringBuilder builder(0xAB);
    builder.add(jumpRange(1, 5));
    builder.add(0xCD);

    auto pattern = hex_string_to_pattern("rule", *builder.get());

    EXPECT_TRUE(pattern->is_regex());
    EXPECT_EQ(pattern->get_pattern(), R"(\xAB.{1,5}\xCD)");
}

TEST_F(ConversionTest,
HexStringWithVaryingJump) {
    YaraHexStringBuilder builder(0xAB);
    builder.add(jumpVarying());
    builder.add(0xCD);

    auto pattern = hex_string_to_pattern("rule", *builder.get());

    EXPECT_TRUE(pattern->is_regex());
    EXPECT_EQ(pattern->get_pattern(), R"(\xAB.*\xCD)");
}

TEST_F(ConversionTest,
HexStringWithVaryingRangeJump) {
    YaraHexStringBuilder builder(0xAB);
    builder.add(jumpVaryingRange(1));
    builder.add(0xCD);

    auto pattern = hex_string_to_pattern("rule", *builder.get());

    EXPECT_TRUE(pattern->is_regex());
    EXPECT_EQ(pattern->get_pattern(), R"(\xAB.{1,}\xCD)");
}

TEST_F(ConversionTest,
HexStringWithWildcard) {
    YaraHexStringBuilder builder(0xAB);
    builder.add(wildcard());
    builder.add(0xCD);

    auto pattern = hex_string_to_pattern("rule", *builder.get());

    EXPECT_TRUE(pattern->is_regex());
    EXPECT_EQ(pattern->get_pattern(), R"(\xAB.\xCD)");
}

TEST_F(ConversionTest,
HexStringWithPartialWildcardLow) {
    YaraHexStringBuilder builder(0xAB);
    builder.add(wildcardLow(0xC));
    builder.add(0xCD);

    auto pattern = hex_string_to_pattern("rule", *builder.get());

    EXPECT_TRUE(pattern->is_regex());
    EXPECT_EQ(pattern->get_pattern(), R"(\xAB(\xC0|\xC1|\xC2|\xC3|\xC4|\xC5|\xC6|\xC7|\xC8|\xC9|\xCA|\xCB|\xCC|\xCD|\xCE|\xCF)\xCD)");
}

TEST_F(ConversionTest,
HexStringWithPartialWildcardHigh) {
    YaraHexStringBuilder builder(0xAB);
    builder.add(wildcardHigh(0xC));
    builder.add(0xCD);

    auto pattern = hex_string_to_pattern("rule", *builder.get());

    EXPECT_TRUE(pattern->is_regex());
    EXPECT_EQ(pattern->get_pattern(), R"(\xAB(\x0C|\x1C|\x2C|\x3C|L|\x5C|l|\x7C|\x8C|\x9C|\xAC|\xBC|\xCC|\xDC|\xEC|\xFC)\xCD)");
}

TEST_F(ConversionTest,
HexStringWithAlternation) {
    YaraHexStringBuilder builder(0xAB);
    builder.add(alt(
        YaraHexStringBuilder{}.add(0x61).add(0x62),
        YaraHexStringBuilder{}.add(0x70).add(0x71)
    ));
    builder.add(0xCD);

    auto pattern = hex_string_to_pattern("rule", *builder.get());

    EXPECT_TRUE(pattern->is_regex());
    EXPECT_EQ(pattern->get_pattern(), R"(\xAB(ab|pq)\xCD)");
}
