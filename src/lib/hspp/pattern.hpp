#pragma once

#include <string>

namespace hspp {

class Pattern
{
public:
    template <typename StrT1, typename StrT2, typename StrT3>
    Pattern(StrT1&& pattern, StrT2&& rule, StrT3&& id, int flags = 0) : _pattern(std::forward<StrT1>(pattern)), _rule(std::forward<StrT2>(rule)), _id(std::forward<StrT3>(id)), _flags(flags) {}
    Pattern(const Pattern&) = default;
    Pattern(Pattern&&) noexcept = default;

    Pattern& operator=(const Pattern&) = default;
    Pattern& operator=(Pattern&) noexcept = default;

    const std::string& get_pattern() const { return _pattern; }
    const std::string& get_rule() const { return _rule; }
    const std::string& get_id() const { return _id; }
    int get_flags() const { return _flags; }

private:
    std::string _pattern;
    std::string _rule;
    std::string _id;
    int _flags;
};

} // namespace hspp
