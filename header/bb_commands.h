#ifndef BB_COMMANDS_H
#define BB_COMMANDS_H

#include <unordered_map>
#include <functional>
#include <string>
#include <vector>

using CommandMap = std::unordered_map<std::string, std::function<void(const std::vector<std::string>&)>>;

void registerCommands(CommandMap& commands);
std::string findCommand(const std::string& cmd);

#endif