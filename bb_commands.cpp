#include "framework.h"
#include "bb_commands.h"
#include "bb_exec.h"
#include "bb_search.h"
#include "bb_signal.h"
#include "bb_env.h"
#include "history.h"

#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <string>
#include <vector>
#include <csignal>

using namespace std;

enum class PkgMgr {
    APT,
    DNF,
    YUM,
    PACMAN,
    ZYPPER,
    APK,
    XBPS,
    PKG,
    UNKNOWN
};

static bool binExists(const std::string& bin) {
    std::string s = "command -v " + bin + " >/dev/null 2>&1";
    return (std::system(s.c_str()) == 0);
}

static PkgMgr detectPkgMgr() {
    if (binExists("apt-get") || binExists("apt")) return PkgMgr::APT;
    if (binExists("dnf")) return PkgMgr::DNF;
    if (binExists("yum")) return PkgMgr::YUM;
    if (binExists("pacman")) return PkgMgr::PACMAN;
    if (binExists("zypper")) return PkgMgr::ZYPPER;
    if (binExists("apk")) return PkgMgr::APK;
    if (binExists("xbps-install")) return PkgMgr::XBPS;
    if (binExists("pkg")) return PkgMgr::PKG;

    return PkgMgr::UNKNOWN;
}

static std::string mgrName(PkgMgr m) {
    switch (m) {
        case PkgMgr::APT: return "apt";
        case PkgMgr::DNF: return "dnf";
        case PkgMgr::YUM: return "yum";
        case PkgMgr::PACMAN: return "pacman";
        case PkgMgr::ZYPPER: return "zypper";
        case PkgMgr::APK: return "apk";
        case PkgMgr::XBPS: return "xbps";
        case PkgMgr::PKG: return "pkg";
        default: return "unknown";
    }
}

static void pkgUpdate(PkgMgr m) {
    switch (m) {
        case PkgMgr::APT:
            cmd("sudo apt update");
            break;
        case PkgMgr::DNF:
            cmd("sudo dnf -y makecache");
            break;
        case PkgMgr::YUM:
            cmd("sudo yum -y makecache");
            break;
        case PkgMgr::PACMAN:
            cmd("sudo pacman -Sy --noconfirm");
            break;
        case PkgMgr::ZYPPER:
            cmd("sudo zypper --non-interactive refresh");
            break;
        case PkgMgr::APK:
            cmd("sudo apk update");
            break;
        case PkgMgr::XBPS:
            cmd("sudo xbps-install -Suy");
            break;
        case PkgMgr::PKG:
            cmd("sudo pkg update -f");
            break;
        default:
            std::cout << "No supported package manager found.\n";
            break;
    }
}

static void pkgUpgrade(PkgMgr m) {
    switch (m) {
        case PkgMgr::APT:
            cmd("sudo apt upgrade -y");
            break;
        case PkgMgr::DNF:
            cmd("sudo dnf -y upgrade");
            break;
        case PkgMgr::YUM:
            cmd("sudo yum -y update");
            break;
        case PkgMgr::PACMAN:
            cmd("sudo pacman -Syu --noconfirm");
            break;
        case PkgMgr::ZYPPER:
            cmd("sudo zypper --non-interactive update");
            break;
        case PkgMgr::APK:
            cmd("sudo apk upgrade");
            break;
        case PkgMgr::XBPS:
            cmd("sudo xbps-install -Suy");
            break;
        case PkgMgr::PKG:
            cmd("sudo pkg upgrade -y");
            break;
        default:
            std::cout << "No supported package manager found.\n";
            break;
    }
}

static void pkgAutoremove(PkgMgr m) {
    switch (m) {
        case PkgMgr::APT:
            cmd("sudo apt autoremove -y");
            break;
        case PkgMgr::DNF:
            cmd("sudo dnf -y autoremove");
            break;
        case PkgMgr::YUM:
            cmd("sudo yum -y autoremove");
            break;
        case PkgMgr::PACMAN:
            cmd("bash -lc 'sudo pacman -Qtdq >/dev/null 2>&1 && sudo pacman -Rns --noconfirm $(pacman -Qtdq) || true'");
            break;
        case PkgMgr::ZYPPER:
            cmd("sudo zypper --non-interactive packages --orphaned");
            break;
        case PkgMgr::APK:
            break;
        case PkgMgr::XBPS:
            cmd("sudo xbps-remove -o");
            break;
        case PkgMgr::PKG:
            cmd("sudo pkg autoremove -y");
            break;
        default:
            break;
    }
}

static void pkgInstall(PkgMgr m, const std::vector<std::string>& pkgs) {
    if (pkgs.empty()) return;

    switch (m) {
        case PkgMgr::APT: {
            std::string s = "sudo apt install -y";
            for (auto& p : pkgs) s += " " + p;
            cmd(s);
            break;
        }
        case PkgMgr::DNF: {
            std::string s = "sudo dnf -y install";
            for (auto& p : pkgs) s += " " + p;
            cmd(s);
            break;
        }
        case PkgMgr::YUM: {
            std::string s = "sudo yum -y install";
            for (auto& p : pkgs) s += " " + p;
            cmd(s);
            break;
        }
        case PkgMgr::PACMAN: {
            std::string s = "sudo pacman -S --noconfirm";
            for (auto& p : pkgs) s += " " + p;
            cmd(s);
            break;
        }
        case PkgMgr::ZYPPER: {
            std::string s = "sudo zypper --non-interactive install";
            for (auto& p : pkgs) s += " " + p;
            cmd(s);
            break;
        }
        case PkgMgr::APK: {
            std::string s = "sudo apk add";
            for (auto& p : pkgs) s += " " + p;
            cmd(s);
            break;
        }
        case PkgMgr::XBPS: {
            std::string s = "sudo xbps-install -y";
            for (auto& p : pkgs) s += " " + p;
            cmd(s);
            break;
        }
        case PkgMgr::PKG: {
            std::string s = "sudo pkg install -y";
            for (auto& p : pkgs) s += " " + p;
            cmd(s);
            break;
        }
        default:
            std::cout << "No supported package manager found.\n";
            break;
    }
}

static void pkgUninstall(PkgMgr m, const std::vector<std::string>& pkgs) {
    if (pkgs.empty()) return;

    switch (m) {
        case PkgMgr::APT: {
            std::string s = "sudo apt purge -y";
            for (auto& p : pkgs) s += " " + p;
            cmd(s);
            cmd("sudo apt autoremove -y");
            break;
        }
        case PkgMgr::DNF: {
            std::string s = "sudo dnf -y remove";
            for (auto& p : pkgs) s += " " + p;
            cmd(s);
            cmd("sudo dnf -y autoremove");
            break;
        }
        case PkgMgr::YUM: {
            std::string s = "sudo yum -y remove";
            for (auto& p : pkgs) s += " " + p;
            cmd(s);
            break;
        }
        case PkgMgr::PACMAN: {
            std::string s = "sudo pacman -Rns --noconfirm";
            for (auto& p : pkgs) s += " " + p;
            cmd(s);
            break;
        }
        case PkgMgr::ZYPPER: {
            std::string s = "sudo zypper --non-interactive remove";
            for (auto& p : pkgs) s += " " + p;
            cmd(s);
            break;
        }
        case PkgMgr::APK: {
            std::string s = "sudo apk del";
            for (auto& p : pkgs) s += " " + p;
            cmd(s);
            break;
        }
        case PkgMgr::XBPS: {
            std::string s = "sudo xbps-remove -Ry";
            for (auto& p : pkgs) s += " " + p;
            cmd(s);
            break;
        }
        case PkgMgr::PKG: {
            std::string s = "sudo pkg delete -y";
            for (auto& p : pkgs) s += " " + p;
            cmd(s);
            cmd("sudo pkg autoremove -y");
            break;
        }
        default:
            std::cout << "No supported package manager found.\n";
            break;
    }
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

    commands["help"] = [&](const std::vector<std::string>&) {
        cout << "\nBitchBatch - Help \n";
        cout << "====================================================================\n\n";

        cout << "Basics\n";
        cout << "--------------------------------------------------------------------\n";
        cout << "help                 Show this help\n";
        cout << "cls | clear | c       Clear the screen\n";
        cout << "exit | exut           Exit the program\n";
        cout << "info | credits        Show version / credits\n";
        cout << "\n";

        cout << "Filesystem\n";
        cout << "--------------------------------------------------------------------\n";
        cout << "l | dir               List directory content (ls -alh)\n";
        cout << "cd [dir]              Change directory (default: $HOME)\n";
        cout << "mk <dir...>           Create directory(s) and cd into first one\n";
        cout << "rm <path...>          Remove files/directories recursively\n";
        cout << "cl [path]             Open ranger file manager (default: .)\n";
        cout << "e <file...>           Edit file(s) with nano (sudo)\n";
        cout << "v [file...]           Open vim (or vim <file...>)\n";
        cout << "me [path]             Open mcedit (default: .)\n";
        cout << "\n";

        cout << "System / Network\n";
        cout << "--------------------------------------------------------------------\n";
        cout << "ip                   Show network interfaces (ip a)\n";
        cout << "ports                Show listening ports/services (ss -tulpn)\n";
        cout << "mem                  Show RAM usage (free -h)\n";
        cout << "disk                 Show disk usage (df -h)\n";
        cout << "grub                 Edit grub config and update-grub\n";
        cout << "off                  Power off the machine\n";
        cout << "\n";

        cout << "Package Management (" << mgrName(PM) << ")\n";
        cout << "--------------------------------------------------------------------\n";
        cout << "update               Update + upgrade + cleanup\n";
        cout << "i | install <pkg...>  Install package(s)\n";
        cout << "ui | uninstall <pkg...>  Uninstall package(s) (purge/remove)\n";
        cout << "init                 Install common admin tools\n";
        cout << "\n";

        cout << "systemd Services\n";
        cout << "--------------------------------------------------------------------\n";
        cout << "en <service>         Enable service\n";
        cout << "dis <service>        Disable service\n";
        cout << "start <service>      Start service\n";
        cout << "stop <service>       Stop service\n";
        cout << "restart <service>    Restart service\n";
        cout << "status <service>     Show service status\n";
        cout << "\n";

        cout << "Search\n";
        cout << "--------------------------------------------------------------------\n";
        cout << "search | find <pattern> [--in-files|-if]\n";
        cout << "                     Search recursively for pattern in names (default)\n";
        cout << "                     Use --in-files / -if to search inside file contents\n";
        cout << "\n";

        cout << "Scripting\n";
        cout << "--------------------------------------------------------------------\n";
        cout << "b | bash              Create & run temporary bash script (nano)\n";
        cout << "b | bash <file.sh>    Run a bash script (sudo bash <file.sh>)\n";
        cout << "\n";

        cout << "History\n";
        cout << "--------------------------------------------------------------------\n";
        cout << "history [N]           Print history (optionally last N entries)\n";
        cout << "history -c            Clear history\n";
        cout << "hist [N]           Print history (optionally last N entries)\n";
        cout << "hist -c            Clear history\n";
        cout << "ch                    Clear history\n";
        cout << "\n";

        cout << "Fun\n";
        cout << "--------------------------------------------------------------------\n";
        cout << "hack fbi             hollywood\n";
        cout << "hack matrix          cmatrix\n";
        cout << "\n";

        cout << "Maintenance\n";
        cout << "--------------------------------------------------------------------\n";
        cout << "update-biba          Reinstall/update from GitHub and restart\n";
        cout << "\n";

        cout << "Notes\n";
        cout << "--------------------------------------------------------------------\n";
        cout << "- Some commands use sudo.\n";
        cout << "- Package manager detected: " << mgrName(PM) << "\n";
        cout << "====================================================================\n\n";
    };

    commands["cls"] = [&](const std::vector<std::string>&) {
        CLS();
    };

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

    commands["clear"] = [&](const std::vector<std::string>&) {
        CLS();
    };

    commands["exit"] = [&](const std::vector<std::string>&) {
        exit(0);
    };

    commands["l"] = [&](const std::vector<std::string>&) {
        cmd("ls -alh");
        cout << "\n";
    };

    commands["grub"] = [&](const std::vector<std::string>&) {
        cmd("sudo nano /etc/default/grub");
        cmd("sudo update-grub");

        cout << "\n";
    };

    commands["update"] = [&](const std::vector<std::string>&) {
        pkgUpdate(PM);
        pkgUpgrade(PM);
        pkgAutoremove(PM);

        cout << "\n";
    };

    commands["dir"] = [&](const std::vector<std::string>&) {
        cmd("ls -alh");
        cout << "\n";
    };

    commands["c"] = [&](const std::vector<std::string>&) {
        CLS();
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

    commands["exut"] = [&](const std::vector<std::string>&) {
        cout << "\n";
        exit(0);
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
}
