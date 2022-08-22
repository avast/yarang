#pragma once

#include <exception>

namespace hspp {

class HyperScanError : public std::exception
{
public:
    HyperScanError(const char* msg) : _msg(msg) {}

    virtual const char* what() const noexcept override { return _msg; }

private:
    const char* _msg;
};

} // namespace hspp
