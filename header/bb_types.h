#ifndef BB_TYPES_H
#define BB_TYPES_H

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

using CommandMap = std::unordered_map<std::string, std::function<void(const std::vector<std::string>&)>>;

#endif
