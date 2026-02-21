# Bitch Batch

Bitch Batch ist eine kleine, interaktive Admin-Shell in C++, die häufige Systemaufgaben hinter kurzen Befehlen bündelt.
Das Tool ist für Linux-/Unix-Umgebungen gedacht und erwartet Root-Rechte beim Start.

## Projektüberblick

- **Sprache:** C++
- **Architektur:** modulare `.cpp`/`.h`-Dateien (Commands, Exec, Readline, Search, Env, Utils)
- **Startpunkt:** `main.cpp`
- **Ziel:** ein schneller, vereinfachter Terminal-Workflow für Server-/Systemadministration

## Build & Start

### Manuell bauen

```bash
g++ *.cpp -Iheader -o biba
```

### Starten

```bash
sudo ./biba
```

> Hinweis: Das Programm prüft beim Start, ob der Nutzer `root` ist.

## Installation (bestehendes Skript)

Im Repository gibt es ein `install.sh`, das kompiliert und die Binary nach `/usr/sbin` kopiert:

```bash
sudo bash install.sh
```

Danach ist der Aufruf i. d. R. über `biba` möglich.

## Wichtige Funktionen

### Paketverwaltung (automatische Erkennung)

Beim Start wird ein verfügbarer Paketmanager erkannt (`apt`, `dnf`, `yum`, `pacman`, `zypper`, `apk`, `xbps`, `pkg`).
Darauf basieren u. a. diese Befehle:

- `update` – Paketquellen aktualisieren + Upgrades + Aufräumen
- `install <paket...>` – Pakete installieren
- `uninstall <paket...>` – Pakete entfernen
- `init` – typische Admin-Tools in einem Schritt installieren

### Datei-/Systembefehle

- `l` / `dir` – `ls -alh`
- `cd <pfad>` – Verzeichnis wechseln
- `mk <name>` – Verzeichnis anlegen und hinein wechseln
- `rm <pfad>` – rekursiv löschen (`rm -rf`)
- `search <pattern>` – rekursive Dateisuche im aktuellen Verzeichnis

### Service-/Systembefehle

- `start|stop|restart|status <service>`
- `en|dis <service>` (enable/disable)
- `ip`, `ports`, `mem`, `disk`

### Editor-/Tool-Shortcuts

- `e <file>` → `nano`
- `v <file>` → `vim`
- `me <file>` → `mcedit`
- `cl <path>` → `ranger`

### Shell-Features

- farbiger Prompt mit User/Host/Pfad
- Command-History
- Tab-Completion für Befehle und Dateipfade
- einfache Command-Chains (`;`, `&&`) und Pipes (`|`) für externe Befehle

## Projektprüfung (kurz)

Folgende Prüfung wurde durchgeführt:

- Build mit `g++ *.cpp -Iheader -o biba_check` erfolgreich.

Es sind aktuell keine automatisierten Tests im Projekt hinterlegt.

## Dateien & Module

- `main.cpp` – Programmstart, Prompt-Loop, Dispatch
- `bb_commands.cpp` – Command-Registry und Implementierungen
- `bb_exec.cpp` – Ausführung externer Befehle, Pipes/Chains
- `bb_readline.cpp` – interaktive Eingabe, History, Completion
- `bb_search.cpp` – rekursive Suche
- `bb_env.cpp` / `bb_util.cpp` – Umgebungs- und Hilfsfunktionen
- `header/` – Header-Dateien

## Sicherheitshinweise

- Mehrere Befehle nutzen `sudo` direkt.
- Einige Aktionen sind destruktiv (`rm -rf`, Paketentfernung, Service-Steuerung).
- Einsatz nur auf Systemen, auf denen diese Eingriffe bewusst gewünscht sind.
