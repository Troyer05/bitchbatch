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
#include "history.h"

using namespace std;

volatile sig_atomic_t g_sigint = 0;

static void onSigInt(int) {
    g_sigint = 1;
    std::cout << "\n" << std::flush;
}

int main() {
    signal(SIGINT, onSigInt);
    signal(SIGQUIT, SIG_IGN);

    // Load persistent history (shared with readLineNice)
    std::vector<std::string> hist;

    historyInit(500);
    loadHistory(hist, 500);

    CLS();

    cmd("figlet BITCHBATCH");

    if (getUser() != "root") {
        cout << Color::RED << "Run me as root please :,(\n\n" << Color::RESET;
        return 0;
    }

    cout << Color::YELLOW
         << "If you run this the first time, please type   init    and hit enter. Then restart Bitch Batch\n\n"
         << Color::RESET;

    CommandMap commands;
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
            Color::RESET + "\n" +
            Color::YELLOW + "> " +
            Color::RESET;

        std::string line = readLineNice(prompt, commands, historyVec());

        // Save external commands too
        appendHistory(historyVec(), line, 500);

        if (line.empty()) continue;

        // Parse tokens for builtin commands
        auto parts = splitSimple(line);
        if (parts.empty()) continue;

        auto it = commands.find(parts[0]);

        if (it != commands.end()) {
            it->second(parts);

            // Save history AFTER a successful command dispatch (optional but nice)
            appendHistory(hist, line, 500);
            continue;
        }

        cout << "\n";

        cmd(line);

        cout << "\n\n";
    }

    return 0;
}
