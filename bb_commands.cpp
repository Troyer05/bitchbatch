#include "framework.h"
#include "bb_commands.h"
#include "bb_exec.h"
#include "bb_search.h"

#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <filesystem>

using namespace std;

void registerCommands(CommandMap& commands) {
    commands["cls"] = [&](const std::vector<std::string>&) {
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
        cmd("sudo apt update");
        cmd("sudo apt upgrade -y");
        cmd("sudo apt autoremove -y");

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

    commands["mem"] = [&](const std::vector<std::string>&) {
        cmd("free -h");
        cout << "\n";
    };

    commands["exut"] = [&](const std::vector<std::string>&) {
        cout << "\n";
        exit(0);
    };

    commands["init"] = [&](const std::vector<std::string>&) {
        cmd("sudo apt update");
        cmd("sudo apt install -y htop");
        cmd("sudo apt install -y btop");
        cmd("sudo apt install -y iptraf");
        cmd("sudo apt install -y wget");
        cmd("sudo apt install -y curl");
        cmd("sudo apt install -y atop");
        cmd("sudo apt install -y iotop");
        cmd("sudo apt install -y glances");
        cmd("sudo apt install -y iperf");
        cmd("sudo apt install -y gcc");
        cmd("sudo apt install -y lolcat");
        cmd("sudo apt install -y nano");
        cmd("sudo apt install -y vim");

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
        if (args.size() < 2) {
            cout << "usage: install <apt-program>\n\n";
            return;
        }

        std::vector<std::string> p = {"sudo", "apt", "install", "-y"};
        for (size_t i = 1; i < args.size(); i++) p.push_back(args[i]);

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

    commands["uninstall"] = [&](const std::vector<std::string>& args) {
        if (args.size() < 2) {
            cout << "usage: uninstall <apt-program>\n\n";
            return;
        }

        std::vector<std::string> p = {"sudo", "apt", "purge", "-y"};
        for (size_t i = 1; i < args.size(); i++) p.push_back(args[i]);

        cmdArgs(p);
        cmd("sudo apt autoremove -y");

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
}
