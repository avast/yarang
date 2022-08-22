#pragma once

#include <string>

struct ScannerInternal;

class Ruleset
{
public:
    using InitializeFn = void(*)();
    using MatchCallbackFn = void(*)(const char*, void*);
    using NewScannerFn = ScannerInternal*(*)(MatchCallbackFn);
    using ScanDataFn = void(*)(ScannerInternal*, char*, std::size_t, const char*, void*);
    using FreeScannerFn = void(*)(ScannerInternal*);
    using FinalizeFn = void(*)();

    class ScannerWrapper
    {
    public:
        ScannerWrapper(const Ruleset* parent, ScannerInternal* scanner) : _parent(parent), _scanner(scanner) {}
        ~ScannerWrapper()
        {
            _parent->free_scanner(_scanner);
        }

        ScannerInternal* get_scanner()
        {
            return _scanner;
        }

        void scan_data(char* data, std::size_t size, const char* cuckoo_file_path, void* context) const
        {
            _parent->scan_data(_scanner, data, size, cuckoo_file_path, context);
        }

    private:
        const Ruleset* _parent;
        ScannerInternal* _scanner;
    };

    Ruleset(const std::string& path);
    Ruleset(const Ruleset&) = delete;
    Ruleset(Ruleset&&) noexcept = default;
    ~Ruleset();

    void initialize() const;
    ScannerWrapper new_scanner(MatchCallbackFn match_callback) const;
    void scan_data(ScannerInternal* scanner, char* data, std::size_t size, const char* cuckoo_file_path, void* context) const;
    void free_scanner(ScannerInternal* scanner) const;
    void finalize() const;

private:
    template <typename T>
    T _load(const char* name);

    void* _handle;
    InitializeFn _initialize;
    NewScannerFn _new_scanner;
    ScanDataFn _scan_data;
    FreeScannerFn _free_scanner;
    FinalizeFn _finalize;
};
