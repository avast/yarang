#pragma once

#include <yaramod/yaramod.h>

#include <yarangc/pattern.hpp>

std::unique_ptr<Pattern> hex_string_to_pattern(const std::string& rule, const yaramod::HexString& hex_string);
