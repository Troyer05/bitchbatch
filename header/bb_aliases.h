#pragma once
#include <string>
#include <unordered_map>
#include <vector>

std::string alias_file_path();

struct AliasStore;

AliasStore& getAliases();

const std::string& getAliasPath();

struct AliasStore {
    std::unordered_map<std::string, std::string> map;

    bool load(const std::string& path);
    bool save(const std::string& path) const;

    void set(const std::string& name, const std::string& expansion);
    bool del(const std::string& name);

    std::vector<std::string> expand_first_token(const std::vector<std::string>& args) const;
};
