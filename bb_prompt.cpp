#include "bb_prompt.h"

#include "bb_env.h"
#include "bb_exec.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <cstdio>

namespace fs = std::filesystem;

static std::string homeDir() {
    const char* h = std::getenv("HOME");
    return h ? std::string(h) : std::string("/root");
}

static bool fileExists(const std::string& p) {
    std::error_code ec;
    return fs::exists(p, ec);
}

static std::string readAll(const std::string& p) {
    std::ifstream f(p);
    std::ostringstream ss;

    ss << f.rdbuf();

    return ss.str();
}

static inline std::string trimLocal(std::string s) {
    auto notSpace = [](unsigned char c){ return !std::isspace(c); };

    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());

    return s;
}

static std::string cfgGet(const std::string& cfg, const std::string& key, const std::string& def) {
    if (!fileExists(cfg)) return def;

    std::istringstream in(readAll(cfg));
    std::string line;

    while (std::getline(in, line)) {
        line = trimLocal(line);

        if (line.empty()) continue;
        if (line.rfind(key + "=", 0) == 0) return trimLocal(line.substr(key.size() + 1));
    }

    return def;
}

static std::string cfgMode(const std::string& cfg) {
    if (!fileExists(cfg)) return "basic";
    auto m = cfgGet(cfg, "MODE", "basic");
    if (m != "basic" && m != "fancy" && m != "ultra") return "basic";
    return m;
}

static std::vector<std::string> splitWords(const std::string& s) {
    std::istringstream iss(s);
    std::vector<std::string> v;

    for (std::string w; iss >> w; ) v.push_back(w);
    return v;
}

static std::string nowHHMM() {
    auto t = std::time(nullptr);
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[6];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", tm.tm_hour, tm.tm_min);
    return std::string(buf);
}

static std::string gitBranch() {
    std::string out;
    FILE* p = popen("git rev-parse --abbrev-ref HEAD 2>/dev/null", "r");
    if (!p) return "";
    char buf[256];
    if (fgets(buf, sizeof(buf), p)) out = buf;
    pclose(p);
    out = trimLocal(out);
    if (out == "HEAD") return "";
    return out;
}

static std::string themeColor(const std::string& theme, const std::string& role) {
    if (theme == "plain") {
        if (role == "user") return Color::GREEN;
        if (role == "host") return Color::BLUE;
        if (role == "path") return Color::CYAN;
        if (role == "git")  return Color::YELLOW;
        if (role == "ok")   return Color::GREEN;
        if (role == "bad")  return Color::RED;
        if (role == "time") return Color::MAGENTA;

        return Color::RESET;
    }

    if (role == "user") return Color::GREEN;
    if (role == "host") return Color::RED;
    if (role == "path") return Color::CYAN;
    if (role == "git")  return Color::YELLOW;
    if (role == "ok")   return Color::GREEN;
    if (role == "bad")  return Color::RED;
    if (role == "time") return Color::MAGENTA;

    return Color::RESET;
}

static std::string segBox(const std::string& c, const std::string& t) {
    return c + t + Color::RESET;
}

static inline std::string escFg(int c) { return "\033[38;5;" + std::to_string(c) + "m"; }
static inline std::string escBg(int c) { return "\033[48;5;" + std::to_string(c) + "m"; }
static inline std::string escBgDefault() { return "\033[49m"; }
static inline std::string escFgDefault() { return "\033[39m"; }

static std::string plSep(int leftBg, int rightBg) {
    return escFg(leftBg) + escBg(rightBg) + "";
}

static std::string plStart(int fg, int bg, const std::string& text) {
    return escFg(fg) + escBg(bg) + " " + text + " ";
}

static std::string plEnd(int lastBg) {
    return plSep(lastBg, 0) + escBgDefault() + escFgDefault() + Color::RESET;
}

std::string makeBibaPrompt(const std::string& user, const std::string& host, const std::string& cwd) {
    const std::string cfg = homeDir() + "/.biba-bash/config";

    const std::string mode = cfgMode(cfg);
    const std::string style = cfgGet(cfg, "STYLE", "multiline");
    const std::string symbol = cfgGet(cfg, "SYMBOL", ">");
    const std::string showHost = cfgGet(cfg, "SHOW_HOST", "1");

    auto modules = splitWords(cfgGet(cfg, "MODULES", "git status time"));

    if (mode == "basic") {
        std::string line1;
        line1 += segBox(Color::GREEN, user);

        if (showHost != "0") {
            line1 += " ";
            line1 += segBox(Color::BLUE, "@");
            line1 += " ";
            line1 += segBox(Color::RED, host);
        }

        line1 += " ";
        line1 += segBox(Color::CYAN, cwd);

        for (auto &m : modules) {
            if (m == "git") {
                auto br = gitBranch();

                if (!br.empty()) {
                    line1 += " ";
                    line1 += segBox(Color::ORANGE, "" + br);
                }

                continue;
            }

            if (m == "status") {
                int st = lastCmdStatus();
                line1 += " ";
                if (st == 0) line1 += segBox(Color::GREEN, "OK");
                else line1 += segBox(Color::RED, std::to_string(st));
                continue;
            }

            if (m == "time") {
                line1 += " ";
                line1 += segBox(Color::PINK, nowHHMM());
                continue;
            }
        }

        std::string line2 = Color::ORANGE + symbol + " " + Color::RESET;

        if (style == "single") return line1 + " " + line2;

        return line1 + "\n" + line2;
    }

    if (mode == "ultra") {
        int bgUser = 31;
        int bgPath = 25;
        int bgGit  = 208;
        int bgTime = 201;
        int bgOk   = 34;
        int bgBad  = 160;
        int bgSym  = 236;
        int fgDark = 16;
        int fgLight = 15;

        std::string tUser = user;

        if (showHost != "0") tUser += " @ " + host;

        std::string tPath = cwd;
        std::string br;

        int st = lastCmdStatus();

        std::string tStatus = (st == 0) ? "OK" : std::to_string(st);
        std::string tTime = nowHHMM();

        bool haveGit = false;

        for (auto &m : modules) {
            if (m == "git") {
                br = gitBranch();
                if (!br.empty()) haveGit = true;
            }
        }

        std::string line1;

        int curBg = -1;

        line1 += plStart(fgLight, bgUser, tUser);
        curBg = bgUser;
        line1 += plSep(curBg, bgPath);
        line1 += plStart(fgLight, bgPath, tPath);
        curBg = bgPath;

        for (auto &m : modules) {
            if (m == "git") {
                if (haveGit) {
                    line1 += plSep(curBg, bgGit);
                    line1 += plStart(fgDark, bgGit, "" + br);
                    curBg = bgGit;
                }

                continue;
            }

            if (m == "status") {
                int bgS = (st == 0) ? bgOk : bgBad;
                int fgS = (st == 0) ? fgDark : fgLight;

                line1 += plSep(curBg, bgS);
                line1 += plStart(fgS, bgS, tStatus);
                curBg = bgS;

                continue;
            }

            if (m == "time") {
                line1 += plSep(curBg, bgTime);
                line1 += plStart(fgLight, bgTime, tTime);
                curBg = bgTime;

                continue;
            }
        }

        line1 += plEnd(curBg);

        std::string line2;

        line2 += Color::GREEN + "└─" + Color::RESET;
        line2 += plStart(208, bgSym, symbol);
        line2 += plEnd(bgSym);
        line2 += " ";

        if (style == "single") return line1 + " " + line2;

        return line1 + "\n" + line2;
    }

    const std::string sep = cfgGet(cfg, "SEP", "│");
    const std::string theme = cfgGet(cfg, "THEME", "neon");

    std::string line1;

    line1 += Color::GREEN + "┌─" + Color::RESET;
    line1 += segBox(themeColor(theme, "user"), user);

    if (showHost != "0") {
        line1 += " ";
        line1 += segBox(Color::BLUE, "@");
        line1 += " ";
        line1 += segBox(Color::RED, host);
    }

    line1 += " ";
    line1 += segBox(themeColor(theme, "path"), cwd);

    for (auto &m : modules) {
        if (m == "git") {
            auto br = gitBranch();

            if (!br.empty()) {
                line1 += " ";
                line1 += segBox(Color::GRAY, sep);
                line1 += " ";
                line1 += segBox(Color::ORANGE, "" + br);
            }

            continue;
        }

        if (m == "status") {
            int st = lastCmdStatus();

            line1 += " ";
            line1 += segBox(Color::GRAY, sep);
            line1 += " ";

            if (st == 0) line1 += segBox(Color::GREEN, "OK");
            else line1 += segBox(Color::RED, std::to_string(st));

            continue;
        }

        if (m == "time") {
            line1 += " ";
            line1 += segBox(Color::GRAY, sep);
            line1 += " ";
            line1 += segBox(Color::PINK, nowHHMM());

            continue;
        }
    }

    std::string line2 = Color::GREEN + "└─" + Color::RESET + Color::ORANGE + symbol + " " + Color::RESET;

    if (style == "single") return line1 + " " + line2;
    
    return line1 + "\n" + line2;
}
