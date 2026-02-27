#include "framework.h"
#include "bb_commands.h"
#include "bb_exec.h"
#include "bb_search.h"
#include "bb_signal.h"
#include "bb_env.h"
#include "bb_help.h"
#include "bb_aliases.h"
#include "history.h"
#include "pkg.h"
#include "tutorial.h"

#include <unistd.h>
#include <termios.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <string>
#include <vector>
#include <csignal>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

static std::string homeDir() {
    const char* h = std::getenv("HOME");
    return h ? std::string(h) : std::string("/root");
}

static bool fileExists(const std::string& p) {
    std::error_code ec;
    return fs::exists(p, ec);
}

static void backupFile(const std::string& p) {
    if (!fileExists(p)) return;

    std::string b = p + ".biba.bak";
    std::error_code ec;

    fs::copy_file(p, b, fs::copy_options::overwrite_existing, ec);
}

static std::string readAll(const std::string& p) {
    std::ifstream f(p);
    std::ostringstream ss;

    ss << f.rdbuf();

    return ss.str();
}

static void writeAll(const std::string& p, const std::string& s) {
    std::ofstream f(p, std::ios::trunc);
    f << s;
}

static void ensureLineInFile(const std::string& p, const std::string& line) {
    std::string c = fileExists(p) ? readAll(p) : "";

    if (c.find(line) != std::string::npos) return;
    if (!c.empty() && c.back() != '\n') c.push_back('\n');

    c += line + "\n";

    writeAll(p, c);
}

static void setZshKey(const std::string& p, const std::string& key, const std::string& valueQuoted) {
    std::string c = fileExists(p) ? readAll(p) : "";
    std::istringstream in(c);
    std::ostringstream out;
    std::string line;

    bool done = false;

    while (std::getline(in, line)) {
        if (line.rfind(key + "=", 0) == 0) {
            out << key << "=" << valueQuoted << "\n";
            done = true;
        } else {
            out << line << "\n";
        }
    }

    if (!done) out << key << "=" << valueQuoted << "\n";

    writeAll(p, out.str());
}

static std::vector<std::string> splitWords(const std::string& s) {
    std::istringstream iss(s);
    std::vector<std::string> v;

    for (std::string w; iss >> w; ) v.push_back(w);

    return v;
}

static std::string joinWords(const std::vector<std::string>& v) {
    std::ostringstream oss;

    for (size_t i=0;i<v.size();++i) {
        if (i) oss << " ";
        oss << v[i];
    }

    return oss.str();
}

static std::vector<std::string> parsePluginsLine(const std::string& line) {
    auto l = line.find('(');
    auto r = line.find(')', l == std::string::npos ? 0 : l);

    if (l == std::string::npos || r == std::string::npos || r <= l) return {};

    return splitWords(line.substr(l+1, r-l-1));
}

static void setPlugins(const std::string& p, const std::vector<std::string>& plugins, bool merge) {
    std::string c = fileExists(p) ? readAll(p) : "";
    std::istringstream in(c);
    std::ostringstream out;
    std::string line;

    bool done = false;

    std::vector<std::string> finalPlugins = plugins;

    while (std::getline(in, line)) {
        if (line.rfind("plugins=(", 0) == 0) {
            if (merge) {
                auto cur = parsePluginsLine(line);

                for (auto &pl : plugins) {
                    bool exists = false;
                    for (auto &x : cur) if (x == pl) { exists = true; break; }
                    if (!exists) cur.push_back(pl);
                }

                finalPlugins = cur;
            }

            out << "plugins=(" << joinWords(finalPlugins) << ")\n";
            done = true;
        } else {
            out << line << "\n";
        }
    }

    if (!done) out << "plugins=(" << joinWords(finalPlugins) << ")\n";

    writeAll(p, out.str());
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
        auto t = trimLocal(line);

        if (t.empty()) continue;
        if (t.rfind(key + "=", 0) == 0) return trimLocal(t.substr(key.size() + 1));
    }

    return def;
}


static void removeLinesContaining(const std::string& p, const std::string& needle) {
    if (!fileExists(p)) return;

    std::string c = readAll(p);
    std::istringstream in(c);
    std::ostringstream out;
    std::string line;

    bool changed = false;

    while (std::getline(in, line)) {
        if (line.find(needle) != std::string::npos) {
            changed = true;
            continue;
        }

        out << line << "\n";
    }

    if (changed) writeAll(p, out.str());
}

static void ensureDir(const std::string& p) {
    std::error_code ec;
    fs::create_directories(p, ec);
}

static void writeIfDifferent(const std::string& p, const std::string& s) {
    if (fileExists(p)) {
        if (readAll(p) == s) return;
    }

    writeAll(p, s);
}

static void setCfgKey(const std::string& p, const std::string& key, const std::string& value) {
    std::string c = fileExists(p) ? readAll(p) : "";
    std::istringstream in(c);
    std::ostringstream out;
    std::string line;

    bool done = false;

    while (std::getline(in, line)) {
        auto t = trimLocal(line);

        if (t.rfind(key + "=", 0) == 0) {
            out << key << "=" << value << "\n";
            done = true;
        } else {
            if (!t.empty()) out << t << "\n";
        }
    }

    if (!done) out << key << "=" << value << "\n";

    writeAll(p, out.str());
}

static void setCfgModules(const std::string& p, const std::vector<std::string>& modules, bool merge) {
    std::string c = fileExists(p) ? readAll(p) : "";
    std::string cur;
    std::istringstream in(c);
    std::ostringstream out;
    std::string line;

    bool have = false;

    while (std::getline(in, line)) {
        auto t = trimLocal(line);

        if (t.rfind("MODULES=", 0) == 0) {
            cur = t.substr(std::string("MODULES=").size());
            have = true;
            continue;
        }

        if (t.rfind("THEME=", 0) == 0) out << t << "\n";
    }

    std::vector<std::string> finalMods = modules;

    if (merge && have) {
        auto curMods = splitWords(cur);

        for (auto &m : modules) {
            bool ex = false;
            for (auto &x : curMods) if (x == m) { ex = true; break; }
            if (!ex) curMods.push_back(m);
        }

        finalMods = curMods;
    }

    out << "MODULES=" << joinWords(finalMods) << "\n";

    if (!fileExists(p) || readAll(p).find("THEME=") == std::string::npos) {
        if (out.str().find("THEME=") == std::string::npos) {
        }
    }

    std::string outStr = out.str();

    if (outStr.find("THEME=") == std::string::npos) outStr = std::string("THEME=neon\n") + outStr;

    writeAll(p, outStr);
}

static const char* BIBA_BASH_MAIN = R"BB(if [ -n "${__BIBA_BASH_LOADED:-}" ]; then return; fi
__BIBA_BASH_LOADED=1

__biba_home="${HOME:-/root}"
__biba_dir="$__biba_home/.biba-bash"
__biba_cfg="$__biba_dir/config"
__biba_theme=""
__biba_modules=""
__biba_style=""
__biba_symbol=""
__biba_show_host=""
__biba_mode=""
__biba_sep=""

__BIBA_RESET="\[\e[0m\]"
__BIBA_G="\[\e[32m\]"
__BIBA_B="\[\e[34m\]"
__BIBA_R="\[\e[31m\]"
__BIBA_C="\[\e[36m\]"
__BIBA_O="\[\e[38;5;208m\]"
__BIBA_P="\[\e[38;5;205m\]"
__BIBA_DIM="\[\e[38;5;244m\]"

__pl_fg() { printf "\\[\\e[38;5;%sm\\]" "$1"; }
__pl_bg() { printf "\\[\\e[48;5;%sm\\]" "$1"; }
__pl_bg_def="\[\e[49m\]"
__pl_fg_def="\[\e[39m\]"

__biba_trim() { local s="$1"; s="${s#"${s%%[![:space:]]*}"}"; s="${s%"${s##*[![:space:]]}"}"; printf "%s" "$s"; }

__biba_load_config() {
  __biba_theme="neon"
  __biba_modules="git status time"
  __biba_style="multiline"
  __biba_symbol=">"
  __biba_show_host="1"
  __biba_mode="basic"
  __biba_sep="│"
  if [ -f "$__biba_cfg" ]; then
    while IFS= read -r line || [ -n "$line" ]; do
      line="$(__biba_trim "$line")"
      [ -z "$line" ] && continue
      case "$line" in
        MODE=*) __biba_mode="${line#MODE=}" ;;
        THEME=*) __biba_theme="${line#THEME=}" ;;
        MODULES=*) __biba_modules="${line#MODULES=}" ;;
        STYLE=*) __biba_style="${line#STYLE=}" ;;
        SYMBOL=*) __biba_symbol="${line#SYMBOL=}" ;;
        SHOW_HOST=*) __biba_show_host="${line#SHOW_HOST=}" ;;
        SEP=*) __biba_sep="${line#SEP=}" ;;
      esac
    done < "$__biba_cfg"
  fi
}

__biba_git_info() {
  command -v git >/dev/null 2>&1 || return 0
  git rev-parse --is-inside-work-tree >/dev/null 2>&1 || return 0
  local b d
  b="$(git symbolic-ref --quiet --short HEAD 2>/dev/null || git rev-parse --short HEAD 2>/dev/null)"
  [ -z "$b" ] && return 0
  git diff --quiet --ignore-submodules -- 2>/dev/null; d=$?
  if [ "$d" -ne 0 ]; then
    printf "%s*" "$b"
  else
    printf "%s" "$b"
  fi
}

__biba_mod_git() { __biba_git_info; }
__biba_mod_status() { :; }
__biba_mod_time() { :; }

__biba_load_theme() { :; }

__biba_join_modules() {
  local out=""
  local m
  for m in $__biba_modules; do
    case "$m" in
      git) out="$out$(__biba_mod_git)" ;;
      status) out="$out$(__biba_mod_status)" ;;
      time) out="$out$(__biba_mod_time)" ;;
    esac
  done
  printf "%s" "$out"
}

__biba_prompt_cmd() {
  local st="$?"
  local u="\u"
  local h="\h"
  local p="\w"

  local git=""
  local m
  for m in $__biba_modules; do
    case "$m" in
      git) git="$(__biba_mod_git)" ;;
    esac
  done

  local status=""
  local time=""
  for m in $__biba_modules; do
    case "$m" in
      status) if [ "$st" -eq 0 ]; then status="${__BIBA_G}OK${__BIBA_RESET}"; else status="${__BIBA_R}${st}${__BIBA_RESET}"; fi ;;
      time) time="${__BIBA_P}$(date +%H:%M)${__BIBA_RESET}" ;;
    esac
  done

  if [ "${__biba_mode}" = "fancy" ]; then
    local l1="${__BIBA_G}┌─${__BIBA_RESET}${__BIBA_G}${u}${__BIBA_RESET}"
    if [ "${__biba_show_host}" != "0" ]; then
      l1="${l1} ${__BIBA_B}@${__BIBA_RESET} ${__BIBA_R}${h}${__BIBA_RESET}"
    fi
    l1="${l1} ${__BIBA_C}${p}${__BIBA_RESET}"
    [ -n "$git" ] && l1="${l1} ${__BIBA_DIM}${__biba_sep}${__BIBA_RESET} ${__BIBA_O}${git}${__BIBA_RESET}"
    [ -n "$status" ] && l1="${l1} ${__BIBA_DIM}${__biba_sep}${__BIBA_RESET} ${status}"
    [ -n "$time" ] && l1="${l1} ${__BIBA_DIM}${__biba_sep}${__BIBA_RESET} ${time}"
    local l2="${__BIBA_G}└─${__BIBA_RESET}${__BIBA_O}${__biba_symbol}${__BIBA_RESET} "
    if [ "${__biba_style}" = "single" ]; then
      PS1="${l1} ${l2}"
    else
      PS1="${l1}\n${l2}"
    fi
    return
  fi

  if [ "${__biba_mode}" = "ultra" ]; then
    local bg_user=31
    local bg_path=25
    local bg_git=208
    local bg_ok=34
    local bg_bad=160
    local bg_time=201
    local bg_sym=236

    local tuser="$u"
    if [ "${__biba_show_host}" != "0" ]; then
      tuser="$u @ $h"
    fi

    local l1
    l1="$(__pl_fg 15)$(__pl_bg $bg_user) $tuser "
    l1+="$(__pl_fg $bg_user)$(__pl_bg $bg_path)$(__pl_fg 15)$(__pl_bg $bg_path) $p "

    local curbg=$bg_path
    if [ -n "$git" ]; then
      l1+="$(__pl_fg $curbg)$(__pl_bg $bg_git)$(__pl_fg 16)$(__pl_bg $bg_git) $git "
      curbg=$bg_git
    fi

    local stxt
    stxt="$st"
    [ "$st" -eq 0 ] && stxt="OK"
    local bgs
    local fgs
    if [ "$st" -eq 0 ]; then bgs=$bg_ok; fgs=16; else bgs=$bg_bad; fgs=15; fi
    l1+="$(__pl_fg $curbg)$(__pl_bg $bgs)$(__pl_fg $fgs)$(__pl_bg $bgs) $stxt "
    curbg=$bgs

    if [ -n "$time" ]; then
      l1+="$(__pl_fg $curbg)$(__pl_bg $bg_time)$(__pl_fg 15)$(__pl_bg $bg_time) $(date +%H:%M) "
      curbg=$bg_time
    fi

    l1+="$(__pl_fg $curbg)$__pl_bg_def$__BIBA_RESET"

    local l2="${__BIBA_G}└─${__BIBA_RESET}$(__pl_fg 208)$(__pl_bg $bg_sym) ${__biba_symbol} $(__pl_fg $bg_sym)$__pl_bg_def$__BIBA_RESET "
    if [ "${__biba_style}" = "single" ]; then
      PS1="${l1} ${l2}"
    else
      PS1="${l1}\n${l2}"
    fi
    return
  fi

  local l1="${__BIBA_G}${u}${__BIBA_RESET}"
  if [ "${__biba_show_host}" != "0" ]; then
    l1="${l1} ${__BIBA_B}@${__BIBA_RESET} ${__BIBA_R}${h}${__BIBA_RESET}"
  fi
  l1="${l1} ${__BIBA_C}${p}${__BIBA_RESET}"
  [ -n "$git" ] && l1="${l1} ${__BIBA_O}${git}${__BIBA_RESET}"
  [ -n "$status" ] && l1="${l1} ${status}"
  [ -n "$time" ] && l1="${l1} ${time}"
  local l2="${__BIBA_O}${__biba_symbol}${__BIBA_RESET} "
  if [ "${__biba_style}" = "single" ]; then
    PS1="${l1} ${l2}"
  else
    PS1="${l1}\n${l2}"
  fi
}

__biba_apply() {
  __biba_load_config
  __biba_load_theme
  PROMPT_COMMAND="__biba_prompt_cmd"
}

__biba_apply
)BB";
static const char* BIBA_BASH_THEME_NEON = R"BB(__BIBA_RESET="\[\e[0m\]"
__BIBA_C0="\[\e[38;5;46m\]"
__BIBA_C1="\[\e[38;5;39m\]"
__BIBA_C2="\[\e[38;5;201m\]"
__BIBA_C3="\[\e[38;5;226m\]"
__BIBA_DIM="\[\e[38;5;244m\]"

__BIBA_LINE1_PRE="${__BIBA_C0}┌─${__BIBA_C1}${__BIBA_DIM}[${__BIBA_RESET}${__BIBA_C1}"
__BIBA_LINE1_MID="${__BIBA_DIM}]${__BIBA_RESET} ${__BIBA_C2}"
__BIBA_LINE1_POST="${__BIBA_RESET}"
__BIBA_LINE2_PRE="${__BIBA_C0}└─${__BIBA_C3}"
__BIBA_LINE2_POST=" ${__BIBA_RESET}"

__BIBA_GIT_PRE=" ${__BIBA_DIM}(${__BIBA_C0}git:${__BIBA_RESET}${__BIBA_C0}"
__BIBA_GIT_POST="${__BIBA_DIM})${__BIBA_RESET}"
__BIBA_STATUS_PRE=" ${__BIBA_DIM}(${__BIBA_C2}x:${__BIBA_RESET}${__BIBA_C2}"
__BIBA_STATUS_POST="${__BIBA_DIM})${__BIBA_RESET}"
__BIBA_TIME_PRE=" ${__BIBA_DIM}(${__BIBA_C3}"
__BIBA_TIME_POST="${__BIBA_DIM})${__BIBA_RESET}"
)BB";
static const char* BIBA_BASH_THEME_PLAIN = R"BB(__BIBA_RESET="\[\e[0m\]"
__BIBA_C0="\[\e[0m\]"
__BIBA_C1="\[\e[0m\]"
__BIBA_C2="\[\e[0m\]"
__BIBA_C3="\[\e[0m\]"
__BIBA_DIM="\[\e[0m\]"

__BIBA_LINE1_PRE="["
__BIBA_LINE1_MID=" "
__BIBA_LINE1_POST="]"
__BIBA_LINE2_PRE=""
__BIBA_LINE2_POST=" "

__BIBA_GIT_PRE=" (git:"
__BIBA_GIT_POST=")"
__BIBA_STATUS_PRE=" (x:"
__BIBA_STATUS_POST=")"
__BIBA_TIME_PRE=" ("
__BIBA_TIME_POST=")"
)BB";

static bool hasCommand(const char* cmd) {
    const char* path = getenv("PATH");

    if (!path) return false;

    std::string p(path);

    size_t start = 0;

    while (start <= p.size()) {
        size_t end = p.find(':', start);

        if (end == std::string::npos) end = p.size();

        std::string dir = p.substr(start, end - start);

        if (dir.empty()) dir = ".";

        std::string full = dir + "/" + cmd;

        if (access(full.c_str(), X_OK) == 0) return true;

        start = end + 1;
    }

    return false;
}

std::string findCommand(const std::string& cmd) {
    const char* path = getenv("PATH");

    if (path) {
        std::string p(path);

        size_t start = 0;

        while (start <= p.size()) {
            size_t end = p.find(':', start);

            if (end == std::string::npos) end = p.size();

            std::string dir = p.substr(start, end - start);

            if (dir.empty()) dir = ".";

            std::string full = dir + "/" + cmd;

            if (access(full.c_str(), X_OK) == 0) return full;

            start = end + 1;
        }
    }

    std::vector<std::string> fallback = {
        "/usr/games",
        "/usr/local/bin",
        "/usr/bin"
    };

    for (auto& dir : fallback) {
        std::string full = dir + "/" + cmd;

        if (access(full.c_str(), X_OK) == 0) return full;
    }

    return "";
}

void registerCommands(CommandMap& commands) {
    std::signal(SIGINT, onSigInt);

    static const PkgMgr PM = detectPkgMgr();

    commands["update-biba"] = [&](const std::vector<std::string>&) {
        cout << "Updating Biba...\n\n";

        cmd("rm -rf bitchbatch 2>/dev/null");
        cmd("rm /sbin/biba 2>/dev/null");
        cmd("rm /usr/local/bin/biba 2>/dev/null");

        cmd("git clone https://github.com/Troyer05/bitchbatch.git");

        if (system("test -d bitchbatch") != 0) {
            cout << "Clone failed.\n";
            return;
        }

        cmd("chmod +x bitchbatch/install.sh");
        cmd("g++ bitchbatch/*.cpp -Iheader -o bitchbatch/biba");
        cmd("sudo install -m 755 bitchbatch/biba /usr/local/bin/biba");
        cmd("sudo install -m 755 bitchbatch/biba /sbin/biba");

        cout << "\nUpdate finished. Restarting...\n\n";

        execl("/usr/local/bin/biba", "biba", nullptr);

        perror("exec failed");
        
        exit(1);
    };

    commands["c"] = commands["cls"] = commands["clear"] = [&](const std::vector<std::string>&) {
        CLS();
    };

    commands["exut"] = commands["exit"] = [&](const std::vector<std::string>&) {
        exit(0);
    };

    commands["dir"] = commands["ls"] = commands["l"] = [&](const std::vector<std::string>&) {
        cmd("ls -alh --color");
        cout << "\n";
    };

    commands["rebuild"] = [&](const std::vector<std::string>&) {
        cmd("sudo bash build.sh");
        exit(0);
    };

    commands["grub"] = [&](const std::vector<std::string>&) {
        cmd("sudo nano /etc/default/grub");
        cmd("sudo update-grub");

        cout << "\n";
    };

    commands["u"] = commands["update"] = [&](const std::vector<std::string>&) {
        pkgUpdate(PM);
        pkgUpgrade(PM);
        pkgAutoremove(PM);

        cout << "\n";
    };

    commands["ports"] = [&](const std::vector<std::string>&) {
        cmd("ss -tulpn");
        cout << "\n";
    };

    commands["mem"] = [&](const std::vector<std::string>&) {
        cmd("free -h");
        cout << "\n";
    };

    commands["disk"] = [&](const std::vector<std::string>&) {
        cmd("df -h");
        cout << "\n";
    };

    commands["learn-linux-terminal"] = [&](const std::vector<std::string>&) {
        learnLinuxTerminal();
    };

    commands["info"] = commands["credits"] = [&](const std::vector<std::string>&) {
        cout << "\n\n";
        cout << "Version: " << getVersion() << "\n";
        cout << "Entwickler: " << getProgrammers() << "\n";
        cout << "\nThanks for using BitchBatch!";
        cout << "\n\n";
    };

    commands["ch"] = [&](const std::vector<std::string>&) {
        clearHistory(historyVec());

        cout << "History cleared.\n\n";
    };

    commands["off"] = [&](const std::vector<std::string>&) {
        cmd("poweroff");

        cout << "\n";
        
        exit(0);
    };

    commands["init"] = [&](const std::vector<std::string>&) {
        pkgUpdate(PM);

        std::vector<std::string> pkgs = {
            "curl","wget","git","htop","btop","iptraf","atop","iotop","glances","iperf",
            "gcc","lolcat","nano","vim","ranger","mc","figlet","cmatrix","hollywood","figlet"
        };

        pkgInstall(PM, pkgs);
        pkgUpgrade(PM);
        pkgAutoremove(PM);

        cout << "\n";
    };

    commands["hist"] = commands["history"] = [&](const std::vector<std::string>& args) {
        auto& hist = historyVec();

        if (args.size() == 2 && args[1] == "-c") {
            clearHistory(hist);
            
            cout << "History cleared.\n\n";

            return;
        }

        if (hist.empty()) {
            cout << "History is empty.\n\n";
            return;
        }

        size_t start = 0;

        if (args.size() == 2) {
            try {
                size_t n = std::stoul(args[1]);
            
                if (n < hist.size())
                    start = hist.size() - n;
            } catch (...) {
            }
        }

        for (size_t i = start; i < hist.size(); ++i) {
            cout << i + 1 << "  " << hist[i] << "\n";
        }

        cout << "\n";
    };

    commands["mk"] = [&](const std::vector<std::string>& args) {
        if (args.size() < 2) {
            cout << "usage: mk <dir-name>\n\n";
            return;
        }

        std::vector<std::string> p = {"mkdir", "-p"};
        for (size_t i = 1; i < args.size(); i++) p.push_back(args[i]);

        cmdArgs(p);

        const char* target = args[1].c_str();
        if (chdir(target) != 0) {
            cerr << "cd: " << strerror(errno) << "\n";
        }

        cout << "\n";
    };

    commands["rm"] = [&](const std::vector<std::string>& args) {
        if (args.size() < 2) {
            cout << "usage: rm <path>\n\n";
            return;
        }

        std::vector<std::string> p = {"rm", "-rf"};
        for (size_t i = 1; i < args.size(); i++) p.push_back(args[i]);

        cmdArgs(p);
        cout << "\n";
    };

    commands["en"] = [&](const std::vector<std::string>& args) {
        if (args.size() < 2) {
            cout << "usage: en <systemctl-service-name>\n\n";
            return;
        }

        std::vector<std::string> p = {"sudo", "systemctl", "enable"};
        for (size_t i = 1; i < args.size(); i++) p.push_back(args[i]);

        cmdArgs(p);
        cout << "\n";
    };

    commands["dis"] = [&](const std::vector<std::string>& args) {
        if (args.size() < 2) {
            cout << "usage: dis <systemctl-service-name>\n\n";
            return;
        }

        std::vector<std::string> p = {"sudo", "systemctl", "disable"};
        for (size_t i = 1; i < args.size(); i++) p.push_back(args[i]);

        cmdArgs(p);
        cout << "\n";
    };

    commands["status"] = [&](const std::vector<std::string>& args) {
        if (args.size() < 2) {
            cout << "usage: status <systemctl-service-name>\n\n";
            return;
        }

        std::vector<std::string> p = {"sudo", "systemctl", "status"};
        for (size_t i = 1; i < args.size(); i++) p.push_back(args[i]);

        cmdArgs(p);
        cout << "\n";
    };

    commands["restart"] = [&](const std::vector<std::string>& args) {
        if (args.size() < 2) {
            cout << "usage: restart <systemctl-service-name>\n\n";
            return;
        }

        std::vector<std::string> p = {"sudo", "systemctl", "restart"};
        for (size_t i = 1; i < args.size(); i++) p.push_back(args[i]);

        cmdArgs(p);
        cout << "\n";
    };

    commands["stop"] = [&](const std::vector<std::string>& args) {
        if (args.size() < 2) {
            cout << "usage: stop <systemctl-service-name>\n\n";
            return;
        }

        std::vector<std::string> p = {"sudo", "systemctl", "stop"};
        for (size_t i = 1; i < args.size(); i++) p.push_back(args[i]);

        cmdArgs(p);
        cout << "\n";
    };

    commands["start"] = [&](const std::vector<std::string>& args) {
        if (args.size() < 2) {
            cout << "usage: start <systemctl-service-name>\n\n";
            return;
        }

        std::vector<std::string> p = {"sudo", "systemctl", "start"};
        for (size_t i = 1; i < args.size(); i++) p.push_back(args[i]);

        cmdArgs(p);
        cout << "\n";
    };

    commands["ip"] = [&](const std::vector<std::string>& args) {
        if (args.size() < 2) {
            cmd("ip a");
            return;
        }

        std::vector<std::string> p = {"ip"};
        for (size_t i = 1; i < args.size(); i++) p.push_back(args[i]);

        cmdArgs(p);
        cout << "\n";
    };

    commands["deploy"] = [&](const std::vector<std::string>& args) {
        if (args.size() < 2) {
            cout << "Forgot commit message!\n\n";
            return;
        }

        std::string msg;

        for (size_t i = 1; i < args.size(); i++) {
            if (i > 1) msg += " ";
            msg += args[i];
        }

        std::string safeMsg;

        for (char c : msg) {
            if (c == '"') safeMsg += "\\\"";
            else safeMsg += c;
        }

        std::vector<std::string> add = {"git","add","."};
        std::vector<std::string> commit = {"git","commit","-m", msg};
        std::vector<std::string> push = {"git","push","origin","main"};

        cmdArgs(add);
        cmdArgs(commit);
        cmdArgs(push);

        cout << "\n";
    };

    commands["i"] = commands["install"] = [&](const std::vector<std::string>& args) {
        if (args.size() < 2) { cout << "usage: install <package...>\n\n"; return; }

        std::vector<std::string> pkgs(args.begin() + 1, args.end());

        pkgInstall(PM, pkgs);
        
        cout << "\n";
    };

    commands["b"] = commands["bash"] = [&](const std::vector<std::string>& args) {
        if (args.size() < 2) {
            cmd("sudo nano bitchtmp.sh");
            cmd("sudo chmod +x bitchtmp.sh");
            cmd("sudo bash bitchtmp.sh");
            cmd("sudo rm bitchtmp.sh");
            
            cout << "\n";

            return;
        }

        std::vector<std::string> p = {"sudo", "bash"};
        
        for (size_t i = 1; i < args.size(); i++) {
            p.push_back(args[i]);
        }

        cmdArgs(p);
        
        cout << "\n";
    };

    commands["e"] = [&](const std::vector<std::string>& args) {
        if (args.size() < 2) {
            cout << "usage: e <file>\n\n";
            return;
        }

        std::vector<std::string> p = {"sudo", "nano"};
        for (size_t i = 1; i < args.size(); i++) p.push_back(args[i]);

        cmdArgs(p);
        cout << "\n";
    };

    commands["ui"] = commands["uninstall"] = [&](const std::vector<std::string>& args) {
        if (args.size() < 2) {
            cout << "usage: uninstall <package...>\n\n";
            return;
        }

        std::vector<std::string> pkgs(args.begin() + 1, args.end());
        
        pkgUninstall(PM, pkgs);
        
        cout << "\n";
    };

    commands["cd"] = [&](const std::vector<std::string>& args) {
        const char* target = nullptr;

        if (args.size() >= 2) {
            target = args[1].c_str();
        } else {
            target = getenv("HOME");
        }

        if (!target || !*target) {
            cerr << "cd: HOME not set\n";
            return;
        }

        if (chdir(target) != 0) {
            cerr << "cd: " << strerror(errno) << "\n";
        }
    };

    commands["find"] = commands["search"] = [&](const std::vector<std::string>& args) {
        if (args.size() < 2) {
            cout << "usage: search <pattern> [--in-files|-if]\n\n";
            return;
        }

        auto isFlag = [&](const std::string& s) {
            return s == "--in-files" || s == "--fin-files" || s == "-if";
        };

        bool inFiles = false;

        std::string pattern;

        size_t i = 1;

        for (; i < args.size(); i++) {
            if (isFlag(args[i])) break;
            if (!pattern.empty()) pattern += " ";

            pattern += args[i];
        }

        for (; i < args.size(); i++) {
            if (isFlag(args[i])) inFiles = true;
        }

        if (pattern.size() >= 2) {
            char q = pattern.front();

            if ((q == '"' || q == '\'') && pattern.back() == q) {
                pattern = pattern.substr(1, pattern.size() - 2);
            }
        }

        if (pattern.empty()) {
            cout << "usage: search <pattern> [--in-files|-if|-f]\n\n";
            return;
        }

        auto cwd = std::filesystem::current_path().string();
        
        if (cwd == "/") {
            cout << "Hint: Searching / will skip /proc,/sys,/dev,/run to avoid crashes.\n\n";
        }

        searchRecursive(cwd, pattern, inFiles);
        cout << "\n";
    };

    commands["cl"] = [&](const std::vector<std::string>& args) {
        if (args.size() < 2) {
            cmd("ranger .");
            return;
        }

        std::vector<std::string> p = {"ranger"};
        for (size_t i = 1; i < args.size(); i++) p.push_back(args[i]);

        cmdArgs(p);

        cout << "\n";
    };

    commands["me"] = [&](const std::vector<std::string>& args) {
        if (args.size() < 2) {
            cmd("mcedit .");
            return;
        }

        std::vector<std::string> p = {"mcedit"};
        for (size_t i = 1; i < args.size(); i++) p.push_back(args[i]);

        cmdArgs(p);

        cout << "\n";
    };

    commands["v"] = [&](const std::vector<std::string>& args) {
        if (args.size() < 2) {
            cmd("vim");
            return;
        }

        std::vector<std::string> p = {"vim"};
        for (size_t i = 1; i < args.size(); i++) p.push_back(args[i]);

        cmdArgs(p);

        cout << "\n";
    };

    commands["vsc"] = [&](const std::vector<std::string>& args) {
        if (args.size() < 2) {
            cmd("code . --no-sandbox --user-data-dir /root");
            return;
        }

        std::vector<std::string> p = {"code"};
        for (size_t i = 1; i < args.size(); i++) p.push_back(args[i]);
        
        p.push_back("--no-sandbox");
        p.push_back("--user-data-dir");
        p.push_back("/root");

        cmdArgs(p);

        cout << "\n";
    };

    commands["h"] = commands["help"] = [&](const std::vector<std::string>& args) {
        if (args.size() < 2) {
            basicHelp(PM);
            return;
        }

        helpFor(args[1], PM);

        cout << "\n";
    };

    commands["alias"] = [&](const std::vector<std::string>& args) {
        if (args.size() == 1) {
            if (getAliases().map.empty()) {
                cout << "No aliases set.\n\n";
                return;
            }

            std::vector<std::string> keys;

            keys.reserve(getAliases().map.size());

            for (auto const& kv : getAliases().map) keys.push_back(kv.first);

            std::sort(keys.begin(), keys.end());

            for (auto const& k : keys) {
                cout << k << "='" << getAliases().map[k] << "'\n";
            }

            cout << "\n";

            return;
        }

        std::string joined;

        for (size_t i = 1; i < args.size(); ++i) {
            if (i > 1) joined += " ";
            joined += args[i];
        }

        auto eq = joined.find('=');

        if (eq != std::string::npos) {
            std::string name = trimLocal(joined.substr(0, eq));
            std::string exp  = trimLocal(joined.substr(eq + 1));

            if (name.empty() || exp.empty()) {
                cout << "usage: alias name=command...\n\n";
                return;
            }

            getAliases().set(name, exp);

            if (!getAliases().save(getAliasPath())) {
                cout << "Failed to save aliases.\n\n";
                return;
            }

            cout << "alias " << name << "='" << exp << "'\n\n";

            return;
        }

        if (args.size() >= 3) {
            std::string name = args[1];
            std::string exp;

            for (size_t i = 2; i < args.size(); ++i) {
                if (i > 2) exp += " ";
                exp += args[i];
            }

            getAliases().set(name, exp);

            if (!getAliases().save(getAliasPath())) {
                cout << "Failed to save aliases.\n\n";
                return;
            }

            cout << "alias " << name << "='" << exp << "'\n\n";

            return;
        }

        cout << "usage:\n";
        cout << "  alias\n";
        cout << "  alias name=command...\n";
        cout << "  alias name command...\n\n";
    };

    commands["unalias"] = [&](const std::vector<std::string>& args) {
        if (args.size() != 2) {
            cout << "usage: unalias <name>\n\n";
            return;
        }

        if (!getAliases().del(args[1])) {
            cout << "alias not found: " << args[1] << "\n\n";
            return;
        }

        if (!getAliases().save(getAliasPath())) {
            cout << "Failed to save aliases.\n\n";
            return;
        }

        cout << "unalias " << args[1] << "\n\n";
    };


commands["bashrc"] = [&](const std::vector<std::string>& args) {
    const std::string bashrc = homeDir() + "/.bashrc";
    const std::string dir = homeDir() + "/.biba-bash";
    const std::string cfg = dir + "/config";
    const std::string themes = dir + "/themes";
    const std::string mainFile = dir + "/biba.sh";

    auto usage = [&](){
        std::cout <<
            "Usage:\n"
            "  bashrc install\n"
            "  bashrc wizard\n"
            "  bashrc theme <neon|plain>\n"
            "  bashrc module <name>           (setzt GENAU dieses eine)\n"
            "  bashrc modules <m1> <m2> ...   (setzt Liste)\n"
            "  bashrc module-add <name>       (merged in bestehende)\n"
            "  bashrc off\n"
            "  bashrc reload\n";
    };

    if (args.size() < 2) { usage(); return; }
    const std::string sub = args[1];

    if (sub == "install") {
        backupFile(bashrc);

        ensureDir(dir);
        ensureDir(themes);

        writeIfDifferent(mainFile, std::string(BIBA_BASH_MAIN) + "\n");

        writeIfDifferent(themes + "/neon.sh", std::string(BIBA_BASH_THEME_NEON) + "\n");
        writeIfDifferent(themes + "/plain.sh", std::string(BIBA_BASH_THEME_PLAIN) + "\n");

        if (!fileExists(cfg)) {
            writeAll(cfg, "MODE=basic\nTHEME=neon\nSTYLE=multiline\nSYMBOL=>\nSHOW_HOST=1\nSEP=│\nMODULES=git status time\n");
        }

        ensureLineInFile(bashrc, "source \"$HOME/.biba-bash/biba.sh\"");

        std::cout << "Bash customization installed.\n";

        return;
    }

    if (sub == "wizard") {
    ensureDir(dir);
    ensureDir(themes);

    if (!fileExists(cfg)) {
        writeAll(cfg, "MODE=basic\nTHEME=neon\nSTYLE=multiline\nSYMBOL=>\nSHOW_HOST=1\nSEP=│\nMODULES=git status time\n");
    }

    std::string mode = cfgGet(cfg, "MODE", "basic");
    std::string curTheme = cfgGet(cfg, "THEME", "neon");
    std::string curStyle = cfgGet(cfg, "STYLE", "multiline");
    std::string curSymbol = cfgGet(cfg, "SYMBOL", ">");
    std::string curShowHost = cfgGet(cfg, "SHOW_HOST", "1");
    std::string curSep = cfgGet(cfg, "SEP", "│");
    std::string curModules = cfgGet(cfg, "MODULES", "git status time");

    auto normMode = [&](std::string m){
        if (m != "basic" && m != "fancy" && m != "ultra") m = "basic";
        return m;
    };

    mode = normMode(mode);

    if (curStyle != "multiline" && curStyle != "single") curStyle = "multiline";
    if (curShowHost != "1" && curShowHost != "0") curShowHost = "1";
    if (curSep.empty()) curSep = "│";
    if (curModules.empty()) curModules = "git status time";
    if (curSymbol.empty()) curSymbol = ">";

    auto renderPreview = [&](const std::string& m, const std::string& style, const std::string& sym, const std::string& sh, const std::string& sep, const std::string& mods){
        std::string u = getUser();
        std::string h = getHost();
        std::string p = prettyPath(getCwd(), u);

        std::vector<std::string> ms; {
            std::istringstream iss(mods);
            for (std::string w; iss >> w; ) ms.push_back(w);
        }

        if (m == "ultra") {
            int bgUser = 31;
            int bgPath = 25;
            int bgGit  = 208;
            int bgTime = 201;
            int bgOk   = 34;
            int bgBad  = 160;
            int bgSym  = 236;
            int fgDark = 16;
            int fgLight = 15;

            std::string tUser = u;

            if (sh != "0") tUser += " @ " + h;

            std::string tPath = p;
            std::string br = "main";

            int st = 0;

            std::string line1;

            int curBg = -1;

            auto escFg = [&](int c){ return std::string("\033[38;5;") + std::to_string(c) + "m"; };
            auto escBg = [&](int c){ return std::string("\033[48;5;") + std::to_string(c) + "m"; };
            auto escBgDefault = [&](){ return std::string("\033[49m"); };
            auto escFgDefault = [&](){ return std::string("\033[39m"); };

            auto plSep = [&](int leftBg, int rightBg){
                return escFg(leftBg) + escBg(rightBg) + "";
            };

            auto plStart = [&](int fg, int bg, const std::string& text){
                return escFg(fg) + escBg(bg) + " " + text + " ";
            };

            auto plEnd = [&](int lastBg){
                return plSep(lastBg, 0) + escBgDefault() + escFgDefault() + "\033[0m";
            };

            line1 += plStart(fgLight, bgUser, tUser);
            curBg = bgUser;
            line1 += plSep(curBg, bgPath);
            line1 += plStart(fgLight, bgPath, tPath);
            curBg = bgPath;

            for (auto &x : ms) {
                if (x == "git") {
                    line1 += plSep(curBg, bgGit);
                    line1 += plStart(fgDark, bgGit, "" + br);
                    curBg = bgGit;
                } else if (x == "status") {
                    int bgS = (st == 0) ? bgOk : bgBad;
                    int fgS = (st == 0) ? fgDark : fgLight;

                    line1 += plSep(curBg, bgS);
                    line1 += plStart(fgS, bgS, (st == 0) ? "OK" : std::to_string(st));
                    curBg = bgS;
                } else if (x == "time") {
                    line1 += plSep(curBg, bgTime);
                    line1 += plStart(fgLight, bgTime, "12:34");
                    curBg = bgTime;
                }
            }

            line1 += plEnd(curBg);

            std::string line2;

            line2 += Color::GREEN + "└─" + Color::RESET;
            line2 += escFg(208) + escBg(bgSym) + " " + sym + " ";
            line2 += escFg(bgSym) + "\033[49m\033[0m ";

            if (style == "single") std::cout << line1 << " " << line2 << "\n\n";
            else std::cout << line1 << "\n" << line2 << "\n\n";

            return;
        }

        if (m == "fancy") {
            std::string line1;
            line1 += Color::GREEN + "┌─" + Color::RESET;
            line1 += Color::GREEN + u + Color::RESET;

            if (sh != "0") {
                line1 += " " + Color::BLUE + "@" + Color::RESET;
                line1 += " " + Color::RED + h + Color::RESET;
            }

            line1 += " " + Color::CYAN + p + Color::RESET;

            for (auto &x : ms) {
                if (x == "git") {
                    line1 += " " + Color::GRAY + sep + Color::RESET + " ";
                    line1 += Color::ORANGE + "main" + Color::RESET;
                } else if (x == "status") {
                    line1 += " " + Color::GRAY + sep + Color::RESET + " ";
                    line1 += Color::GREEN + "OK" + Color::RESET;
                } else if (x == "time") {
                    line1 += " " + Color::GRAY + sep + Color::RESET + " ";
                    line1 += Color::PINK + "12:34" + Color::RESET;
                }
            }

            std::string line2 = Color::GREEN + "└─" + Color::RESET + Color::ORANGE + sym + Color::RESET + " ";

            if (style == "single") std::cout << line1 << " " << line2 << "\n\n";
            else std::cout << line1 << "\n" << line2 << "\n\n";

            return;
        }

        std::string line1;
        line1 += Color::GREEN + u + Color::RESET;

        if (sh != "0") {
            line1 += " " + Color::BLUE + "@" + Color::RESET;
            line1 += " " + Color::RED + h + Color::RESET;
        }

        line1 += " " + Color::CYAN + p + Color::RESET;

        for (auto &x : ms) {
            if (x == "git") line1 += " " + Color::ORANGE + "main" + Color::RESET;
            else if (x == "status") line1 += " " + Color::GREEN + "OK" + Color::RESET;
            else if (x == "time") line1 += " " + Color::PINK + "12:34" + Color::RESET;
        }

        std::string line2 = Color::ORANGE + sym + Color::RESET + " ";

        if (style == "single") std::cout << line1 << " " << line2 << "\n\n";
        else std::cout << line1 << "\n" << line2 << "\n\n";
    };

    
termios old{};
termios raw{};

bool rawOk = false;

if (tcgetattr(STDIN_FILENO, &old) == 0) {
    raw = old;

    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    rawOk = (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0);
}

auto restoreTerm = [&](){
    if (rawOk) tcsetattr(STDIN_FILENO, TCSANOW, &old);
};

auto enableRaw = [&](){
    if (rawOk) tcsetattr(STDIN_FILENO, TCSANOW, &raw);
};

auto readKey = [&](){
        unsigned char c = 0;

        if (read(STDIN_FILENO, &c, 1) != 1) return std::string();
        if (c != 0x1B) return std::string(1, (char)c);

        unsigned char a = 0;

        if (read(STDIN_FILENO, &a, 1) != 1) return std::string("\x1b");
        if (a != '[') return std::string("\x1b") + (char)a;

        unsigned char b = 0;

        if (read(STDIN_FILENO, &b, 1) != 1) return std::string("\x1b[");
        return std::string("\x1b[") + (char)b;
    };

    std::vector<std::string> modes = {"basic", "fancy", "ultra"};
    std::vector<std::string> styles = {"multiline", "single"};
    std::vector<std::string> syms = {">", "$", "➜", "❯", "⚡"};
    std::vector<std::string> seps = {"│", "·", "•", "/", ">"};
    std::vector<std::string> modPresets = {"git status time", "git time", "status time", "git status", "git", "time"};

    auto cycle = [&](const std::vector<std::string>& list, const std::string& cur, int dir){
        if (list.empty()) return cur;

        int idx = 0;

        for (int i = 0; i < (int)list.size(); i++) if (list[i] == cur) { idx = i; break; }

        idx += dir;

        if (idx < 0) idx = (int)list.size() - 1;
        if (idx >= (int)list.size()) idx = 0;

        return list[idx];
    };

    int sel = 0;

    auto draw = [&](){
        CLS();

        std::cout << "Biba Wizard (↑↓ move, ←→ change, Enter edit, s save, q abort)\n\n";

        renderPreview(mode, curStyle, curSymbol, curShowHost, curSep, curModules);

        auto row = [&](int i, const std::string& k, const std::string& v){
            if (sel == i) std::cout << "\033[7m";
            std::cout << (i+1) << ") " << k;

            if (k.size() < 10) std::cout << std::string(10 - k.size(), ' ');
            std::cout << ": " << v;
            
            if (sel == i) std::cout << "\033[0m";
            std::cout << "\n";
        };

        row(0, "mode", mode);
        row(1, "style", curStyle);
        row(2, "symbol", curSymbol);
        row(3, "show host", curShowHost);
        row(4, "modules", curModules);
        row(5, "sep", curSep);

        std::cout << "\n";
        if (sel == 6) std::cout << "\033[7m";
        std::cout << "s) save";
        if (sel == 6) std::cout << "\033[0m";
        std::cout << "\n";
        if (sel == 7) std::cout << "\033[7m";
        std::cout << "q) abort";
        if (sel == 7) std::cout << "\033[0m";
        std::cout << "\n";
    };

    while (true) {
        draw();

        auto k = readKey();

        if (k.empty()) continue;

        if (k == "q") { std::cout << "Aborted.\n"; restoreTerm(); return; }

        if (k == "s") {
            setCfgKey(cfg, "MODE", mode);
            setCfgKey(cfg, "THEME", curTheme);
            setCfgKey(cfg, "STYLE", curStyle);
            setCfgKey(cfg, "SYMBOL", curSymbol);
            setCfgKey(cfg, "SHOW_HOST", curShowHost);
            setCfgKey(cfg, "SEP", curSep);
            setCfgKey(cfg, "MODULES", trimLocal(curModules));

            std::cout << "Saved.\n";

            restoreTerm();

            return;
        }

        if (k == "\x1b[A") {
            sel--;
            if (sel < 0) sel = 7;
            continue;
        }

        if (k == "\x1b[B") {
            sel++;
            if (sel > 7) sel = 0;
            continue;
        }

        if (k == "\x1b[D") {
            if (sel == 0) mode = cycle(modes, mode, -1);
            else if (sel == 1) curStyle = cycle(styles, curStyle, -1);
            else if (sel == 2) curSymbol = cycle(syms, curSymbol, -1);
            else if (sel == 3) curShowHost = (curShowHost == "1") ? "0" : "1";
            else if (sel == 4) curModules = cycle(modPresets, curModules, -1);
            else if (sel == 5) curSep = cycle(seps, curSep, -1);

            continue;
        }

        if (k == "\x1b[C") {
            if (sel == 0) mode = cycle(modes, mode, +1);
            else if (sel == 1) curStyle = cycle(styles, curStyle, +1);
            else if (sel == 2) curSymbol = cycle(syms, curSymbol, +1);
            else if (sel == 3) curShowHost = (curShowHost == "1") ? "0" : "1";
            else if (sel == 4) curModules = cycle(modPresets, curModules, +1);
            else if (sel == 5) curSep = cycle(seps, curSep, +1);

            continue;
        }

        if (k == "\n" || k == "\r") {
            if (sel == 2) {
                restoreTerm();
                CLS();

                std::cout << "symbol presets: >  $  ➜  ❯  ⚡\n";
                std::cout << "symbol: ";
                std::string a;
                std::getline(std::cin, a);

                a = trimLocal(a);

                if (!a.empty()) curSymbol = a;

                restoreTerm();

                continue;
            }

            if (sel == 4) {
                restoreTerm();
                CLS();

                std::cout << "modules presets:\n";

                for (size_t i=0;i<modPresets.size();++i) std::cout << "  " << (i+1) << ") " << modPresets[i] << "\n";

                std::cout << "\nmodules: ";
                std::string a;
                std::getline(std::cin, a);

                a = trimLocal(a);

                if (!a.empty()) curModules = a;

                restoreTerm();

                continue;
            }

            if (sel == 5) {
                restoreTerm();
                CLS();

                std::cout << "sep presets: │  ·  •  /  >\n";
                std::cout << "sep: ";
                std::string a;
                std::getline(std::cin, a);

                a = trimLocal(a);

                if (!a.empty()) curSep = a;

                restoreTerm();

                continue;
            }

            if (sel == 6) {
                setCfgKey(cfg, "MODE", mode);
                setCfgKey(cfg, "THEME", curTheme);
                setCfgKey(cfg, "STYLE", curStyle);
                setCfgKey(cfg, "SYMBOL", curSymbol);
                setCfgKey(cfg, "SHOW_HOST", curShowHost);
                setCfgKey(cfg, "SEP", curSep);
                setCfgKey(cfg, "MODULES", trimLocal(curModules));

                std::cout << "Saved.\n";

                return;
            }

            if (sel == 7) { std::cout << "Aborted.\n"; restoreTerm(); return; }

            continue;
        }
    }
}
    if (sub == "theme") {
        if (args.size() < 3) { usage(); return; }
        const std::string t = args[2];
        setCfgKey(cfg, "THEME", t);
        std::cout << "Theme set.\n";
        return;
    }

    if (sub == "module") {
        if (args.size() < 3) { usage(); return; }
        setCfgModules(cfg, {args[2]}, false);
        std::cout << "Module set.\n";
        return;
    }

    if (sub == "modules") {
        if (args.size() < 3) { usage(); return; }
        std::vector<std::string> ms;
        for (size_t i=2;i<args.size();++i) ms.push_back(args[i]);
        setCfgModules(cfg, ms, false);
        std::cout << "Modules set.\n";
        return;
    }

    if (sub == "module-add") {
        if (args.size() < 3) { usage(); return; }
        setCfgModules(cfg, {args[2]}, true);
        std::cout << "Module added.\n";
        return;
    }

    if (sub == "off") {
        backupFile(bashrc);
        removeLinesContaining(bashrc, "source \"$HOME/.biba-bash/biba.sh\"");
        std::cout << "Disabled in .bashrc.\n";
        return;
    }

    if (sub == "reload") {
        execl("/usr/bin/bash", "bash", "-l", (char*)nullptr);
        perror("execl");
        return;
    }

    usage();
};

commands["zsh"] = [&](const std::vector<std::string>& args) {
    const std::string zshrc = homeDir() + "/.zshrc";
    const std::string ohmyzshDir = homeDir() + "/.oh-my-zsh";
    const std::string p10kDir = homeDir() + "/powerlevel10k";

    auto usage = [&](){
        std::cout <<
            "Usage:\n"
            "  zsh install\n"
            "  zsh theme <name>\n"
            "  zsh plugin <name>           (setzt GENAU dieses eine Plugin)\n"
            "  zsh plugins <p1> <p2> ...   (setzt Liste)\n"
            "  zsh plugin-add <name>       (merged in bestehende plugins=())\n"
            "  zsh p10k                    (install + source)\n"
            "  zsh reload                  (startet zsh)\n";
    };

    if (args.size() < 2) { usage(); return; }

    const std::string sub = args[1];

    backupFile(zshrc);

    if (sub == "install") {
        cmd("sudo apt-get update -y");
        cmd("sudo apt-get install -y zsh curl git");

        if (!fileExists(ohmyzshDir)) {
            cmd("RUNZSH=no CHSH=no KEEP_ZSHRC=yes sh -c \"$(curl -fsSL https://raw.githubusercontent.com/ohmyzsh/ohmyzsh/master/tools/install.sh)\"");
        } else {
            std::cout << "Oh My Zsh already installed.\n";
        }

        std::cout << "Install done.\n";

        return;
    }

    if (sub == "theme") {
        if (args.size() < 3) { usage(); return; }
        setZshKey(zshrc, "ZSH_THEME", "\"" + args[2] + "\"");
        std::cout << "Theme set.\n";
        return;
    }

    if (sub == "plugin") {
        if (args.size() < 3) { usage(); return; }
        setPlugins(zshrc, {args[2]}, false);
        std::cout << "Plugin set.\n";
        return;
    }

    if (sub == "plugins") {
        if (args.size() < 3) { usage(); return; }
        std::vector<std::string> pls;
        for (size_t i=2;i<args.size();++i) pls.push_back(args[i]);
        setPlugins(zshrc, pls, false);
        std::cout << "Plugins set.\n";
        return;
    }

    if (sub == "plugin-add") {
        if (args.size() < 3) { usage(); return; }
        setPlugins(zshrc, {args[2]}, true);
        std::cout << "Plugin added.\n";
        return;
    }

    if (sub == "p10k") {
        cmd("sudo apt-get install -y git");
        if (!fileExists(p10kDir)) {
            cmd("git clone --depth=1 https://github.com/romkatv/powerlevel10k.git \"" + p10kDir + "\"");
        }
        ensureLineInFile(zshrc, "source \"" + p10kDir + "/powerlevel10k.zsh-theme\"");
        std::cout << "Powerlevel10k enabled.\n";
        return;
    }

    if (sub == "reload") {
        // ersetzt aktuellen Prozess -> danach kein cout mehr möglich
        execl("/usr/bin/zsh", "zsh", (char*)nullptr);
        perror("execl");
        return;
    }

    usage();
};
}
