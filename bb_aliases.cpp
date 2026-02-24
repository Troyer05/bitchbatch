#include "bb_aliases.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

std::string alias_file_path() {
    const char* xdg  = std::getenv("XDG_CONFIG_HOME");
    const char* home = std::getenv("HOME");

    std::string base;
    
    if (xdg && *xdg) base = xdg;
    else if (home && *home) base = std::string(home) + "/.config";
    else base = ".";

    return base + "/bitchbatch/aliases";
}

static inline std::string trim(std::string s) {
    auto notSpace = [](unsigned char c){ return !std::isspace(c); };

    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());

    return s;
}

static std::vector<std::string> split_ws(const std::string& s) {
    std::istringstream iss(s);
    std::vector<std::string> out;

    for (std::string tok; iss >> tok;) out.push_back(tok);

    return out;
}

void AliasStore::set(const std::string& name, const std::string& expansion) {
    map[name] = expansion;
}

bool AliasStore::del(const std::string& name) {
    return map.erase(name) > 0;
}

bool AliasStore::load(const std::string& path) {
    map.clear();

    std::ifstream in(path);

    if (!in.good()) return true;

    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);

        if (line.empty() || line[0] == '#') continue;

        auto pos = line.find('=');

        if (pos == std::string::npos) continue;

        std::string name = trim(line.substr(0, pos));
        std::string exp  = trim(line.substr(pos + 1));

        if (!name.empty() && !exp.empty()) map[name] = exp;
    }
    return true;
}

bool AliasStore::save(const std::string& path) const {
    std::filesystem::path p(path);
    std::filesystem::create_directories(p.parent_path());

    std::ofstream out(path, std::ios::trunc);

    if (!out.good()) return false;

    out << "# bitchbatch aliases\n";

    std::vector<std::string> keys;
    keys.reserve(map.size());

    for (const auto& kv : map) keys.push_back(kv.first);

    std::sort(keys.begin(), keys.end());

    for (const auto& k : keys) {
        out << k << "=" << map.at(k) << "\n";
    }

    return true;
}

std::vector<std::string> AliasStore::expand_first_token(const std::vector<std::string>& args) const {
    if (args.empty()) return args;

    if (!args[0].empty() && args[0][0] == '\\') {
        auto out = args;
        out[0] = out[0].substr(1);
        return out;
    }

    std::vector<std::string> out = args;
    std::string head = out[0];

    for (int depth = 0; depth < 10; ++depth) {
        auto it = map.find(head);

        if (it == map.end()) break;

        auto expanded = split_ws(it->second);

        if (expanded.empty()) break;

        std::vector<std::string> merged;

        merged.insert(merged.end(), expanded.begin(), expanded.end());
        merged.insert(merged.end(), out.begin() + 1, out.end());

        out.swap(merged);

        head = out[0];
    }

    return out;
}

static AliasStore G_ALIASES;
static std::string G_ALIAS_PATH = alias_file_path();
static bool G_LOADED = false;

AliasStore& getAliases() {
    if (!G_LOADED) {
        G_ALIASES.load(G_ALIAS_PATH);
        G_LOADED = true;
    }

    return G_ALIASES;
}

const std::string& getAliasPath() {
    return G_ALIAS_PATH;
}