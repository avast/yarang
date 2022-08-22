#pragma once

#include <algorithm>
#include <fstream>
#include <numeric>
#include <vector>
#include <span>

#ifndef ONLY_RUNTIME
#include <hs/hs.h>
#else
#include <hs/hs_runtime.h>
#endif

#include <hspp/error.hpp>
#include <hspp/pattern.hpp>

namespace hspp {

class Database
{
public:
    enum Flags
    {
        None = 0,
        ReportStart = 1,
        Multiline = 2,
        SingleMatch = 4
    };

    Database(std::uint32_t flags = Flags::None) : _db(nullptr), _flags(flags)
    {
    }

    Database(const Database&) = delete;
    Database(Database&& rhs) noexcept
    {
        std::swap(_db, rhs._db);
    }

    ~Database()
    {
        if (_db)
        {
            hs_free_database(_db);
            _db = nullptr;
        }
    }

    Database& operator=(const Database&) = delete;
    Database& operator=(Database&& rhs) noexcept
    {
        std::swap(_db, rhs._db);
        return *this;
    }

#ifndef ONLY_RUNTIME
    template <typename PatternT>
    void compile_regexes(const std::vector<PatternT>& patterns, unsigned int base_id = 0)
    {
        unsigned int flag = HS_FLAG_DOTALL | HS_FLAG_UTF8;
        if (_flags & Flags::ReportStart)
            flag |= HS_FLAG_SOM_LEFTMOST;
        if (_flags & Flags::Multiline)
            flag |= HS_FLAG_MULTILINE;
        if (_flags & Flags::SingleMatch)
            flag |= HS_FLAG_SINGLEMATCH;

        std::vector<const char*> expressions(patterns.size(), nullptr);
        std::vector<unsigned int> flags(patterns.size(), flag);
        std::vector<unsigned int> ids(patterns.size(), 0);

        for (std::size_t i = 0; i < patterns.size(); ++i)
        {
            expressions[i] = patterns[i]->get_pattern().c_str();
            flags[i] |= 0;
            ids[i] = base_id + i;
        }

        hs_compile_error_t* error;
        auto rc = hs_compile_multi(
            expressions.data(),
            flags.data(),
            ids.data(),
            patterns.size(),
            HS_MODE_BLOCK,
            nullptr,
            &_db,
            &error
        );

        if (rc != HS_SUCCESS)
            throw HyperScanError("Failed to compile regexes");
    }

    template <typename PatternT>
    void compile_literals(const std::vector<PatternT>& patterns, unsigned int base_id = 0)
    {
        unsigned int flag = 0;
        if (_flags & Flags::ReportStart)
            flag |= HS_FLAG_SOM_LEFTMOST;
        if (_flags & Flags::SingleMatch)
            flag |= HS_FLAG_SINGLEMATCH;

        std::vector<const char*> expressions(patterns.size(), nullptr);
        std::vector<unsigned int> flags(patterns.size(), flag);
        std::vector<unsigned int> ids(patterns.size(), 0);
        std::vector<std::size_t> lengths(patterns.size(), 0);

        for (std::size_t i = 0; i < patterns.size(); ++i)
        {
            expressions[i] = patterns[i]->get_pattern().c_str();
            flags[i] |= 0;
            ids[i] = base_id + i;
            lengths[i] = patterns[i]->get_pattern().length();
        }

        hs_compile_error_t* error;
        auto rc = hs_compile_lit_multi(
            expressions.data(),
            flags.data(),
            ids.data(),
            lengths.data(),
            patterns.size(),
            HS_MODE_BLOCK,
            nullptr,
            &_db,
            &error
        );

        if (rc != HS_SUCCESS)
            throw HyperScanError("Failed to compile literals");
    }
#endif

    void save(const std::string& path) const
    {
        std::size_t db_size = 0;
        hs_database_size(_db, &db_size);

        auto db_bytes = (char*)::malloc(db_size);
        if (hs_serialize_database(_db, &db_bytes, &db_size) != HS_SUCCESS)
            throw HyperScanError("Failed to serialize database");

        std::ofstream out_file(path, std::ios::binary | std::ios::trunc);
        out_file.write(db_bytes, db_size);
        out_file.close();

        ::free(db_bytes);
    }

    void load(const std::string& path)
    {
        std::ifstream in_file(path, std::ios::binary);
        in_file.seekg(0, std::ios::end);
        auto size = in_file.tellg();
        in_file.seekg(0, std::ios::beg);

        std::vector<char> data(size);
        in_file.read(data.data(), data.size());

        if (hs_deserialize_database(data.data(), data.size(), &_db) != HS_SUCCESS)
            throw HyperScanError("Failed to deserialize database");
    }

    //template <typename MatchingContext>
    //void scan(const std::span<std::uint8_t>& data, const MatchingContext& matching_context)
    //{
    //    hs_scan(_db, data.data(), data.size(), 0, 
    //}

private:
    hs_database_t* _db;
    std::uint32_t _flags;
};

} // namespace hspp
