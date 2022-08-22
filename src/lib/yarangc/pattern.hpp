#pragma once

#include <string>
#include <utility>

enum class PatternType
{
    Literal,
    Regex
};

enum class PatternClass
{
    String,
    Mutex,
    Event,
    Semaphore,
    Atom,
    Section,
    Job,
    Timer,
    Desktop
};

class Pattern
{
public:
    template <typename PatternT, typename RuleT, typename IdT>
    Pattern(PatternType type, PatternT&& pattern, RuleT&& rule, IdT&& id) : _type(type), _pattern(std::forward<PatternT>(pattern)), _rule(std::forward<RuleT>(rule)), _id(std::forward<IdT>(id)) {}
    Pattern(const Pattern&) = default;
    Pattern(Pattern&&) noexcept = default;

    Pattern& operator=(const Pattern&) = default;
    Pattern& operator=(Pattern&&) noexcept = default;

    bool is_literal() const { return _type == PatternType::Literal; }
    bool is_regex() const { return _type == PatternType::Regex; }

    const std::string& get_pattern() const { return _pattern; }
    const std::string& get_rule() const { return _rule; }
    const std::string& get_id()const { return _id; }

private:
    PatternType _type;
    std::string _pattern;
    std::string _rule;
    std::string _id;
};
