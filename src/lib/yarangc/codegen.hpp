#pragma once

#include <iterator>
#include <unordered_map>
#include <sstream>
#include <vector>

#include <yaramod/utils/observing_visitor.h>
#include <yaramod/yaramod.h>

#include <yarangc/pattern_extractor.hpp>

//class Expression {};
//
//class And : public Expression {};
//class Or : public Expression {};
//class Eq : public Expression {};
//class Neq : public Expression {};
//class Not : public Expression {};
//class Lt : public Expression {};
//class Le : public Expression {};
//class Gt : public Expression {};
//class Ge : public Expression {};

class Codegen : public yaramod::ObservingVisitor
{
public:
    Codegen(const PatternExtractor* pattern_extractor) : _pattern_extractor(pattern_extractor)
    {
    }

    void generate(const yaramod::YaraFile* yara_file)
    {
        _out << "#define PATTERN_COUNT " << _pattern_extractor->get_literal_patterns().size() + _pattern_extractor->get_regex_patterns().size() << "\n";
        _out << "#define MUTEX_PATTERN_COUNT " << _pattern_extractor->get_mutex_patterns().size();
        _result.push_back(_out.str());
        _out.str(std::string{});
        _out.clear();

        _result.push_back("struct ScanContext\n"
            "{\n"
            "    Scanner* scanner;\n"
            "    const char* data;\n"
            "    std::size_t data_size;\n"
            "    void* user_data;\n"
            "    Match matches[PATTERN_COUNT];\n"
            "    std::uint64_t mutex_matches[MUTEX_PATTERN_COUNT];\n"
            "};"
        );

        for (auto&& rule : yara_file->getRules())
            generate(rule.get());

        _out << "static auto rules = std::array<Rule, " << yara_file->getRules().size() << ">{\n";
        for (auto&& rule : yara_file->getRules())
            _out << "Rule{\"" << rule->getName() << "\", RuleVisibility::" << (rule->isPrivate() ? "Private" : "Public")
                << ", &rule_" << rule->getName() << ", RuleMatch::NotEvaluated},\n";
        _out << "};";
        _result.push_back(_out.str());
    }

    const std::string& generate(const yaramod::Rule* rule)
    {
        _rule = rule;
        _rule_info = &_pattern_extractor->get_rule_info_table().at(_rule->getName());

        _out << "static bool rule_" << rule->getName() << "(const ScanContext* ctx)\n"
            << "{\n"
            << "return ";

        observe(rule->getCondition());

        _out << ";\n}";

        _result.push_back(_out.str());
        _out.str(std::string{});
        _out.clear();

        return _result.back();
    }

    std::string get_result()
    {
        _out.str(std::string{});
        _out.clear();

        for (std::size_t i = 0; i < _result.size(); ++i)
        {
            _out << _result[i];
            if (i < _result.size() - 1)
                _out << "\n\n";
        }

        return _out.str();
    }

    virtual yaramod::VisitResult visit(yaramod::AndExpression* expr) override
    {
        expr->getLeftOperand()->accept(this);
        _out << "\n&& ";
        expr->getRightOperand()->accept(this);
        return {};
    }

    virtual yaramod::VisitResult visit(yaramod::OrExpression* expr) override
    {
        expr->getLeftOperand()->accept(this);
        _out << "\n|| ";
        expr->getRightOperand()->accept(this);
        return {};
    }

    virtual yaramod::VisitResult visit(yaramod::NotExpression* expr) override
    {
        _out << "!";
        expr->getOperand()->accept(this);
        return {};
    }

    virtual yaramod::VisitResult visit(yaramod::OfExpression* expr) override
    {
        std::vector<std::uint64_t> ids;
        if (dynamic_cast<yaramod::ThemExpression*>(expr->getIterable().get()))
            ids = get_string_ids();
        else if (auto set_expr = dynamic_cast<yaramod::SetExpression*>(expr->getIterable().get()))
            ids = get_string_ids_from_set(set_expr);

        std::uint64_t count = 0;
        auto* var_expr = expr->getVariable().get();
        if (dynamic_cast<yaramod::AnyExpression*>(var_expr))
            count = 1;
        else if (dynamic_cast<yaramod::AllExpression*>(var_expr))
            count = ids.size();
        else if (auto int_expr = dynamic_cast<yaramod::IntLiteralExpression*>(var_expr))
            count = std::max(static_cast<std::uint64_t>(1), std::min(ids.size(), int_expr->getValue()));

        _out << "of(ctx, " << count << "ul, ";
        for (std::size_t i = 0; i < ids.size(); ++i)
        {
            _out << ids[i] << "ul";
            if (i < ids.size() - 1)
                _out << ", ";
        }
        _out << ")";
        return {};
    }

    virtual yaramod::VisitResult visit(yaramod::ForStringExpression* expr) override
    {
        std::vector<std::uint64_t> ids;
        if (dynamic_cast<yaramod::ThemExpression*>(expr->getIterable().get()))
            ids = get_string_ids();
        else if (auto set_expr = dynamic_cast<yaramod::SetExpression*>(expr->getIterable().get()))
            ids = get_string_ids_from_set(set_expr);

        std::uint64_t count = 0;
        auto* var_expr = expr->getVariable().get();
        if (dynamic_cast<yaramod::AnyExpression*>(var_expr))
            count = 1;
        else if (dynamic_cast<yaramod::AllExpression*>(var_expr))
            count = ids.size();
        else if (auto int_expr = dynamic_cast<yaramod::IntLiteralExpression*>(var_expr))
            count = std::max(static_cast<std::uint64_t>(1), std::min(ids.size(), int_expr->getValue()));

        _out << "loop(ctx, " << count << "ul, ";
        if (!_loop_vars.empty())
            _out << "std::move(vars), ";
        _out << "[](auto ctx, auto&& vars, auto id) {\nreturn ";
        expr->getBody()->accept(this);
        _out << ";\n}, ";
        for (std::size_t i = 0; i < ids.size(); ++i)
        {
            _out << ids[i] << "ul";
            if (i < ids.size() - 1)
                _out << ", ";
        }
        _out << ")";
        return {};
    }

    virtual yaramod::VisitResult visit(yaramod::ForArrayExpression* expr) override
    {
        yaramod::SetExpression* set_expr = nullptr;
        yaramod::RangeExpression* range_expr = nullptr;

        if ((set_expr = dynamic_cast<yaramod::SetExpression*>(expr->getIterable().get())) != nullptr)
            ;
        else if ((range_expr = dynamic_cast<yaramod::RangeExpression*>(expr->getIterable().get())) != nullptr)
            ;

        std::uint64_t count = 0, total_count = set_expr ? set_expr->getElements().size() : static_cast<std::uint64_t>(-1ULL);
        auto* var_expr = expr->getVariable().get();
        if (dynamic_cast<yaramod::AnyExpression*>(var_expr))
            count = 1;
        else if (dynamic_cast<yaramod::AllExpression*>(var_expr))
            count = total_count;
        else if (auto int_expr = dynamic_cast<yaramod::IntLiteralExpression*>(var_expr))
            count = std::max(static_cast<std::uint64_t>(1), std::min(total_count, int_expr->getValue()));

        _out << (set_expr ? "loop_ints" : "loop_range") << "(ctx, " << count << "ul, ";
        if (!_loop_vars.empty())
            _out << "std::move(vars), ";
        _out << "[](auto ctx, auto&& vars, auto id) {\nreturn ";
        _loop_vars.push_back(expr->getId());
        expr->getBody()->accept(this);
        _out << ";\n}, ";
        _loop_vars.pop_back();

        if (set_expr)
        {
            const auto& elems = set_expr->getElements();
            for (std::size_t i = 0; i < set_expr->getElements().size(); ++i)
            {
                elems[i]->accept(this);
                if (i < elems.size() - 1)
                    _out << ", ";
            }
        }
        else
            range_expr->accept(this);

        _out << ")";
        return {};
    }

    //virtual yaramod::VisitResult visit(yaramod::SetExpression* expr) override
    //{
    //    std::cout << "set expr (size=" << expr->getElements().size() << ")" << std::endl;
    //    for (auto& elem : expr->getElements())
    //        elem->accept(this);
    //    return {};
    //}

    virtual yaramod::VisitResult visit(yaramod::StringExpression* expr) override
    {
        _out << "match_string(ctx, " << _rule_info->get_string_id(expr->getId()) << "ul)";
        return {};
    }

    //virtual yaramod::VisitResult visit(yaramod::StringWildcardExpression* expr) override
    //{
    //    auto ids = _rule_info->get_string_wildcard_ids(expr->getId());
    //    std::cout << "match(matches, ";
    //    for (std::size_t i = 0; i < ids.size(); ++i)
    //    {
    //        std::cout << ids[i];
    //        if (i < ids.size() - 1)
    //            std::cout << ", ";
    //    }
    //    std::cout << ")";
    //    return {};
    //}

    virtual yaramod::VisitResult visit(yaramod::PlusExpression* expr) override
    {
        expr->getLeftOperand()->accept(this);
        _out << " + ";
        expr->getRightOperand()->accept(this);
        return {};
    }

    virtual yaramod::VisitResult visit(yaramod::MinusExpression* expr) override
    {
        expr->getLeftOperand()->accept(this);
        _out << " - ";
        expr->getRightOperand()->accept(this);
        return {};
    }

    virtual yaramod::VisitResult visit(yaramod::MultiplyExpression* expr) override
    {
        expr->getLeftOperand()->accept(this);
        _out << " * ";
        expr->getRightOperand()->accept(this);
        return {};
    }

    virtual yaramod::VisitResult visit(yaramod::ModuloExpression* expr) override
    {
        expr->getLeftOperand()->accept(this);
        _out << " % ";
        expr->getRightOperand()->accept(this);
        return {};
    }

    virtual yaramod::VisitResult visit(yaramod::BitwiseAndExpression* expr) override
    {
        expr->getLeftOperand()->accept(this);
        _out << " & ";
        expr->getRightOperand()->accept(this);
        return {};
    }

    virtual yaramod::VisitResult visit(yaramod::BitwiseXorExpression* expr) override
    {
        expr->getLeftOperand()->accept(this);
        _out << " ^ ";
        expr->getRightOperand()->accept(this);
        return {};
    }

    virtual yaramod::VisitResult visit(yaramod::EqExpression* expr) override
    {
        expr->getLeftOperand()->accept(this);
        _out << " == ";
        expr->getRightOperand()->accept(this);
        return {};
    }

    virtual yaramod::VisitResult visit(yaramod::NeqExpression* expr) override
    {
        expr->getLeftOperand()->accept(this);
        _out << " == ";
        expr->getRightOperand()->accept(this);
        return {};
    }

    virtual yaramod::VisitResult visit(yaramod::LtExpression* expr) override
    {
        expr->getLeftOperand()->accept(this);
        _out << " < ";
        expr->getRightOperand()->accept(this);
        return {};
    }

    virtual yaramod::VisitResult visit(yaramod::LeExpression* expr) override
    {
        expr->getLeftOperand()->accept(this);
        _out << " <= ";
        expr->getRightOperand()->accept(this);
        return {};
    }

    virtual yaramod::VisitResult visit(yaramod::GtExpression* expr) override
    {
        expr->getLeftOperand()->accept(this);
        _out << " > ";
        expr->getRightOperand()->accept(this);
        return {};
    }

    virtual yaramod::VisitResult visit(yaramod::GeExpression* expr) override
    {
        expr->getLeftOperand()->accept(this);
        _out << " >= ";
        expr->getRightOperand()->accept(this);
        return {};
    }

    virtual yaramod::VisitResult visit(yaramod::StringAtExpression* expr) override
    {
        _out << "match_at(ctx, ";
        auto id = expr->getId();
        if (id == "$")
            _out << "id";
        else
            _out << _rule_info->get_string_id(expr->getId()) << "ul";
        _out << ", ";
        expr->getAtExpression()->accept(this);
        _out << ")";
        return {};
    }

    virtual yaramod::VisitResult visit(yaramod::StringInRangeExpression* expr) override
    {
        _out << "match_in(ctx, ";
        auto id = expr->getId();
        if (id == "$")
            _out << "id";
        else
            _out << _rule_info->get_string_id(expr->getId()) << "ul";
        _out << ", ";
        expr->getRangeExpression()->accept(this);
        _out << ")";
        return {};
    }

    virtual yaramod::VisitResult visit(yaramod::StringCountExpression* expr) override
    {
        _out << "match_count(ctx, " << _rule_info->get_string_id("$" + expr->getId().substr(1)) << "ul)";
        return {};
    }

    virtual yaramod::VisitResult visit(yaramod::StringLengthExpression* expr) override
    {
        _out << "match_length(ctx, " << _rule_info->get_string_id("$" + expr->getId().substr(1)) << "ul";
        if (auto index_expr = expr->getIndexExpression(); index_expr)
        {
            _out << ", ";
            index_expr->accept(this);
            _out << " - 1ul";
        }
        _out << ")";
        return {};
    }

    virtual yaramod::VisitResult visit(yaramod::StringOffsetExpression* expr) override
    {
        _out << "match_offset(ctx, ";
        auto id = expr->getId();
        if (id == "@")
            _out << "id";
        else
            _out << _rule_info->get_string_id("$" + id.substr(1)) << "ul";
        if (auto index_expr = expr->getIndexExpression(); index_expr)
        {
            _out << ", ";
            index_expr->accept(this);
            _out << " - 1ul";
        }
        _out << ")";
        return {};
    }

    virtual yaramod::VisitResult visit(yaramod::IdExpression* expr) override
    {
        // id can be
        // - variable inside loop
        // - rule
        // - external variable
        // in this order
        auto name = expr->getSymbol()->getName();
        if (auto var_itr = std::find(_loop_vars.begin(), _loop_vars.end(), name); var_itr != _loop_vars.end())
            _out << "std::get<" << (var_itr - _loop_vars.begin()) << ">(vars)";
        else if (auto rule_itr = _pattern_extractor->get_rule_info_table().find(expr->getSymbol()->getName()); rule_itr != _pattern_extractor->get_rule_info_table().end())
            _out << "evaluate_rule(ctx, " << rule_itr->second.id << ")";
        return {};
    }

    virtual yaramod::VisitResult visit(yaramod::IntLiteralExpression* expr) override
    {
        _out << expr->getValue() << "ul";
        return {};
    }

    virtual yaramod::VisitResult visit(yaramod::BoolLiteralExpression* expr) override
    {
        _out << (expr->getValue() ? "true" : "false");
        return {};
    }

    virtual yaramod::VisitResult visit(yaramod::ParenthesesExpression* expr) override
    {
        _out << "(";
        expr->getEnclosedExpression()->accept(this);
        _out << ")";
        return {};
    }

    virtual yaramod::VisitResult visit(yaramod::RangeExpression* expr) override
    {
        expr->getLow()->accept(this);
        _out << ", ";
        expr->getHigh()->accept(this);
        return {};
    }

    virtual yaramod::VisitResult visit(yaramod::IntFunctionExpression* expr) override
    {
        auto function_name = expr->getFunction();
        auto endianess = "Endian::Little";
        if (auto pos = function_name.find("be"); pos != std::string::npos)
        {
            function_name = function_name.substr(0, pos);
            endianess = "Endian::Big";
        }

        _out << "read_data<" << endianess << ", std::" << function_name << "_t>(ctx, ";
        expr->getArgument()->accept(this);
        _out << ")";
        return {};
    }

    virtual yaramod::VisitResult visit(yaramod::FunctionCallExpression* expr) override
    {
        if (expr->getFunction()->getText() == "cuckoo.sync.mutex")
        {
            auto pattern = expr->getArguments()[0]->as<yaramod::RegexpExpression>()->getRegexpString()->getPureText();
            _out << "get_mutex_match(ctx, " << _rule_info->get_mutex_id(pattern) << "ul) > 0";
        }
        else
        {
            for (auto&& arg : expr->getArguments())
                arg->accept(this);
        }

        return {};
    }

    virtual yaramod::VisitResult visit(yaramod::FilesizeExpression*) override
    {
        _out << "filesize(ctx)";
        return {};
    }

    //virtual yaramod::VisitResult visit(yaramod::AllExpression* expr) override
    //{
    //    std::cout << "all expr" << std::endl;
    //    return {};
    //}

    //virtual yaramod::VisitResult visit(yaramod::AnyExpression* expr) override
    //{
    //    std::cout << "any expr" << std::endl;
    //    return {};
    //}

    //virtual yaramod::VisitResult visit(yaramod::ThemExpression* expr) override
    //{
    //    std::cout << "them expr" << std::endl;
    //    return {};
    //}

private:
    std::vector<std::uint64_t> get_string_ids() const
    {
        auto strings = _rule->getStrings();
        std::vector<std::uint64_t> result(strings.size());
        std::transform(strings.begin(), strings.end(), result.begin(), [this](const auto& string) {
            return _rule_info->get_string_id(string->getIdentifier());
        });
        return result;
    }

    std::vector<std::uint64_t> get_string_ids_from_set(const yaramod::SetExpression* set_expr) const
    {
        std::vector<std::uint64_t> result;
        for (auto& elem_expr : set_expr->getElements())
        {
            if (auto str_expr = dynamic_cast<yaramod::StringExpression*>(elem_expr.get()))
            {
                result.push_back(_rule_info->get_string_id(str_expr->getId()));
            }
            else if (auto str_expr = dynamic_cast<yaramod::StringWildcardExpression*>(elem_expr.get()))
            {
                auto ids = _rule_info->get_string_wildcard_ids(str_expr->getId());
                std::copy(ids.begin(), ids.end(), std::back_inserter(result));
            }
        }
        return result;
    }

    const PatternExtractor* _pattern_extractor;
    const yaramod::Rule* _rule;
    const RuleInfo* _rule_info;
    std::ostringstream _out;
    std::vector<std::string> _result;

    std::vector<std::string> _loop_vars;
};
