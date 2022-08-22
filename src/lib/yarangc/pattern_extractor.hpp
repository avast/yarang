#pragma once

#include <yaramod/types/expressions.h>
#include <yaramod/utils/observing_visitor.h>
#include <yaramod/yaramod.h>

#include <yarangc/conversion.hpp>
#include <yarangc/pattern.hpp>

using StringInfoTable = std::unordered_map<std::string, std::uint64_t>;

struct RuleInfo
{
    std::uint64_t get_string_id(const std::string& key) const
    {
        if (auto itr = literal_strings.find(key); itr != literal_strings.end())
            return itr->second;

        if (auto itr = regex_strings.find(key); itr != regex_strings.end())
            return itr->second;

        throw std::runtime_error("RuleInfo: unexpected string id");
    }

    std::uint64_t get_mutex_id(const std::string& pattern) const
    {
        if (auto itr = mutexes.find(pattern); itr != mutexes.end())
            return itr->second;

        throw std::runtime_error("RuleInfo: unexpected mutex pattern");
    }

    std::vector<std::uint64_t> get_string_wildcard_ids(std::string key) const
    {
        auto strings = rule->getStringsTrie()->getValuesWithPrefix(key.substr(0, key.length() - 1));
        std::vector<std::uint64_t> result(strings.size());
        std::transform(strings.begin(), strings.end(), result.begin(), [this](const auto& string) {
            return get_string_id(string->getIdentifier());
        });
        return result;
    }

    std::uint64_t id;
    const yaramod::Rule* rule;
    StringInfoTable literal_strings;
    StringInfoTable regex_strings;
    StringInfoTable mutexes;
};

using RuleInfoTable = std::unordered_map<std::string, RuleInfo>;

class PatternExtractor : public yaramod::ObservingVisitor
{
public:
    void extract(const yaramod::YaraFile* yara_file)
    {
        std::uint64_t rule_index = 0;
        for (auto& rule : yara_file->getRules())
        {
            auto [rule_info_itr, rule_inserted] = _rule_info_table.emplace(rule->getName(), RuleInfo{rule_index, rule.get(), {}, {}});
            auto& rule_info = rule_info_itr->second;

            std::uint64_t literal_index = 0, regex_index = 0;
            for (const auto& string : rule->getStrings())
            {
                if (string->isPlain())
                {
                    auto pattern = string->getPureText();
                    auto [itr, inserted] = _literal_cache.emplace(pattern, literal_index);
                    if (inserted)
                        _literal_patterns.emplace_back(std::make_unique<Pattern>(PatternType::Literal, std::move(pattern), rule->getName(), string->getIdentifier()));
                    rule_info.literal_strings.emplace(string->getIdentifier(), inserted ? literal_index++ : itr->second);

                }
                else if (string->isRegexp())
                {
                    auto pattern = string->getPureText();
                    auto [itr, inserted] = _regex_cache.emplace(pattern, regex_index);
                    if (inserted)
                        _regex_patterns.emplace_back(std::make_unique<Pattern>(PatternType::Regex, std::move(pattern), rule->getName(), string->getIdentifier()));
                    rule_info.regex_strings.emplace(string->getIdentifier(), inserted ? regex_index++ : itr->second);
                }
                else if (string->isHex())
                {
                    auto pattern = hex_string_to_pattern(rule->getName(), *static_cast<const yaramod::HexString*>(string));
                    if (pattern->is_literal())
                    {
                        auto [itr, inserted] = _literal_cache.emplace(pattern->get_pattern(), literal_index);
                        if (inserted)
                            _literal_patterns.emplace_back(std::move(pattern));
                        rule_info.literal_strings.emplace(string->getIdentifier(), inserted ? literal_index++ : itr->second);
                    }
                    else if (pattern->is_regex())
                    {
                        auto [itr, inserted] = _regex_cache.emplace(pattern->get_pattern(), regex_index);
                        if (inserted)
                            _regex_patterns.emplace_back(std::move(pattern));
                        rule_info.regex_strings.emplace(string->getIdentifier(), inserted ? regex_index++ : itr->second);
                    }
                }
            }

            _current_rule_info = &rule_info;
            observe(rule->getCondition());

            rule_index++;
        }

        // Fix literal indices
        auto total_regex_size = std::accumulate(_rule_info_table.begin(), _rule_info_table.end(), static_cast<std::size_t>(0), [](std::size_t sum, const auto& rule_info) {
            return sum + rule_info.second.regex_strings.size();
        });
        for (auto& [rule_name, rule_info] : _rule_info_table)
        {
            for (auto& [string_id, string_info] : rule_info.literal_strings)
                string_info += total_regex_size;
        }
    }

    const RuleInfoTable& get_rule_info_table() const { return _rule_info_table; }
    std::vector<std::unique_ptr<Pattern>>&& get_literal_patterns() { return std::move(_literal_patterns); }
    std::vector<std::unique_ptr<Pattern>>&& get_regex_patterns() { return std::move(_regex_patterns); }
    const std::vector<std::unique_ptr<Pattern>>& get_literal_patterns() const { return _literal_patterns; }
    const std::vector<std::unique_ptr<Pattern>>& get_regex_patterns() const { return _regex_patterns; }
    const std::vector<std::unique_ptr<Pattern>>& get_mutex_patterns() const { return _mutexes; }

    virtual yaramod::VisitResult visit(yaramod::FunctionCallExpression* expr) override
    {
        if (expr->getFunction()->getText() == "cuckoo.sync.mutex")
        {
            auto pattern = expr->getArguments()[0]->as<yaramod::RegexpExpression>()->getRegexpString()->getPureText();
            auto mutex_idx = _mutexes.size();
            auto [itr, inserted] = _mutex_cache.emplace(pattern, mutex_idx);
            if (inserted)
                _mutexes.emplace_back(std::make_unique<Pattern>(PatternType::Regex, pattern, _current_rule_info->rule->getName(), pattern));
            _current_rule_info->mutexes.emplace(std::move(pattern), inserted ? mutex_idx : itr->second);
        }
        else
        {
            for (auto&& arg : expr->getArguments())
                arg->accept(this);
        }

        return {};
    }

private:
    RuleInfoTable _rule_info_table;
    RuleInfo* _current_rule_info;
    std::vector<std::unique_ptr<Pattern>> _literal_patterns, _regex_patterns;
    std::vector<std::unique_ptr<Pattern>> _mutexes;
    std::unordered_map<std::string, std::uint64_t> _literal_cache, _regex_cache;
    std::unordered_map<std::string, std::uint64_t> _mutex_cache;
};
