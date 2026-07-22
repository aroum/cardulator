#pragma once

#include <string>
#include <vector>
#include "app_state.h"

inline std::vector<std::string> splitPlotArgs(const std::string& args_str) {
    std::vector<std::string> args;
    std::string cur = "";
    int paren_depth = 0;
    int bracket_depth = 0;
    bool in_quotes = false;
    char quote_char = 0;
    for (char c : args_str) {
        if (c == '"' || c == '\'') {
            if (!in_quotes) {
                in_quotes = true;
                quote_char = c;
            } else if (c == quote_char) {
                in_quotes = false;
            }
            cur += c;
        } else if (in_quotes) {
            cur += c;
        } else if (c == '(') {
            paren_depth++;
            cur += c;
        } else if (c == ')') {
            paren_depth--;
            cur += c;
        } else if (c == '[') {
            bracket_depth++;
            cur += c;
        } else if (c == ']') {
            bracket_depth--;
            cur += c;
        } else if (c == ',' && paren_depth == 0 && bracket_depth == 0) {
            args.push_back(cur);
            cur = "";
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) args.push_back(cur);
    for (auto& a : args) {
        a.erase(0, a.find_first_not_of(" \t"));
        a.erase(a.find_last_not_of(" \t") + 1);
    }
    return args;
}

inline std::string cleanQuotes(const std::string& str) {
    if (str.size() >= 2 && (str.front() == '"' || str.front() == '\'') && str.back() == str.front()) {
        return str.substr(1, str.size() - 2);
    }
    return str;
}

bool handlePlotCommands(const std::string& line, std::string& err, double& result);
