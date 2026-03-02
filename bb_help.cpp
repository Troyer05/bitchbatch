#include "bb_help.h"

#include <iostream>
#include <cctype>

using std::cout;
using std::string;

extern std::string mgrName(PkgMgr m);

static std::string normCmd(std::string c) {
    while (!c.empty() && std::isspace((unsigned char)c.front())) c.erase(c.begin());
    while (!c.empty() && std::isspace((unsigned char)c.back())) c.pop_back();

    for (auto& ch : c) ch = (char)std::tolower((unsigned char)ch);

    if (c == "cls" || c == "clear" || c == "c") return "clear";
    if (c == "exit" || c == "exut") return "exit";
    if (c == "u" || c == "update") return "update";
    if (c == "i" || c == "install") return "install";
    if (c == "ui" || c == "uninstall") return "uninstall";
    if (c == "hist" || c == "history") return "history";
    if (c == "dir" || c == "l") return "ls";
    if (c == "find" || c == "search") return "search";
    if (c == "credits" || c == "info") return "info";
    if (c == "b" || c == "bash") return "bash";
    if (c == "e") return "e";
    if (c == "v") return "v";
    if (c == "me") return "me";
    if (c == "alias") return "alias";
    if (c == "unalias") return "unalias";

    return c;
}

static void printHdr(const std::string& title) {
    cout << "\n" << title << "\n";
    cout << "--------------------------------------------------------------------\n";
}

static void printUnknown(const std::string& c) {
    cout << "Unknown command: " << c << "\n\n";
    cout << "Try: help\n";
}

void basicHelp(PkgMgr pm) {
    cout << "\nBitchBatch - Help (" << mgrName(pm) << ")\n";
    cout << "====================================================================\n\n";

    cout << "Basics\n";
    cout << "--------------------------------------------------------------------\n";
    cout << "help                  Show this help\n";
    cout << "help <command>         Show detailed help for a command\n";
    cout << "cls | clear | c        Clear the screen\n";
    cout << "exit | exut            Exit the program\n";
    cout << "info | credits         Show version / credits\n\n";

    cout << "Aliases\n";
    cout << "--------------------------------------------------------------------\n";
    cout << "alias                 List all aliases\n";
    cout << "alias name=command    Create alias\n";
    cout << "unalias <name>        Remove alias\n\n";

    cout << "Filesystem\n";
    cout << "--------------------------------------------------------------------\n";
    cout << "l | dir                List directory content (ls -alh)\n";
    cout << "cd [dir]               Change directory (default: $HOME)\n";
    cout << "mk <dir...>            Create directory(s) and cd into first one\n";
    cout << "rm <path...>           Remove file/dir recursively\n";
    cout << "cl [path]              Open ranger (default: .)\n";
    cout << "e <file...>            Open nano (sudo)\n";
    cout << "v [file...]            Open vim\n";
    cout << "vsc <path>             Opens VSCode on path - without path given, the actual dir is gonna be opened\n";
    cout << "me [path]              Open mcedit (default: .)\n\n";

    cout << "System / Network\n";
    cout << "--------------------------------------------------------------------\n";
    cout << "ip                    Show network interfaces\n";
    cout << "ports                 Show open/listening ports\n";
    cout << "mem                   Show RAM usage\n";
    cout << "disk                  Show disk usage\n";
    cout << "grub                  Edit grub config + update\n";
    cout << "off                   Power off machine\n\n";

    cout << "Package Management (" << mgrName(pm) << ")\n";
    cout << "--------------------------------------------------------------------\n";
    cout << "u | update             Update + upgrade + cleanup\n";
    cout << "i | install <pkg...>   Install packages\n";
    cout << "ui | uninstall <pkg...> Uninstall packages\n";
    cout << "init                  Install common admin tools\n\n";

    cout << "systemd Services\n";
    cout << "--------------------------------------------------------------------\n";
    cout << "en <service>          Enable service\n";
    cout << "dis <service>         Disable service\n";
    cout << "start <service>       Start service\n";
    cout << "stop <service>        Stop service\n";
    cout << "restart <service>     Restart service\n";
    cout << "status <service>      Show service status\n\n";

    cout << "Search\n";
    cout << "--------------------------------------------------------------------\n";
    cout << "search | find <pattern> [--in-files|-if]\n\n";

    cout << "Scripting\n";
    cout << "--------------------------------------------------------------------\n";
    cout << "b | bash               Temp bash script (nano) OR run file.sh\n\n";

    cout << "History\n";
    cout << "--------------------------------------------------------------------\n";
    cout << "history | hist [N]     Print history (optionally last N)\n";
    cout << "history -c | hist -c   Clear history\n";
    cout << "ch                     Clear history\n\n";

    cout << "Maintenance\n";
    cout << "--------------------------------------------------------------------\n";
    cout << "update-biba           Update from GitHub and restart\n\n";

    cout << "Style\n";
    cout << "--------------------------------------------------------------------\n";
    cout << "mcfont                Switch terminal to Monocraft (best effort)\n";
    cout << "mcfont -no            Back to default monospace\n\n";

    cout << "Development\n";
    cout << "--------------------------------------------------------------------\n";
    cout << "deploy <msg>          Deploys on github with provided commit message\n";
    cout << "rebuild               Rebuilds the test binary\n\n";

    cout << "Notes\n";
    cout << "--------------------------------------------------------------------\n";
    cout << "- Some commands use sudo.\n";
    cout << "- Package manager detected: " << mgrName(pm) << "\n";
    cout << "====================================================================\n\n";
}

void helpFor(const std::string& command, PkgMgr pm) {
    std::string c = normCmd(command);

    if (c == "alias") {
        printHdr("alias");

        cout << "Usage:\n";
        cout << "  alias\n";
        cout << "  alias name=command...\n";
        cout << "  alias name command...\n\n";

        cout << "Description:\n";
        cout << "  Creates or lists command aliases.\n\n";

        cout << "Examples:\n";
        cout << "  alias ll=l -alh\n";
        cout << "  alias x=cls\n";
        cout << "  alias u=\"update && mem\"\n\n";

        cout << "Notes:\n";
        cout << "  Aliases are stored permanently in ~/.config/bitchbatch/aliases\n";

        return;
    }

    if (c == "unalias") {
        printHdr("unalias");

        cout << "Usage:\n";
        cout << "  unalias <name>\n\n";

        cout << "Description:\n";
        cout << "  Removes an existing alias.\n\n";

        cout << "Example:\n";
        cout << "  unalias ll\n";

        return;
    }

    if (c == "help") {
        printHdr("help");

        cout << "Usage:\n  help\n  help <command>\n\n";
        cout << "Examples:\n  help install\n  help search\n";

        return;
    }

    if (c == "clear") {
        printHdr("clear / cls / c");

        cout << "Usage:\n  clear\n  cls\n  c\n\n";
        cout << "Description:\n  Clears the terminal screen.\n";

        return;
    }

    if (c == "exit") {
        printHdr("exit / exut");

        cout << "Usage:\n  exit\n  exut\n\n";
        cout << "Description:\n  Exits BitchBatch.\n";

        return;
    }

    if (c == "info") {
        printHdr("info / credits");

        cout << "Usage:\n  info\n  credits\n\n";
        cout << "Description:\n  Prints version and credits.\n";

        return;
    }

    if (c == "mcfont") {
        printHdr("mcfont");

        cout << "Usage:\n";
        cout << "  mcfont\n";
        cout << "  mcfont -no\n\n";

        cout << "Description:\n";
        cout << "  Tries to switch your terminal font to Monocraft.\n";
        cout << "  Works by editing terminal config files (depends on environment).\n\n";

        cout << "Notes:\n";
        cout << "  - Windows Terminal needs Monocraft installed on Windows host too.\n";
        cout << "  - Restart the terminal after changing.\n";
        cout << "  - State is stored in ~/.config/bitchbatch/mcfont\n";

        return;
    }

    if (c == "ls") {
        printHdr("l / dir");

        cout << "Usage:\n  l\n  dir\n\n";
        cout << "Description:\n  Runs: ls -alh\n";

        return;
    }

    if (c == "cd") {
        printHdr("cd");

        cout << "Usage:\n  cd\n  cd <dir>\n\n";
        cout << "Description:\n  Changes directory. Without args goes to $HOME.\n";

        return;
    }

    if (c == "mk") {
        printHdr("mk");

        cout << "Usage:\n  mk <dir...>\n\n";
        cout << "Description:\n  mkdir -p <dir...> then cd into first dir.\n";

        return;
    }

    if (c == "rm") {
        printHdr("rm");

        cout << "Usage:\n  rm <path...>\n\n";
        cout << "Description:\n  Removes recursively (rm -rf).\n";
        cout << "Warning:\n  Destructive. Be careful with / and ..\n";

        return;
    }

    if (c == "ip") {
        printHdr("ip");

        cout << "Usage:\n  ip\n\n";
        cout << "Description:\n  Runs: ip a\n";

        return;
    }

    if (c == "ports") {
        printHdr("ports");

        cout << "Usage:\n  ports\n\n";
        cout << "Description:\n  Runs: ss -tulpn\n";

        return;
    }

    if (c == "mem") {
        printHdr("mem");

        cout << "Usage:\n  mem\n\n";
        cout << "Description:\n  Runs: free -h\n";

        return;
    }

    if (c == "disk") {
        printHdr("disk");

        cout << "Usage:\n  disk\n\n";
        cout << "Description:\n  Runs: df -h\n";

        return;
    }

    if (c == "grub") {
        printHdr("grub");

        cout << "Usage:\n  grub\n\n";
        cout << "Description:\n  Opens /etc/default/grub in nano and runs update-grub.\n";
        cout << "Notes:\n  Requires sudo.\n";

        return;
    }

    if (c == "update") {
        printHdr("u / update");

        cout << "Usage:\n  u\n  update\n\n";
        cout << "Description:\n  Update + upgrade + cleanup via " << mgrName(pm) << ".\n";
        cout << "Notes:\n  Requires sudo.\n";

        return;
    }

    if (c == "install") {
        printHdr("i / install");

        cout << "Usage:\n  i <pkg...>\n  install <pkg...>\n\n";
        cout << "Description:\n  Installs packages via " << mgrName(pm) << ".\n";
        cout << "Examples:\n  install vim\n  i curl wget git\n";

        return;
    }

    if (c == "uninstall") {
        printHdr("ui / uninstall");

        cout << "Usage:\n  ui <pkg...>\n  uninstall <pkg...>\n\n";
        cout << "Description:\n  Removes packages via " << mgrName(pm) << ".\n";
        cout << "Notes:\n  May run autoremove depending on distro.\n";

        return;
    }

    if (c == "init") {
        printHdr("init");

        cout << "Usage:\n  init\n\n";
        cout << "Description:\n  Installs common admin tools then upgrade + cleanup.\n";
        cout << "Notes:\n  Requires sudo.\n";

        return;
    }

    if (c == "en" || c == "dis" || c == "start" || c == "stop" || c == "restart" || c == "status") {
        printHdr("systemd services");

        cout << "Usage:\n";
        cout << "  en <service>\n";
        cout << "  dis <service>\n";
        cout << "  start <service>\n";
        cout << "  stop <service>\n";
        cout << "  restart <service>\n";
        cout << "  status <service>\n\n";
        cout << "Examples:\n  en ssh\n  restart nginx\n  status docker\n";
        cout << "Notes:\n  Requires sudo.\n";

        return;
    }

    if (c == "search") {
        printHdr("search / find");

        cout << "Usage:\n  search <pattern> [--in-files|-if]\n\n";
        cout << "Description:\n";
        cout << "  Default: search file/dir names.\n";
        cout << "  With --in-files/-if: search inside file contents.\n";
        cout << "Examples:\n  search nginx\n  search \"server_name\" --in-files\n";

        return;
    }

    if (c == "bash") {
        printHdr("b / bash");

        cout << "Usage:\n  b\n  bash\n  b <file.sh>\n  bash <file.sh>\n\n";
        cout << "Description:\n";
        cout << "  No args: create temp script in nano, run it, delete it.\n";
        cout << "  With file: run via sudo bash <file.sh>\n";
        cout << "Notes:\n  Requires sudo.\n";

        return;
    }

    if (c == "history") {
        printHdr("history / hist");

        cout << "Usage:\n";
        cout << "  history [N]\n  hist [N]\n";
        cout << "  history -c\n  hist -c\n";
        cout << "  ch\n\n";
        cout << "Description:\n  Prints or clears history.\n";

        return;
    }

    if (c == "off") {
        printHdr("off");

        cout << "Usage:\n  off\n\n";
        cout << "Description:\n  Power off machine (poweroff) and exits.\n";

        return;
    }

    if (c == "update-biba") {
        printHdr("update-biba");

        cout << "Usage:\n  update-biba\n\n";
        cout << "Description:\n  Updates from GitHub, reinstalls, copies biba and restarts.\n";
        cout << "Notes:\n  Requires sudo + git + network.\n";

        return;
    }

    if (c == "e" || c == "v" || c == "me") {
        printHdr("File Editor");

        cout << "Usage:\n  e|v|me <file>\n\n";
        cout << "Description:\n  Edits a file with nano/vim/mcedit.\n";
        cout << "Notes:\n  e = nano / v = vim / me = mcedit.\n";

        return;
    }

    printUnknown(command);
}