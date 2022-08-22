#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <tuple>
#include <variant>
#include <vector>

#include <hs/hs_runtime.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/document.h>

#include "endian.hpp"

#define UNDEFINED 0xFFFABADAFABADAFFull
#define IS_UNDEF(x) static_cast<std::uint64_t>(x) == UNDEFINED

struct Scanner;
struct ScanContext;

using RuleFunction = bool(*)(const ScanContext*);
using MatchCallback = void(*)(const char*, void*);

enum class RuleVisibility
{
    Public,
    Private
};

enum class RuleMatch
{
    Hit,
    NoHit,
    NotEvaluated
};

struct Rule
{
    const char* name;
    RuleVisibility visibility;
    RuleFunction function;
    RuleMatch match;
};

struct Match
{
    std::uint32_t count;
    std::vector<std::uint64_t> offsets;
    std::vector<std::uint32_t> lengths;
};

struct Scanner
{
    hs_scratch_t* literal_scratch;
    hs_scratch_t* regex_scratch;
    hs_scratch_t* mutex_scratch;
    MatchCallback match_callback;
    ScanContext* ctx;
};

static void add_match(ScanContext* ctx, std::size_t id, std::uint64_t offset, std::uint64_t length);
static void add_mutex_match(ScanContext* ctx, std::size_t id);
static const Match* get_match(const ScanContext* ctx, std::size_t id);
static std::uint64_t get_mutex_match(const ScanContext* ctx, std::size_t id);
static const char* get_data(const ScanContext* ctx, std::uint64_t offset = 0);
static std::uint64_t get_data_size(const ScanContext* ctx);
static bool evaluate_rule(const ScanContext* ctx, std::size_t id);

static hs_database_t* literal_db = nullptr;
static hs_database_t* regex_db = nullptr;
static hs_database_t* mutex_db = nullptr;

static int on_match(unsigned int id, unsigned long long from, unsigned long long to, unsigned int/* flags*/, void *context)
{
    auto ctx = static_cast<ScanContext*>(context);
    add_match(ctx, id, from, to - from);
    return 0;
}

static int on_mutex_match(unsigned int id, unsigned long long from, unsigned long long to, unsigned int/* flags*/, void *context)
{
    auto ctx = static_cast<ScanContext*>(context);
    add_mutex_match(ctx, id);
    return 0;
}

inline bool match_string(const ScanContext* ctx, std::size_t id)
{
    return get_match(ctx, id)->count > 0;
}

inline std::uint64_t match_count(const ScanContext* ctx, std::size_t id)
{
    return get_match(ctx, id)->count;
}

inline std::uint64_t match_offset(const ScanContext* ctx, std::size_t id, std::size_t index = 0)
{
    auto& offsets = get_match(ctx, id)->offsets;
    return index < offsets.size() ? offsets[index] : UNDEFINED;
}

inline std::uint64_t match_length(const ScanContext* ctx, std::size_t id, std::size_t index = 0)
{
    auto& lengths = get_match(ctx, id)->lengths;
    return index < lengths.size() ? lengths[index] : UNDEFINED;
}

inline bool match_at(const ScanContext* ctx, std::size_t id, std::uint64_t expected)
{
    for (auto offset : get_match(ctx, id)->offsets)
    {
        if (offset == expected)
            return true;
    }
    return false;
}

inline bool match_in(const ScanContext* ctx, std::size_t id, std::uint64_t low, std::uint64_t high)
{
    for (auto offset : get_match(ctx, id)->offsets)
    {
        if (low <= offset && offset < high)
            return true;
    }
    return false;
}

template <typename BodyFn, typename... Vars, typename... Ts>
inline bool loop(const ScanContext* ctx, std::uint64_t n, std::tuple<Vars...>&& vars, const BodyFn& body, Ts... ids)
{
    const std::uint64_t tolerance = sizeof...(Ts) - n;
    std::uint64_t index = 0, hits = 0;
    for (auto id : std::array<std::uint64_t, sizeof...(Ts)>{ids...})
    {
        if (index - hits > tolerance)
            return false;

        if (body(ctx, std::move(vars), id) && ++hits == n)
            return true;

        index++;
    }

    return false;
}

template <typename BodyFn, typename... Ts>
inline bool loop(const ScanContext* ctx, std::uint64_t n, const BodyFn& body, Ts... ids)
{
    return loop(ctx, n, std::tuple{}, body, ids...);
}

template <typename BodyFn, typename... Vars, typename... Ints>
inline bool loop_ints(const ScanContext* ctx, std::uint64_t n, std::tuple<Vars...>&& vars, const BodyFn& body, Ints... ints)
{
    const std::uint64_t tolerance = sizeof...(Ints) - n;
    std::uint64_t index = 0, hits = 0;
    for (auto i : std::array<std::uint64_t, sizeof...(Ints)>{ints...})
    {
        if (index - hits > tolerance)
            return false;

        if (body(ctx, std::tuple_cat(vars, std::tuple{std::ref(i)}), i) && ++hits == n)
            return true;

        index++;
    }

    return false;
}

template <typename BodyFn, typename... Ints>
inline bool loop_ints(const ScanContext* ctx, std::uint64_t n, const BodyFn& body, Ints... ints)
{
    return loop_ints(ctx, n, std::tuple{}, body, ints...);
}

template <typename BodyFn, typename... Vars>
inline bool loop_range(const ScanContext* ctx, std::uint64_t n, std::tuple<Vars...>&& vars, const BodyFn& body, std::uint64_t low, std::uint64_t high)
{
    n = std::min(n, high);
    const std::uint64_t tolerance = high - low - n;

    std::uint64_t i = low, index = 0, hits = 0;
    for (i = low; i < high; ++i)
    {
        if (index - hits > tolerance)
            return false;

        if (body(ctx, std::tuple_cat(vars, std::tuple{std::ref(i)}), i) && ++hits == n)
            return true;

        index++;
    }

    return false;
}

template <typename BodyFn>
inline bool loop_range(const ScanContext* ctx, std::uint64_t n, const BodyFn& body, std::uint64_t low, std::uint64_t high)
{
    return loop_range(ctx, n, std::tuple{}, body, low, high);
}

template <typename... Ts>
inline bool of(const ScanContext* ctx, std::uint64_t n, Ts... ids)
{
    return loop(ctx, n, [](auto ctx, auto&& /*vars*/, auto id) { return match_string(ctx, id); }, ids...);
}

inline std::uint64_t filesize(const ScanContext* ctx)
{
    return get_data_size(ctx);
}

template <Endian endian, typename T>
std::uint64_t read_data(const ScanContext* ctx, std::uint64_t offset)
{
    auto data_size = get_data_size(ctx);
    if (offset >= data_size || offset + sizeof(T) > data_size)
        return UNDEFINED;

    using UnsignedT = std::make_unsigned_t<T>;
    return endian_convert<endian, Endian::Native, UnsignedT>(*(UnsignedT*)get_data(ctx, offset));
}


#include "rules.def"


static bool evaluate_rule(const ScanContext* ctx, Rule& rule)
{
    if (rule.match == RuleMatch::NotEvaluated)
    {
        rule.match = rule.function(ctx) ? RuleMatch::Hit : RuleMatch::NoHit;
        if (rule.match == RuleMatch::Hit && rule.visibility == RuleVisibility::Public)
            ctx->scanner->match_callback(rule.name, ctx->user_data);
    }

    return rule.match == RuleMatch::Hit;
}

static bool evaluate_rule(const ScanContext* ctx, std::size_t id)
{
    return evaluate_rule(ctx, rules[id]);
}

static void evaluate_rules(const ScanContext* ctx)
{
    for (auto& rule : rules)
        evaluate_rule(ctx, rule);
}

static void add_match(ScanContext* ctx, std::size_t id, std::uint64_t offset, std::uint64_t length)
{
    ctx->matches[id].count++;
    ctx->matches[id].offsets.push_back(offset);
    ctx->matches[id].lengths.push_back(length);
}

static void add_mutex_match(ScanContext* ctx, std::size_t id)
{
    ctx->mutex_matches[id]++;
}

static const Match* get_match(const ScanContext* ctx, std::size_t id)
{
    return &ctx->matches[id];
}

static std::uint64_t get_mutex_match(const ScanContext* ctx, std::size_t id)
{
    return ctx->mutex_matches[id];
}

static const char* get_data(const ScanContext* ctx, std::uint64_t offset)
{
    return ctx->data + offset;
}

static std::uint64_t get_data_size(const ScanContext* ctx)
{
    return ctx->data_size;
}

extern "C" {

extern char* literal_db_start;
extern char* literal_db_end;
extern std::uint32_t literal_db_size;

extern char* regex_db_start;
extern char* regex_db_end;
extern std::uint32_t regex_db_size;

extern char* mutex_db_start;
extern char* mutex_db_end;
extern std::uint32_t mutex_db_size;

void yng_initialize()
{
    hs_deserialize_database((char*)&literal_db_start, (std::size_t)literal_db_size, &literal_db);
    hs_deserialize_database((char*)&regex_db_start, (std::size_t)regex_db_size, &regex_db);
    hs_deserialize_database((char*)&mutex_db_start, (std::size_t)mutex_db_size, &mutex_db);
}

Scanner* yng_new_scanner(MatchCallback match_callback)
{
    auto scanner = static_cast<Scanner*>(::malloc(sizeof(Scanner)));
    std::memset(scanner, 0, sizeof(Scanner));
    hs_alloc_scratch(literal_db, &scanner->literal_scratch);
    hs_alloc_scratch(regex_db, &scanner->regex_scratch);
    hs_alloc_scratch(mutex_db, &scanner->mutex_scratch);
    scanner->match_callback = match_callback;
    scanner->ctx = new ScanContext();
    scanner->ctx->scanner = scanner;
    return scanner;
}

void yng_free_scanner(Scanner* scanner)
{
    hs_free_scratch(scanner->literal_scratch);
    hs_free_scratch(scanner->regex_scratch);
    hs_free_scratch(scanner->mutex_scratch);
    delete scanner->ctx;
    ::free(scanner);
}

void yng_scan_data(Scanner* scanner, const char* data, std::size_t size, const char* cuckoo_file_path, void* user_data)
{
    for (auto& match : scanner->ctx->matches)
    {
        match.count = 0;
        match.offsets.clear();
        match.lengths.clear();
    }

    for (auto& match : scanner->ctx->mutex_matches)
    {
        match = 0;
    }

    for (auto& rule : rules)
        rule.match = RuleMatch::NotEvaluated;

    hs_error_t rc;

    if (literal_db_size != 0)
    {
        rc = hs_scan(
            literal_db,
            data,
            size,
            0,
            scanner->literal_scratch,
            &on_match,
            scanner->ctx
        );

        if (rc != HS_SUCCESS)
            throw std::runtime_error("Error while scanning with literal DB (" + std::to_string(rc) + ")");
    }

    if (regex_db_size != 0)
    {
        rc = hs_scan(
            regex_db,
            data,
            size,
            0,
            scanner->regex_scratch,
            &on_match,
            scanner->ctx
        );

        if (rc != HS_SUCCESS)
            throw std::runtime_error("Error while scanning with regex DB (" + std::to_string(rc) + ")");
    }

    if (cuckoo_file_path)
    {
        std::ifstream cuckoo_file{cuckoo_file_path, std::ios::in};
        cuckoo_file.seekg(0, std::ios::end);
        auto file_size = cuckoo_file.tellg();
        cuckoo_file.seekg(0, std::ios::beg);

        std::string cuckoo_file_data;
        cuckoo_file_data.reserve(file_size);
        cuckoo_file.read(cuckoo_file_data.data(), file_size);

        rapidjson::Document cuckoo_json;
        cuckoo_json.Parse(cuckoo_file_data.data());

        auto mutexes = cuckoo_json["behavior"]["summary"]["mutexes"].GetArray();
        std::size_t total_length = 0ul;
        for (auto& mutex : mutexes)
        {
            total_length += mutex.GetStringLength();
        }

        // length of all mutexes + '\n' for each mutex + null terminator
        auto mutex_data_size = total_length + mutexes.Size() + 1;
        auto mutex_data = std::make_unique<char[]>(mutex_data_size);
        char* mutex_data_write_ptr = mutex_data.get();
        for (auto& mutex : mutexes)
        {
            ::memcpy(mutex_data_write_ptr, mutex.GetString(), mutex.GetStringLength());
            mutex_data_write_ptr += mutex.GetStringLength();
            *mutex_data_write_ptr = '\n';
            mutex_data_write_ptr++;
        }
        *mutex_data_write_ptr = '\0';

        if (mutex_data_size != 0)
        {
            rc = hs_scan(
                mutex_db,
                mutex_data.get(),
                mutex_data_size,
                0,
                scanner->mutex_scratch,
                &on_mutex_match,
                scanner->ctx
            );

            if (rc != HS_SUCCESS)
                throw std::runtime_error("Error while scanning with mutex DB (" + std::to_string(rc) + ")");
        }
    }

    scanner->ctx->data = data;
    scanner->ctx->data_size = size;
    scanner->ctx->user_data = user_data;
    evaluate_rules(scanner->ctx);
}

void yng_finalize()
{
    hs_free_database(literal_db);
    hs_free_database(regex_db);
    hs_free_database(mutex_db);
}

}

static void print_hit(const char* rule, void* context)
{
    auto file = (const char*)context;
    ::printf("%s: %s\n", (const char*)context, rule);
}

int main(int argc, char* argv[])
{
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty())
    {
        std::cerr << "Usage: " << argv[0] << " FILE CUCKOO_FILE" << std::endl;
        return 1;
    }

    std::ifstream in_file(args[0], std::ios::binary);
    in_file.seekg(0, std::ios::end);
    auto file_size = in_file.tellg();
    in_file.seekg(0, std::ios::beg);

    std::vector<char> data(file_size);
    in_file.read(data.data(), data.size());

    yng_initialize();
    auto scanner = yng_new_scanner(print_hit);
    yng_scan_data(scanner, data.data(), data.size(), args.size() >= 2 ? args[1].c_str() : nullptr, (void*)args[0].c_str());
    yng_free_scanner(scanner);
    yng_finalize();
}
