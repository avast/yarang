#include <stdexcept>

#include <dlfcn.h>

#include <yarang/ruleset.hpp>

Ruleset::Ruleset(const std::string& path)
{
    // If we haven't used realpath, we'd have to modify LD_LIBRARY_PATH
    auto full_path = ::realpath(path.c_str(), nullptr);
    _handle = ::dlopen(full_path, RTLD_NOW);
    if (_handle == nullptr)
    {
        ::free(full_path);
        throw std::runtime_error(::dlerror());
    }

    _initialize = _load<InitializeFn>("yng_initialize");
    _new_scanner = _load<NewScannerFn>("yng_new_scanner");
    _scan_data = _load<ScanDataFn>("yng_scan_data");
    _free_scanner = _load<FreeScannerFn>("yng_free_scanner");
    _finalize = _load<FinalizeFn>("yng_finalize");

    _initialize();
    ::free(full_path);
}

Ruleset::~Ruleset()
{
    if (_finalize)
        _finalize();

    if (_handle)
        ::dlclose(_handle);
}

void Ruleset::initialize() const
{
    _initialize();
}

Ruleset::ScannerWrapper Ruleset::new_scanner(MatchCallbackFn match_callback) const
{
    return {this, _new_scanner(match_callback)};
}

void Ruleset::scan_data(ScannerInternal* scanner, char* data, std::size_t size, const char* cuckoo_file_path, void* context) const
{
    return _scan_data(scanner, data, size, cuckoo_file_path, context);
}

void Ruleset::free_scanner(ScannerInternal* scanner) const
{
    return _free_scanner(scanner);
}

void Ruleset::finalize() const
{
    return _finalize();
}

template <typename T>
T Ruleset::_load(const char* name)
{
    auto result = reinterpret_cast<T>(::dlsym(_handle, name));
    if (result == nullptr)
        throw std::runtime_error("Unable to load symbol from the binary ruleset");
    return result;
}
