#include <sstream>

#include <gtest/gtest.h>

#include <yarangc/codegen.hpp>
#include <yarangc/pattern_extractor.hpp>

using namespace ::testing;
using namespace yaramod;

class CodegenTest : public Test
{
public:
    CodegenTest() : pattern_extractor(), codegen(&pattern_extractor) {}

    void with_import(const std::string& module_name)
    {
        ss << "import \"" << module_name << "\"\n\n";
    }

    template <typename... Ts>
    void input(const std::string& condition, Ts&&... strings)
    {
        ss << "rule abc {\n";
        if (sizeof...(Ts) > 0)
            ss << "strings:\n";
        _input(strings...);
        ss << "condition:\n"
            << condition
            << "\n}";
        ruleset = yaramod.parseStream(ss);

        pattern_extractor.extract(ruleset.get());
        result = codegen.generate(ruleset->getRules()[0].get());
    }

    template <typename StrT, typename... Ts>
    void _input(StrT&& str, Ts&&... strings)
    {
        ss << str;
        _input(std::forward<Ts>(strings)...);
    }

    void _input() {}

    PatternExtractor pattern_extractor;
    Codegen codegen;

    std::stringstream ss;
    Yaramod yaramod;
    std::unique_ptr<YaraFile> ruleset;

    std::string result;
};

TEST_F(CodegenTest,
True) {
    input("true");

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return true;\n"
        "}"
    );
}

TEST_F(CodegenTest,
False) {
    input("false");

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return false;\n"
        "}"
    );
}

TEST_F(CodegenTest,
StringId) {
    input("$s01", R"($s01 = "abc")");

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return match_string(ctx, 0ul);\n"
        "}"
    );
}

TEST_F(CodegenTest,
StringCount) {
    input("#s01 > 0", R"($s01 = "abc")");

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return match_count(ctx, 0ul) > 0ul;\n"
        "}"
    );
}

TEST_F(CodegenTest,
StringOffset) {
    input("@s01 > 0", R"($s01 = "abc")");

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return match_offset(ctx, 0ul) > 0ul;\n"
        "}"
    );
}

TEST_F(CodegenTest,
StringOffsetIndex) {
    input("@s01[1] > 0", R"($s01 = "abc")");

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return match_offset(ctx, 0ul, 1ul - 1ul) > 0ul;\n"
        "}"
    );
}

TEST_F(CodegenTest,
StringLength) {
    input("!s01 > 0", R"($s01 = "abc")");

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return match_length(ctx, 0ul) > 0ul;\n"
        "}"
    );
}

TEST_F(CodegenTest,
StringLengthIndex) {
    input("!s01[1] > 0", R"($s01 = "abc")");

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return match_length(ctx, 0ul, 1ul - 1ul) > 0ul;\n"
        "}"
    );
}

TEST_F(CodegenTest,
StringAt) {
    input("$s01 at 0x100", R"($s01 = "abc")");

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return match_at(ctx, 0ul, 256ul);\n"
        "}"
    );
}

TEST_F(CodegenTest,
StringIn) {
    input("$s01 in (0x100 .. 0x200)", R"($s01 = "abc")");

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return match_in(ctx, 0ul, 256ul, 512ul);\n"
        "}"
    );
}

TEST_F(CodegenTest,
AnyOfThem) {
    input("any of them",
        R"($s01 = "abc")",
        R"($s02 = "def")"
        R"($s03 = "ghi")"
    );

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return of(ctx, 1ul, 0ul, 1ul, 2ul);\n"
        "}"
    );
}

TEST_F(CodegenTest,
TwoOfThem) {
    input("2 of them",
        R"($s01 = "abc")",
        R"($s02 = "def")"
        R"($s03 = "ghi")"
    );

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return of(ctx, 2ul, 0ul, 1ul, 2ul);\n"
        "}"
    );
}

TEST_F(CodegenTest,
AllOfThem) {
    input("all of them",
        R"($s01 = "abc")",
        R"($s02 = "def")"
        R"($s03 = "ghi")"
    );

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return of(ctx, 3ul, 0ul, 1ul, 2ul);\n"
        "}"
    );
}

TEST_F(CodegenTest,
AnyOfSome) {
    input("any of ($s01, $s02)",
        R"($s01 = "abc")",
        R"($s02 = "def")"
        R"($s03 = "ghi")"
    );

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return of(ctx, 1ul, 0ul, 1ul);\n"
        "}"
    );
}

TEST_F(CodegenTest,
TwoOfSome) {
    input("2 of ($s01, $s02)",
        R"($s01 = "abc")",
        R"($s02 = "def")"
        R"($s03 = "ghi")"
    );

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return of(ctx, 2ul, 0ul, 1ul);\n"
        "}"
    );
}

TEST_F(CodegenTest,
AllOfSome) {
    input("all of ($s01, $s02)",
        R"($s01 = "abc")",
        R"($s02 = "def")"
        R"($s03 = "ghi")"
    );

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return of(ctx, 2ul, 0ul, 1ul);\n"
        "}"
    );
}

TEST_F(CodegenTest,
AnyOfWildcard) {
    input("any of ($s*)",
        R"($s01 = "abc")",
        R"($b01 = "def")"
        R"($b02 = "ghi")"
    );

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return of(ctx, 1ul, 0ul);\n"
        "}"
    );
}

TEST_F(CodegenTest,
TwoOfWildcard) {
    input("all of ($s*)",
        R"($s01 = "abc")",
        R"($b01 = "def")"
        R"($b02 = "ghi")"
    );

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return of(ctx, 1ul, 0ul);\n"
        "}"
    );
}

TEST_F(CodegenTest,
AllOfWildcard) {
    input("all of ($s*)",
        R"($s01 = "abc")",
        R"($b01 = "def")"
        R"($b02 = "ghi")"
    );

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return of(ctx, 1ul, 0ul);\n"
        "}"
    );
}

TEST_F(CodegenTest,
ForLoopOverStrings) {
    input("for 2 of them : ( $ at 0x500 )",
        R"($s01 = "abc")",
        R"($s02 = "def")"
        R"($s03 = "ghi")"
    );

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return loop(ctx, 2ul, [](auto ctx, auto&& vars, auto id) {\n"
        "return match_at(ctx, id, 1280ul);\n"
        "}, 0ul, 1ul, 2ul);\n"
        "}"
    );
}

TEST_F(CodegenTest,
ForLoopOverIntEnum) {
    input("for 2 i in (1, 2, 3, 4, 5, 6) : ( $s01 at 0x500 + i )",
        R"($s01 = "abc")"
    );

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return loop_ints(ctx, 2ul, [](auto ctx, auto&& vars, auto id) {\n"
        "return match_at(ctx, 0ul, 1280ul + std::get<0>(vars));\n"
        "}, 1ul, 2ul, 3ul, 4ul, 5ul, 6ul);\n"
        "}"
    );
}

TEST_F(CodegenTest,
ForLoopOverIntRange) {
    input("for 2 i in (0x100 .. filesize) : ( $s01 at i )",
        R"($s01 = "abc")"
    );

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return loop_range(ctx, 2ul, [](auto ctx, auto&& vars, auto id) {\n"
        "return match_at(ctx, 0ul, std::get<0>(vars));\n"
        "}, 256ul, filesize(ctx));\n"
        "}"
    );
}

TEST_F(CodegenTest,
ForLoopOverIntsNested) {
    input("for any i in (0x100 .. filesize) : ( for any j in (0,1,2,3) : ( $s01 at i + j ) )",
        R"($s01 = "abc")"
    );

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return loop_range(ctx, 1ul, [](auto ctx, auto&& vars, auto id) {\n"
        "return loop_ints(ctx, 1ul, std::move(vars), [](auto ctx, auto&& vars, auto id) {\n"
        "return match_at(ctx, 0ul, std::get<0>(vars) + std::get<1>(vars));\n"
        "}, 0ul, 1ul, 2ul, 3ul);\n"
        "}, 256ul, filesize(ctx));\n"
        "}"
    );
}

TEST_F(CodegenTest,
ForLoopOverStringsNested) {
    input("for any i in (0x100 .. filesize) : ( for all of them : ( $ at i ) )",
        R"($s01 = "abc")",
        R"($s02 = "def")"
    );

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return loop_range(ctx, 1ul, [](auto ctx, auto&& vars, auto id) {\n"
        "return loop(ctx, 2ul, std::move(vars), [](auto ctx, auto&& vars, auto id) {\n"
        "return match_at(ctx, id, std::get<0>(vars));\n"
        "}, 0ul, 1ul);\n"
        "}, 256ul, filesize(ctx));\n"
        "}"
    );
}

TEST_F(CodegenTest,
ReadInt8) {
    input("int8(0x10) == 0x80");

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return read_data<Endian::Little, std::int8_t>(ctx, 16ul) == 128ul;\n"
        "}"
    );
}

TEST_F(CodegenTest,
ReadUInt8) {
    input("uint8(0x10) == 0x80");

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return read_data<Endian::Little, std::uint8_t>(ctx, 16ul) == 128ul;\n"
        "}"
    );
}

TEST_F(CodegenTest,
ReadInt16ul) {
    input("int16(0x10) == 0x160");

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return read_data<Endian::Little, std::int16_t>(ctx, 16ul) == 352ul;\n"
        "}"
    );
}

TEST_F(CodegenTest,
ReadUInt16ul) {
    input("uint16(0x10) == 0x160");

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return read_data<Endian::Little, std::uint16_t>(ctx, 16ul) == 352ul;\n"
        "}"
    );
}

TEST_F(CodegenTest,
ReadInt32) {
    input("int32(0x10) == 0x320");

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return read_data<Endian::Little, std::int32_t>(ctx, 16ul) == 800ul;\n"
        "}"
    );
}

TEST_F(CodegenTest,
ReadUInt32) {
    input("uint32(0x10) == 0x320");

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return read_data<Endian::Little, std::uint32_t>(ctx, 16ul) == 800ul;\n"
        "}"
    );
}

TEST_F(CodegenTest,
ReadInt8Be) {
    input("int8be(0x10) == 0x80");

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return read_data<Endian::Big, std::int8_t>(ctx, 16ul) == 128ul;\n"
        "}"
    );
}

TEST_F(CodegenTest,
ReadUInt8Be) {
    input("uint8be(0x10) == 0x80");

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return read_data<Endian::Big, std::uint8_t>(ctx, 16ul) == 128ul;\n"
        "}"
    );
}

TEST_F(CodegenTest,
ReadInt16Be) {
    input("int16be(0x10) == 0x160");

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return read_data<Endian::Big, std::int16_t>(ctx, 16ul) == 352ul;\n"
        "}"
    );
}

TEST_F(CodegenTest,
ReadUInt16Be) {
    input("uint16be(0x10) == 0x160");

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return read_data<Endian::Big, std::uint16_t>(ctx, 16ul) == 352ul;\n"
        "}"
    );
}

TEST_F(CodegenTest,
ReadInt32Be) {
    input("int32be(0x10) == 0x320");

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return read_data<Endian::Big, std::int32_t>(ctx, 16ul) == 800ul;\n"
        "}"
    );
}

TEST_F(CodegenTest,
ReadUInt32Be) {
    input("uint32be(0x10) == 0x320");

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return read_data<Endian::Big, std::uint32_t>(ctx, 16ul) == 800ul;\n"
        "}"
    );
}

TEST_F(CodegenTest,
CuckooSyncMutex) {
    with_import("cuckoo");
    input("cuckoo.sync.mutex(/^abc$/)");

    EXPECT_EQ(result,
        "static bool rule_abc(const ScanContext* ctx)\n"
        "{\n"
        "return get_mutex_match(ctx, 0ul) > 0;\n"
        "}"
    );
}
