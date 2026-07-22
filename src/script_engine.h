#pragma once

#include <string>
#include <vector>
#include "app_state.h"

inline bool isBlockStart(const std::string& line) {
    if (line.empty()) return false;
    return line.back() == '{';
}

inline bool isBlockEnd(const std::string& line) {
    if (line.empty()) return false;
    return line[0] == '}' || line == "}";
}

inline size_t findMatchingBlockEnd(const std::vector<std::string>& lines, size_t start_ip, bool stop_at_else_elif = false) {
    int nesting = 1;
    for (size_t k = start_ip + 1; k < lines.size(); ++k) {
        std::string line = lines[k];
        line.erase(0, line.find_first_not_of(" \t\r"));
        line.erase(line.find_last_not_of(" \t\r") + 1);
        bool starts_with_close = (!line.empty() && line[0] == '}');
        bool ends_with_open = (!line.empty() && line.back() == '{');

        if (stop_at_else_elif && nesting == 1 && starts_with_close && ends_with_open) {
            return k;
        }

        if (starts_with_close && ends_with_open) {
            continue;
        } else if (ends_with_open) {
            nesting++;
        } else if (starts_with_close || line == "}") {
            nesting--;
            if (nesting == 0) {
                return k;
            }
        }
    }
    return std::string::npos;
}

inline std::vector<std::string> splitIntoStatements(const std::string& code) {
    std::vector<std::string> statements;
    std::string current_stmt = "";
    bool in_quotes = false;
    
    auto push_stmt = [&](const std::string& raw) {
        std::string s = raw;
        size_t first = s.find_first_not_of(" \t\r");
        size_t last = s.find_last_not_of(" \t\r");
        if (first == std::string::npos || last == std::string::npos) return;
        s = s.substr(first, last - first + 1);
        
        auto trim = [](std::string& str) {
            str.erase(0, str.find_first_not_of(" \t\r"));
            str.erase(str.find_last_not_of(" \t\r") + 1);
        };

        if (s.size() > 2 && s.substr(s.size() - 2) == "++") {
            std::string var = s.substr(0, s.size() - 2);
            trim(var);
            s = var + " = " + var + " + 1";
        }
        else if (s.size() > 2 && s.substr(s.size() - 2) == "--") {
            std::string var = s.substr(0, s.size() - 2);
            trim(var);
            s = var + " = " + var + " - 1";
        }
        else if (size_t plus_eq = s.find("+="); plus_eq != std::string::npos) {
            std::string var = s.substr(0, plus_eq);
            std::string val = s.substr(plus_eq + 2);
            trim(var);
            trim(val);
            s = var + " = " + var + " + (" + val + ")";
        }
        else if (size_t minus_eq = s.find("-="); minus_eq != std::string::npos) {
            std::string var = s.substr(0, minus_eq);
            std::string val = s.substr(minus_eq + 2);
            trim(var);
            trim(val);
            s = var + " = " + var + " - (" + val + ")";
        }
        
        statements.push_back(s);
    };
    
    char quote_char = 0;
    int paren_depth = 0;
    for (size_t i = 0; i < code.size(); ++i) {
        char c = code[i];
        if (c == '"' || c == '\'') {
            if (!in_quotes) {
                in_quotes = true;
                quote_char = c;
            } else if (c == quote_char) {
                in_quotes = false;
            }
            current_stmt += c;
        } else if (!in_quotes && c == '(') {
            paren_depth++;
            current_stmt += c;
        } else if (!in_quotes && c == ')') {
            if (paren_depth > 0) paren_depth--;
            current_stmt += c;
        } else if (c == '#' && !in_quotes) {
            while (i < code.size() && code[i] != '\n') {
                i++;
            }
            if (!current_stmt.empty()) {
                push_stmt(current_stmt);
                current_stmt = "";
            }
        } else if (c == ';' && !in_quotes && paren_depth == 0) {
            if (!current_stmt.empty()) {
                push_stmt(current_stmt);
                current_stmt = "";
            }
        } else if (c == '\n' || c == '\r') {
            if (!current_stmt.empty()) {
                push_stmt(current_stmt);
                current_stmt = "";
            }
        } else {
            current_stmt += c;
        }
    }
    if (!current_stmt.empty()) {
        push_stmt(current_stmt);
    }
    return statements;
}

extern double script_return_val;
extern bool script_has_returned;

void runScript(const std::string& code);
