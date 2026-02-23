#pragma once
#include <string>
#include <vector>
#include "bb_types.h"

std::string readLineNice(const std::string& prompt,
                         const CommandMap& commands,
                         std::vector<std::string>& history);
