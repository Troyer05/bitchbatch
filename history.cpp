#include "history.h"

#include <fstream>
#include <deque>
#include <cstdlib>
#include <cstdio>

static std::string trimRight(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    return s;
}

std::string historyPath() {
    const char* h = std::getenv("HOME");
    if (h && *h) return std::string(h) + "/.bitchbatch_history";
    return ".bitchbatch_history";
}

void loadHistory(std::vector<std::string>& out, size_t maxLines) {
    out.clear();

    std::ifstream f(historyPath());

    if (!f.is_open()) return;

    std::deque<std::string> q;
    std::string line;

    while (std::getline(f, line)) {
        line = trimRight(line);

        if (line.empty()) continue;

        q.push_back(line);

        if (q.size() > maxLines) q.pop_front();
    }

    out.assign(q.begin(), q.end());
}

static void appendLineToFile(const std::string& line) {
    std::ofstream f(historyPath(), std::ios::app);
    if (!f.is_open()) return;
    f << line << "\n";
}

void appendHistory(std::vector<std::string>& hist, const std::string& line, size_t maxLines) {
    std::string x = trimRight(line);

    if (x.empty()) return;
    if (!hist.empty() && hist.back() == x) return;

    hist.push_back(x);

    if (hist.size() > maxLines) hist.erase(hist.begin(), hist.begin() + (hist.size() - maxLines));

    appendLineToFile(x);
}

void clearHistory(std::vector<std::string>& hist) {
    hist.clear();
    std::ofstream f(historyPath(), std::ios::trunc);
}

/* ---------------- global shared history ---------------- */

static std::vector<std::string> g_hist;

void historyInit(size_t maxLines) {
    loadHistory(g_hist, maxLines);
}

std::vector<std::string>& historyVec() {
    return g_hist;
}