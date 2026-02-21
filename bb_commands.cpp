#include "framework.h"
#include "bb_commands.h"
#include "bb_exec.h"
#include "bb_search.h"

#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <string>
#include <vector>

using namespace std;

enum class PkgMgr {
    APT,
    DNF,
    YUM,
    PACMAN,
    ZYPPER,
    APK,
    XBPS,
    PKG,      // FreeBSD
    UNKNOWN
};

static bool binExists(const std::string& bin) {
    // "command -v" ist POSIX, läuft auf Linux/macOS/BSD
    std::string s = "command -v " + bin + " >/dev/null 2>&1";
    return (std::system(s.c_str()) == 0);
}

static PkgMgr detectPkgMgr() {
    // Priorität so wählen, dass "modernere" zuerst greifen
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
            // xbps macht update+upgrade zusammen in pkgUpdate() bereits,
            // aber wir lassen es hier trotzdem sauber:
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
            // Entfernt verwaiste Pakete
            cmd("bash -lc 'sudo pacman -Qtdq >/dev/null 2>&1 && sudo pacman -Rns --noconfirm $(pacman -Qtdq) || true'");
            break;
        case PkgMgr::ZYPPER:
            cmd("sudo zypper --non-interactive packages --orphaned"); // nur anzeigen
            break;
        case PkgMgr::APK:
            // kein direktes autoremove
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

void registerCommands(CommandMap& commands) {
    static const PkgMgr PM = detectPkgMgr();
    // optional zum Debuggen:
    // cout << "[pkg] detected: " << mgrName(PM) << "\n";

    commands["help"] = [&](const std::vector<std::string>&) {
        cout << "\nAvailable Commands:\n";
        cout << "------------------------------------------------------------\n";

        cout << "help        - Shows this help overview\n";
        cout << "cls/clear/c - Clears the screen\n";
        cout << "exit/exut   - Exits the program\n";

        cout << "l/dir       - Lists directory content (ls -alh)\n";
        cout << "cd <dir>    - Changes current directory\n";
        cout << "mk <dir>    - Creates directory and enters it\n";
        cout << "rm <path>   - Removes file or directory recursively\n";

        cout << "ip          - Shows network interfaces\n";
        cout << "ports       - Shows open ports and listening services\n";
        cout << "mem         - Shows RAM usage\n";
        cout << "disk        - Shows disk usage\n";

        cout << "update      - Runs apt update/upgrade/autoremove\n";
        cout << "install <p> - Installs apt package\n";
        cout << "uninstall<p>- Removes apt package completely\n";
        cout << "init        - Installs common admin tools\n";

        cout << "en <svc>    - Enables systemd service\n";
        cout << "dis <svc>   - Disables systemd service\n";
        cout << "start <svc> - Starts service\n";
        cout << "stop <svc>  - Stops service\n";
        cout << "restart<svc>- Restarts service\n";
        cout << "status <svc>- Shows service status\n";

        cout << "e <file>    - Opens file in nano\n";
        cout << "v <file>    - Opens file in vim\n";
        cout << "me <file>   - Opens file in mcedit\n";
        cout << "cl <path>   - Opens ranger file manager\n";

        cout << "search <p>  - Recursively searches for pattern\n";
        cout << "grub        - Edits grub config and updates it\n";
        cout << "bash        - Creates and executes temporary bash script\n";

        cout << "update-biba - Updates Bitch Batch\n";

        cout << "------------------------------------------------------------\n\n";
    };

    commands["cls"] = [&](const std::vector<std::string>&) {
        CLS();
    };

    commands["update-biba"] = [&](const std::vector<std::string>&) {
        cmd(
            "set -e; "
            "tmpdir=$(mktemp -d); "
            "trap 'rm -rf \"$tmpdir\"' EXIT; "
            "git clone --depth 1 https://github.com/Troyer05/bitchbatch.git \"$tmpdir/bitchbatch\"; "
            "cd \"$tmpdir/bitchbatch\"; "
            "chmod +x install.sh; "
            "sudo ./install.sh"
        );

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

    commands["bash"] = [&](const std::vector<std::string>&) {
        cmd("sudo nano bitchtmp.sh");
        cmd("sudo chmod +x bitchtmp.sh");
        cmd("sudo bash bitchtmp.sh");
        cmd("sudo rm bitchtmp.sh");

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

    commands["init"] = [&](const std::vector<std::string>&) {
        pkgUpdate(PM);

        std::vector<std::string> pkgs = {
            "curl","wget","git","htop","btop","iptraf","atop","iotop","glances","iperf",
            "gcc","lolcat","nano","vim","ranger","mc"
        };

        pkgInstall(PM, pkgs);
        pkgUpgrade(PM);
        pkgAutoremove(PM);

        cout << "\n";
    };

    // ==== ====

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


    commands["install"] = [&](const std::vector<std::string>& args) {
        if (args.size() < 2) { cout << "usage: install <package...>\n\n"; return; }

        std::vector<std::string> pkgs(args.begin() + 1, args.end());

        pkgInstall(PM, pkgs);
        
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

    commands["uninstall"] = [&](const std::vector<std::string>& args) {
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

    commands["search"] = [&](const std::vector<std::string>& args) {
        if (args.size() < 2) {
            std::cout << "usage: search <pattern>\n\n";
            return;
        }

        namespace fs = std::filesystem;
        
        searchRecursive(fs::current_path().string(), args[1]);

        std::cout << "\n";
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
