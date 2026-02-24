#pragma once
#include <string>

// Vorwärtsdeklaration (damit wir PkgMgr nicht doppelt definieren müssen)
enum class PkgMgr;

void basicHelp(PkgMgr pm);
void helpFor(const std::string& command, PkgMgr pm);
