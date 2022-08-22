#include <yarangc/conversion.hpp>

using namespace std::literals;

namespace {

char byte_to_hex_char(std::uint8_t byte)
{
    if (byte < 0xA)
        return static_cast<char>(byte + '0');
    else if (byte >= 0xA && byte <= 0xF)
        return static_cast<char>(byte - 0xA + 'A');

    return 'X';
}

std::uint8_t hex_char_to_byte(char ch)
{
    if ('0' <= ch && ch <= '9')
        return ch - '0';
    else if ('A' <= ch && ch <= 'F')
        return ch - 'A' + 0xA;

    return 0xFF;
}

std::string nibbles_to_string(char high, char low)
{
    int ch = (static_cast<int>(hex_char_to_byte(high)) << 4) | hex_char_to_byte(low);
    if (std::isalnum(ch))
        return std::string(1, static_cast<char>(ch));
    else
        return "\\x"s + high + low;
}

void hex_string_to_pattern(std::string& pattern, bool& literal_only, const std::vector<std::shared_ptr<yaramod::HexStringUnit>>& units)
{
    std::vector<char> str_units;
    str_units.reserve(2);

    for (const auto& unit : units)
    {
        if (unit->isNibble() || unit->isWildcard())
        {
            str_units.emplace_back(unit->getText()[0]);
            if (str_units.size() < 2)
                continue;

            if (str_units[0] == '?' && str_units[1] == '?')
            {
                literal_only = false;
                pattern += ".";
            }
            else if (str_units[0] == '?')
            {
                literal_only = false;
                pattern += "(";
                for (int i = 0; i < 0x10; ++i)
                {
                    pattern += nibbles_to_string(byte_to_hex_char(i), str_units[1]);;
                    if (i < 0xF)
                        pattern += "|";
                }
                pattern += ")";
            }
            else if (str_units[1] == '?')
            {
                literal_only = false;
                pattern += "(";
                for (int i = 0; i < 0x10; ++i)
                {
                    pattern += nibbles_to_string(str_units[0], byte_to_hex_char(i));;
                    if (i < 0xF)
                        pattern += "|";
                }
                pattern += ")";
            }
            else
                pattern += nibbles_to_string(str_units[0], str_units[1]);

            str_units.clear();
        }
        else if (unit->isJump())
        {
            literal_only = false;
            auto jump_unit = static_cast<const yaramod::HexStringJump*>(unit.get());
            std::string low_jump_str, high_jump_str;
            if (jump_unit->getLow())
                low_jump_str = std::to_string(jump_unit->getLow().value());
            if (jump_unit->getHigh())
                high_jump_str = std::to_string(jump_unit->getHigh().value());

            if (low_jump_str.empty() && high_jump_str.empty())
                pattern += ".*";
            else
            {
                pattern += ".{";
                if (low_jump_str == high_jump_str)
                    pattern += low_jump_str;
                else
                    pattern += low_jump_str + "," + high_jump_str;
                pattern += "}";
            }
        }
        else if (unit->isOr())
        {
            literal_only = false;
            pattern += "(";
            const auto& substrings = static_cast<const yaramod::HexStringOr*>(unit.get())->getSubstrings();
            for (auto itr = substrings.begin(), end = substrings.end(); itr != end; ++itr)
            {
                hex_string_to_pattern(pattern, literal_only, (*itr)->getUnits());
                if (itr + 1 != end)
                    pattern += "|";
            }
            pattern += ")";
        }
    }
}

}

std::unique_ptr<Pattern> hex_string_to_pattern(const std::string& rule, const yaramod::HexString& hex_string)
{
    std::string result;
    bool literal_only = true;
    hex_string_to_pattern(result, literal_only, hex_string.getUnits());
    return std::make_unique<Pattern>(
        literal_only ? PatternType::Literal : PatternType::Regex,
        std::move(result),
        rule,
        hex_string.getIdentifier()
    );
}
