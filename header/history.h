#ifndef BB_HISTORY_H
#define BB_HISTORY_H

#include <string>
#include <vector>

std::string historyPath();

void loadHistory(std::vector<std::string>& out, size_t maxLines = 500);
void appendHistory(std::vector<std::string>& hist, const std::string& line, size_t maxLines = 500);
void clearHistory(std::vector<std::string>& hist);

// 🔥 global access (damit commands.cpp und main.cpp denselben vector benutzen)
void historyInit(size_t maxLines = 500);
std::vector<std::string>& historyVec();

#endif