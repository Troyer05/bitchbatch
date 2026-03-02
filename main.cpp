#include <iostream>
#include <string>
#include <vector>
#include <signal.h>
#include <locale.h>

#include "framework.h"
#include "bb_types.h"
#include "bb_env.h"
#include "bb_readline.h"
#include "bb_util.h"
#include "bb_exec.h"
#include "bb_commands.h"
#include "bb_prompt.h"
#include "bb_aliases.h"
#include "history.h"

using namespace std;

volatile sig_atomic_t g_sigint = 0;

static void onSigInt(int) {
    g_sigint = 1;
    std::cout << "\n" << std::flush;
}

int main() {
    setlocale(LC_ALL, "");
    signal(SIGINT, onSigInt);
    signal(SIGQUIT, SIG_IGN);

    std::vector<std::string> hist;

    historyInit(500);
    loadHistory(hist, 500);

    (void)getAliases();

    std::string figlet = findCommand("figlet");
    std::string lolcat = findCommand("lolcat");

    CLS();

    if (!figlet.empty()) {
        if (!lolcat.empty()) {
            cmd(figlet + " BITCHBATCH | " + lolcat);
        } else {
            cmd(figlet + " BITCHBATCH");
        }
    } else {
        cout << Color::GREEN << "BITCHBATCH\n\n";
    }

    if (getUser() != "root") {
        cout << Color::RED << "Run me as root please :,(\n\n" << Color::RESET;
        return 0;
    }

    cout << Color::YELLOW
         << "If you run this the first time, please type   init    and hit enter. Then restart Bitch Batch\n"
         << Color::CYAN
         << "If you are new to Linux, type learn-linux-terminal and hit enter - to enter a small linux tutorial\n"
         << Color::GREEN
         << "Thanks for using BITCHBATCH :)\n\n"
         << Color::RESET;

    CommandMap commands;
    registerCommands(commands);

    while (true) {
        const std::string user = getUser();
        const std::string host = getHost();
        const std::string cwd  = prettyPath(getCwd(), user);

        std::string prompt = makeBibaPrompt(user, host, cwd);
        std::string line = readLineNice(prompt, commands, historyVec());

        appendHistory(historyVec(), line, 500);

        if (line.empty()) continue;

        auto parts = splitSimple(line);

        if (parts.empty()) continue;

        parts = getAliases().expand_first_token(parts);

        if (parts.empty()) continue;

        auto joinParts = [&](const std::vector<std::string>& v) {
            std::string out;

            for (size_t i = 0; i < v.size(); i++) {
                if (i) out += " ";
                out += v[i];
            }

            return out;
        };

        auto it = commands.find(parts[0]);

        if (it != commands.end()) {
            it->second(parts);

            appendHistory(hist, line, 500);

            continue;
        }

        cout << "\n";

        cmd(joinParts(parts));

        cout << "\n\n";
    }

    return 0;
}
