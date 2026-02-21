#include <iostream>
#include <string>
#include <cstdlib>
#include <limits>
#include "framework.h"

void CLS() {
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif
}

void CLS_ENTER() {
    std::cout << "Drücken Sie ENTER, um fortzufahren...";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif
}

std::string userInput(int line) {
    if (line != -1) {
        std::cout << line << "| ";
    }

    std::string input;
    std::getline(std::cin, input);

    return input;
}

std::string getInput() {
    std::string input;
    std::getline(std::cin, input);

    return input;
}
