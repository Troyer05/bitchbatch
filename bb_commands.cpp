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

#include <unistd.h>
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

using namespace std;

static inline std::string trimLocal(std::string s) {
    auto notSpace = [](unsigned char c){ return !std::isspace(c); };

    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());

    return s;
}

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
        cmd("sudo rm -r bitchbatch");
        cmd("sudo rm /usr/local/bin/biba");
        cmd("sudo rm /sbin/biba");
        cmd("git clone https://github.com/Troyer05/bitchbatch.git");
        cmd("sudo chmod +x bitchbatch/install.sh");
        cmd("sudo bash bitchbatch/install.sh");
        cmd("sudo cp bitchbatch/biba /usr/local/bin/biba");
        cmd("sudo cp bitchbatch/biba /usr/sbin/biba");
        cmd("sudo cp bitchbatch/biba /sbin/biba");

        cout << "\nUpdate finished. Restarting...\n\n";

        char* argv[] = { (char*)"biba", nullptr };

        execvp(argv[0], argv);
        perror("execvp");
        exit(1);
    };

    commands["c"] = commands["cls"] = commands["clear"] = [&](const std::vector<std::string>&) {
        CLS();
    };

    commands["exut"] = commands["exit"] = [&](const std::vector<std::string>&) {
        exit(0);
    };

    commands["l"] = [&](const std::vector<std::string>&) {
        cmd("ls -alh");
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

    commands["dir"] = [&](const std::vector<std::string>&) {
        cmd("ls -alh");
        cout << "\n";
    };

    commands["ports"] = [&](const std::vector<std::string>&) {
        cmd("ss -tulpn");
        cout << "\n";
    };

    commands["ip"] = [&](const std::vector<std::string>&) {
        cmd("ip a");
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
}