#ifndef BB_EXEC_H
#define BB_EXEC_H

#include <string>
#include <vector>

int cmdArgs(const std::vector<std::string>& parts);
int cmd(const std::string& line);
int lastCmdStatus();

#endif
