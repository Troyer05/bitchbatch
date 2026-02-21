#include <iostream>
#include <string>
#include <vector>
#include <signal.h>

#include "framework.h"
#include "bb_types.h"
#include "bb_env.h"
#include "bb_readline.h"
#include "bb_util.h"
#include "bb_exec.h"
#include "bb_commands.h"

using namespace std;

volatile sig_atomic_t g_sigint = 0;

static void onSigInt(int) {
    g_sigint = 1;
    std::cout << "\n" << std::flush;
}

int main() {
    signal(SIGINT, onSigInt);
    signal(SIGQUIT, SIG_IGN);

    CLS();

    cout << "Bitch Batch\n\n";

    if (getUser() != "root") {
        cout << Color::RED << "Run me as root please :,(\n\n" << Color::RESET;
        return 0;
    }

    CommandMap commands;
    std::vector<std::string> history;

    registerCommands(commands);

    while (true) {
        const std::string user = getUser();
        const std::string host = getHost();
        const std::string cwd  = prettyPath(getCwd(), user);

        std::string prompt =
            Color::GREEN + user +
            Color::BLUE + " @ " +
            Color::RED + host +
            Color::CYAN + " : " +
            Color::CYAN + cwd +
            Color::YELLOW + " > " +
            Color::RESET;

        string line = readLineNice(prompt, commands, history);

        if (line.empty()) continue;
        if (history.empty() || history.back() != line) history.push_back(line);

        auto parts = splitSimple(line);

        if (parts.empty()) continue;

        auto it = commands.find(parts[0]);

        if (it != commands.end()) {
            it->second(parts);
            continue;
        }

        cmd(line);

        cout << "\n";
    }

    return 0;
}
