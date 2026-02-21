#ifndef BB_SEARCH_H
#define BB_SEARCH_H

#include <string>
#include <signal.h>

extern volatile sig_atomic_t g_sigint;

void searchRecursive(const std::string& root, const std::string& pattern);

#endif