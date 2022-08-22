#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>
#include <vector>

#include <yaramod/yaramod.h>

#include <hspp/database.hpp>
#include <yarangc/codegen.hpp>
#include <yarangc/pattern_extractor.hpp>

int main(int argc, char* argv[])
{
    if (argc != 2)
        return 2;

    yaramod::Yaramod ymod;

    try
    {
        auto ruleset_file_path = std::string{argv[1]};
        auto ruleset = ymod.parseFile(ruleset_file_path);
        if (!ruleset)
            return 1;

        PatternExtractor extractor;
        extractor.extract(ruleset.get());

        const auto& regexes = extractor.get_regex_patterns();
        const auto& literals = extractor.get_literal_patterns();
        const auto& mutexes = extractor.get_mutex_patterns();

        std::ofstream patterns(std::filesystem::path{ruleset_file_path}.parent_path() / "patterns.txt");
        for (std::size_t i = 0; i < regexes.size(); ++i)
            patterns << i << " " << regexes[i]->get_rule() << ":" << regexes[i]->get_id() << " R " << regexes[i]->get_pattern() << "\n";
        for (std::size_t i = 0; i < literals.size(); ++i)
            patterns << regexes.size() + i << " " << literals[i]->get_rule() << ":" << literals[i]->get_id() << " R " << literals[i]->get_pattern() << "\n";
        for (std::size_t i = 0; i < mutexes.size(); ++i)
            patterns << i << " " << mutexes[i]->get_rule() << ":" << mutexes[i]->get_id() << " R " << mutexes[i]->get_pattern() << "\n";
        patterns.close();

        hspp::Database db_regex{hspp::Database::Flags::ReportStart};
        hspp::Database db_literal{hspp::Database::Flags::ReportStart};
        hspp::Database db_mutex{hspp::Database::Flags::Multiline | hspp::Database::Flags::SingleMatch};

        if (!regexes.empty())
          db_regex.compile_regexes(regexes);
        if (!literals.empty())
          db_literal.compile_literals(literals, regexes.size());
        if (!mutexes.empty())
          db_mutex.compile_regexes(mutexes);

        Codegen codegen(&extractor);
        codegen.generate(ruleset.get());

        std::ofstream rules(std::filesystem::path{ruleset_file_path}.parent_path() / "rules.def");
        rules << codegen.get_result() << std::endl;
        rules.close();

        if (!regexes.empty())
          db_regex.save(ruleset_file_path + ".regex.db");
        if (!literals.empty())
          db_literal.save(ruleset_file_path + ".literal.db");
        if (!mutexes.empty())
          db_mutex.save(ruleset_file_path + ".mutex.db");
    }
    catch (const yaramod::YaramodError& error)
    {
        std::cerr << error.getErrorMessage() << std::endl;
    }

    return 0;
}
