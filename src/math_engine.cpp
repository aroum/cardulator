#include "math_engine.h"
#include "tinyexpr.h"
#include <sstream>
#include <iomanip>

std::vector<double> parseArrayExpr(const std::string& rhs, bool& is_array, std::string& err);
std::string preprocessLen(const std::string& s);
std::string preprocessVectorStats(const std::string& s);

inline std::vector<UserFunc> getParsedUserFuncs() {
    std::vector<UserFunc> res;
    for (const auto& f_str : user_funcs) {
        size_t eq = f_str.find('=');
        if (eq == std::string::npos) continue;
        std::string lhs = f_str.substr(0, eq);
        std::string rhs = f_str.substr(eq + 1);
        
        lhs.erase(0, lhs.find_first_not_of(" \t"));
        lhs.erase(lhs.find_last_not_of(" \t") + 1);
        rhs.erase(0, rhs.find_first_not_of(" \t"));
        rhs.erase(rhs.find_last_not_of(" \t") + 1);
        
        size_t paren_open = lhs.find('(');
        size_t paren_close = lhs.find(')');
        if (paren_open == std::string::npos || paren_close == std::string::npos || paren_close < paren_open) continue;
        
        std::string name = lhs.substr(0, paren_open);
        name.erase(0, name.find_first_not_of(" \t"));
        name.erase(name.find_last_not_of(" \t") + 1);
        
        std::string params_str = lhs.substr(paren_open + 1, paren_close - paren_open - 1);
        std::vector<std::string> params;
        std::stringstream ss(params_str);
        std::string p;
        while (std::getline(ss, p, ',')) {
            p.erase(0, p.find_first_not_of(" \t"));
            p.erase(p.find_last_not_of(" \t") + 1);
            if (!p.empty()) params.push_back(p);
        }
        res.push_back({name, params, rhs});
    }
    return res;
}

inline double executeScriptFunc(const std::string& name, const std::vector<double>& arg_vals, std::string& err) {
    auto it = std::find_if(user_script_funcs.begin(), user_script_funcs.end(), [&](const CustomScriptFunc& f) {
        return f.name == name;
    });
    if (it == user_script_funcs.end()) {
        err = "Func Not Found";
        return std::numeric_limits<double>::quiet_NaN();
    }
    
    const auto& func = *it;
    if (arg_vals.size() != func.params.size()) {
        err = "Arg Count Mismatch";
        return std::numeric_limits<double>::quiet_NaN();
    }

    auto orig_args = user_args;
    auto orig_arrays = user_arrays;

    for (size_t i = 0; i < func.params.size(); ++i) {
        user_args.erase(std::remove_if(user_args.begin(), user_args.end(), [&](const UserArg& a) {
            return a.name == func.params[i];
        }), user_args.end());
        user_args.push_back({func.params[i], arg_vals[i]});
    }

    double ret_val = 0.0;
    bool has_returned = false;
    int step_count = 0;
    int max_steps = 1000;
    
    auto lines = func.statements;
    size_t ip = 0;
    while (ip < lines.size() && step_count < max_steps) {
        if (has_returned) break;
        step_count++;
        std::string line = lines[ip];
        if (line.empty()) { ip++; continue; }

        if (line.rfind("return ", 0) == 0 || line == "return") {
            std::string ret_expr = (line.size() > 7) ? line.substr(7) : "0";
            ret_expr.erase(0, ret_expr.find_first_not_of(" \t"));
            ret_expr.erase(ret_expr.find_last_not_of(" \t") + 1);
            if (ret_expr.empty()) ret_expr = "0";
            ret_val = evaluate(ret_expr, err);
            has_returned = true;
            break;
        }

        if (line.find('=') != std::string::npos) {
            size_t eq = line.find('=');
            std::string lhs = line.substr(0, eq);
            std::string rhs = line.substr(eq + 1);
            lhs.erase(0, lhs.find_first_not_of(" \t"));
            lhs.erase(lhs.find_last_not_of(" \t") + 1);
            rhs.erase(0, rhs.find_first_not_of(" \t"));
            rhs.erase(rhs.find_last_not_of(" \t") + 1);
            
            bool isDef = false;
            evaluateInput(line, err, isDef);
            if (!err.empty()) break;
        } else {
            bool isDef = false;
            evaluateInput(line, err, isDef);
            if (!err.empty()) break;
        }
        ip++;
    }

    user_args = orig_args;
    user_arrays = orig_arrays;

    if (!err.empty()) return std::numeric_limits<double>::quiet_NaN();
    return ret_val;
}

inline std::string substituteUserFuncs(std::string expr, std::string& err) {
    if (!user_script_funcs.empty()) {
        for (const auto& sf : user_script_funcs) {
            std::string search = sf.name + "(";
            size_t pos = 0;
            while ((pos = expr.find(search, pos)) != std::string::npos) {
                bool prev_ok = (pos == 0 || (!std::isalnum(expr[pos - 1]) && expr[pos - 1] != '_'));
                if (!prev_ok) { pos += search.size(); continue; }
                
                size_t start = pos + search.size();
                int depth = 1;
                size_t end = start;
                while (end < expr.size() && depth > 0) {
                    if (expr[end] == '(') depth++;
                    else if (expr[end] == ')') depth--;
                    if (depth > 0) end++;
                }
                if (depth > 0) { err = "Mismatched Brackets"; return expr; }
                
                std::string args_str = expr.substr(start, end - start);
                std::vector<std::string> args;
                int arg_depth = 0;
                std::string current_arg = "";
                for (char c : args_str) {
                    if (c == '(') arg_depth++;
                    else if (c == ')') arg_depth--;
                    if (c == ',' && arg_depth == 0) {
                        args.push_back(current_arg);
                        current_arg = "";
                    } else {
                        current_arg += c;
                    }
                }
                if (!current_arg.empty() || args_str.empty()) args.push_back(current_arg);
                
                std::vector<double> arg_vals;
                for (auto& a_str : args) {
                    a_str.erase(0, a_str.find_first_not_of(" \t"));
                    a_str.erase(a_str.find_last_not_of(" \t") + 1);
                    if (a_str.empty()) continue;
                    std::string eval_err;
                    double val = evaluate(a_str, eval_err);
                    if (!eval_err.empty() || std::isnan(val)) {
                        err = eval_err.empty() ? "Arg Eval Error" : eval_err;
                        return expr;
                    }
                    arg_vals.push_back(val);
                }
                
                double func_res = executeScriptFunc(sf.name, arg_vals, err);
                if (!err.empty()) return expr;
                
                std::string res_str = fmtNum(func_res);
                expr.replace(pos, (end + 1) - pos, res_str);
                pos += res_str.size();
            }
        }
    }

    if (user_funcs.empty()) return expr;
    auto funcs = getParsedUserFuncs();
    bool changed = true;
    int limit = 10;
    while (changed && limit-- > 0) {
        changed = false;
        for (const auto& f : funcs) {
            std::string search = f.name + "(";
            size_t pos = 0;
            while ((pos = expr.find(search, pos)) != std::string::npos) {
                size_t start = pos + search.size();
                int depth = 1;
                size_t end = start;
                while (end < expr.size() && depth > 0) {
                    if (expr[end] == '(') depth++;
                    else if (expr[end] == ')') depth--;
                    if (depth > 0) end++;
                }
                if (depth > 0) {
                    err = "Mismatched Brackets";
                    return expr;
                }
                
                std::string args_str = expr.substr(start, end - start);
                std::vector<std::string> args;
                int arg_depth = 0;
                std::string current_arg = "";
                for (char c : args_str) {
                    if (c == '(') arg_depth++;
                    else if (c == ')') arg_depth--;
                    
                    if (c == ',' && arg_depth == 0) {
                        args.push_back(current_arg);
                        current_arg = "";
                    } else {
                        current_arg += c;
                    }
                }
                if (!current_arg.empty() || args_str.empty()) {
                    args.push_back(current_arg);
                }
                
                for (auto& arg : args) {
                    arg.erase(0, arg.find_first_not_of(" \t"));
                    arg.erase(arg.find_last_not_of(" \t") + 1);
                }
                
                if (args.size() != f.params.size()) {
                    err = "Arg Count Mismatch";
                    return expr;
                }
                
                std::string body_subbed = f.body;
                for (size_t i = 0; i < f.params.size(); ++i) {
                    std::string param = f.params[i];
                    std::string arg_val = "(" + args[i] + ")";
                    size_t p_pos = 0;
                    while ((p_pos = body_subbed.find(param, p_pos)) != std::string::npos) {
                        bool prev_ok = (p_pos == 0 || (!std::isalnum(body_subbed[p_pos - 1]) && body_subbed[p_pos - 1] != '_'));
                        bool next_ok = (p_pos + param.size() == body_subbed.size() || (!std::isalnum(body_subbed[p_pos + param.size()]) && body_subbed[p_pos + param.size()] != '_'));
                        if (prev_ok && next_ok) {
                            body_subbed.replace(p_pos, param.size(), arg_val);
                            p_pos += arg_val.size();
                        } else {
                            p_pos += param.size();
                        }
                    }
                }
                
                expr.replace(pos, (end + 1) - pos, "(" + body_subbed + ")");
                changed = true;
                break;
            }
        }
    }
    return expr;
}

inline std::string preprocessExpression(const std::string& s) {
    std::string s_clean = "";
    s_clean.reserve(s.size());
    for (char c : s) {
        if (c != ' ') {
            s_clean += c;
        }
    }
    std::string res = "";
    res.reserve(s_clean.size());
    int n = s_clean.size();
    for (int i = 0; i < n; ) {
        if (std::isdigit(s_clean[i]) || s_clean[i] == '.') {
            std::string num = "";
            while (i < n && (std::isdigit(s_clean[i]) || s_clean[i] == '.')) {
                num += s_clean[i++];
            }
            res += num;
            
            if (i < n) {
                if (i + 1 < n && s_clean[i] == 'd' && s_clean[i+1] == 'a') {
                    res += " * 1e1";
                    i += 2;
                    continue;
                }
                char c = s_clean[i];
                if (c == 'e' || c == 'E') {
                    int j = i + 1;
                    bool is_exp = false;
                    if (j < n && (s_clean[j] == '+' || s_clean[j] == '-')) j++;
                    if (j < n && std::isdigit(s_clean[j])) {
                        // Ensure it's not e1, e2 history variable (e.g. 5*e1 or 2+e1 is handled separately, but 5e1 vs e1 after num)
                        // If it's e followed by digits with no +/- sign and no more digits after, check if e is part of history variable
                        is_exp = true;
                    }
                    if (is_exp) {
                        res += " * 10^(";
                        if (s_clean[i+1] == '+' || s_clean[i+1] == '-') {
                            res += s_clean[i+1];
                            i += 2;
                        } else {
                            i++;
                        }
                        while (i < n && std::isdigit(s_clean[i])) {
                            res += s_clean[i++];
                        }
                        res += ")";
                        continue;
                    }
                }
                
                std::string mult = "";
                switch (c) {
                    case 'h': mult = "1e2"; break;
                    case 'k': mult = "1e3"; break;
                    case 'M': mult = "1e6"; break;
                    case 'G': mult = "1e9"; break;
                    case 'T': mult = "1e12"; break;
                    case 'P': mult = "1e15"; break;
                    case 'E': mult = "1e18"; break;
                    case 'Z': mult = "1e21"; break;
                    case 'Y': mult = "1e24"; break;
                    
                    case 'd': mult = "1e-1"; break;
                    case 'c': mult = "1e-2"; break;
                    case 'm': mult = "1e-3"; break;
                    case 'u': mult = "1e-6"; break;
                    case 'n': mult = "1e-9"; break;
                    case 'p': mult = "1e-12"; break;
                    case 'f': mult = "1e-15"; break;
                    case 'a': mult = "1e-18"; break;
                    case 'z': mult = "1e-21"; break;
                    case 'y': mult = "1e-24"; break;
                }
                if (!mult.empty()) {
                    int prev_pos = (int)res.size() - (int)num.size();
                    bool prev_is_ident = (prev_pos > 0 && (std::isalpha((unsigned char)res[prev_pos - 1]) || res[prev_pos - 1] == '_'));
                    bool is_ident = prev_is_ident || (i + 1 < n && (std::isalpha((unsigned char)s_clean[i+1]) || s_clean[i+1] == '_'));
                    if (!is_ident) {
                        i++;
                        std::string dec_part = "";
                        if (i < n && std::isdigit((unsigned char)s_clean[i])) {
                            while (i < n && std::isdigit((unsigned char)s_clean[i])) {
                                dec_part += s_clean[i++];
                            }
                        }
                        if (c == 'k' && dec_part.size() == 2) {
                            try {
                                double base_val = std::stod(num);
                                double dec_val = std::stod(dec_part);
                                double total = base_val * 1000.0 + dec_val;
                                // replace the appended num in res with total
                                res.erase(res.size() - num.size());
                                std::ostringstream oss;
                                oss << total;
                                res += oss.str();
                            } catch (...) {
                                res += "." + dec_part + " * " + mult;
                            }
                        } else {
                            if (!dec_part.empty()) {
                                res += "." + dec_part;
                            }
                            res += " * " + mult;
                        }
                        continue;
                    }
                }
            }
        } else {
            res += s_clean[i++];
        }
    }
    return res;
}

inline bool checkParentheses(const std::string& expr, std::string& err) {
    std::vector<char> stack;
    for (char c : expr) {
        if (c == '(' || c == '{' || c == '[') {
            stack.push_back(c);
        } else if (c == ')' || c == '}' || c == ']') {
            if (stack.empty()) {
                err = "Brackets Error";
                return false;
            }
            char open = stack.back();
            if ((c == ')' && open != '(') ||
                (c == '}' && open != '{') ||
                (c == ']' && open != '[')) {
                err = "Brackets Error";
                return false;
            }
            stack.pop_back();
        }
    }
    if (!stack.empty()) {
        err = "Brackets Error";
        return false;
    }
    return true;
}

inline std::string preprocessPercentage(const std::string& s) {
    if (s.find('%') == std::string::npos) return s;
    std::string res = s;
    while (true) {
        size_t pct_pos = std::string::npos;
        for (size_t i = 0; i < res.size(); ++i) {
            if (res[i] == '%') {
                size_t j = i + 1;
                while (j < res.size() && (res[j] == ' ' || res[j] == '\t')) j++;
                bool is_postfix = true;
                if (j < res.size()) {
                    char next_c = res[j];
                    if (std::isalnum(next_c) || next_c == '(' || next_c == '[' || next_c == '_' || next_c == '.') {
                        is_postfix = false;
                    }
                }
                if (is_postfix) {
                    pct_pos = i;
                    break;
                }
            }
        }
        
        if (pct_pos == std::string::npos) break;
        
        int depth = 0;
        size_t op_pos = std::string::npos;
        for (int i = (int)pct_pos - 1; i >= 0; --i) {
            if (res[i] == ')' || res[i] == ']' || res[i] == '}') depth++;
            else if (res[i] == '(' || res[i] == '[' || res[i] == '{') depth--;
            else if (depth == 0 && (res[i] == '+' || res[i] == '-')) {
                op_pos = i;
                break;
            }
        }
        
        if (op_pos != std::string::npos) {
            int lhs_start = 0;
            int lhs_depth = 0;
            for (int i = (int)op_pos - 1; i >= 0; --i) {
                if (res[i] == ')' || res[i] == ']' || res[i] == '}') lhs_depth++;
                else if (res[i] == '(' || res[i] == '[' || res[i] == '{') {
                    lhs_depth--;
                    if (lhs_depth < 0) {
                        lhs_start = i + 1;
                        break;
                    }
                }
            }
            
            std::string lhs = res.substr(lhs_start, op_pos - lhs_start);
            char op = res[op_pos];
            std::string rhs = res.substr(op_pos + 1, pct_pos - op_pos - 1);
            
            auto trim = [](std::string& str) {
                str.erase(0, str.find_first_not_of(" \t"));
                str.erase(str.find_last_not_of(" \t") + 1);
            };
            trim(lhs);
            trim(rhs);
            
            std::string replacement = "((" + lhs + ") " + op + " (" + lhs + ") * (" + rhs + ") * 0.01)";
            res = res.substr(0, lhs_start) + replacement + res.substr(pct_pos + 1);
        } else {
            int x_start = 0;
            int x_depth = 0;
            for (int i = (int)pct_pos - 1; i >= 0; --i) {
                if (res[i] == ')' || res[i] == ']' || res[i] == '}') x_depth++;
                else if (res[i] == '(' || res[i] == '[' || res[i] == '{') {
                    x_depth--;
                    if (x_depth < 0) {
                        x_start = i + 1;
                        break;
                    }
                }
            }
            std::string x_val = res.substr(x_start, pct_pos - x_start);
            res = res.substr(0, x_start) + "((" + x_val + ") * 0.01)" + res.substr(pct_pos + 1);
        }
    }
    return res;
}

inline std::string preprocessFactorial(const std::string& s) {
    if (s.find('!') == std::string::npos) return s;
    std::string res = "";
    res.reserve(s.size());
    int n = s.size();
    for (int i = 0; i < n; i++) {
        if (s[i] == '!') {
            bool is_postfix = false;
            if (i > 0) {
                char prev = s[i - 1];
                if (std::isalnum(prev) || prev == ')' || prev == ']' || prev == '}') {
                    is_postfix = true;
                }
            }
            if (is_postfix) {
                int j = i - 1;
                if (s[j] == ')' || s[j] == ']' || s[j] == '}') {
                    char close = s[j];
                    char open = (close == ')') ? '(' : ((close == ']') ? '[' : '{');
                    int depth = 1;
                    j--;
                    while (j >= 0 && depth > 0) {
                        if (s[j] == close) depth++;
                        else if (s[j] == open) depth--;
                        j--;
                    }
                    j++;
                } else {
                    while (j >= 0 && (std::isalnum(s[j]) || s[j] == '_')) {
                        j--;
                    }
                    j++;
                }
                std::string operand = s.substr(j, i - j);
                res = res.substr(0, j) + "fact(" + operand + ")";
            } else {
                res += s[i];
            }
        } else {
            res += s[i];
        }
    }
    return res;
}

inline std::string replaceLogicalWords(const std::string& s) {
    if (s.find("and") == std::string::npos &&
        s.find("or") == std::string::npos &&
        s.find("not") == std::string::npos &&
        s.find("xor") == std::string::npos) {
        return s;
    }
    std::string res = "";
    res.reserve(s.size());
    int n = s.size();
    for (int i = 0; i < n; ) {
        if (s[i] == '"' || s[i] == '\'') {
            char q = s[i];
            res += s[i++];
            while (i < n && s[i] != q) {
                res += s[i++];
            }
            if (i < n) res += s[i++];
            continue;
        }
        
        auto is_word_char = [](char c) {
            return std::isalnum(c) || c == '_';
        };
        
        bool boundary_left = (i == 0 || !is_word_char(s[i - 1]));
        
        if (boundary_left) {
            if (i + 3 <= n && s.substr(i, 3) == "and" && (i + 3 == n || !is_word_char(s[i + 3]))) {
                res += "&&";
                i += 3;
                continue;
            }
            if (i + 2 <= n && s.substr(i, 2) == "or" && (i + 2 == n || !is_word_char(s[i + 2]))) {
                res += "||";
                i += 2;
                continue;
            }
            if (i + 3 <= n && s.substr(i, 3) == "not" && (i + 3 == n || !is_word_char(s[i + 3]))) {
                res += "!";
                i += 3;
                continue;
            }
            if (i + 3 <= n && s.substr(i, 3) == "xor" && (i + 3 == n || !is_word_char(s[i + 3]))) {
                res += "!=";
                i += 3;
                continue;
            }
        }
        res += s[i++];
    }
    return res;
}

// Forward declaration: preprocessLen is defined below evaluateRelational
inline std::string preprocessLen(const std::string& s);

inline double evaluateRelational(const std::string& expr, std::string& err) {
    auto findOp = [&](const std::string& op) -> size_t {
        int p = 0, b = 0;
        bool q = false;
        if (expr.size() < op.size()) return std::string::npos;
        for (size_t i = 0; i <= expr.size() - op.size(); ++i) {
            char c = expr[i];
            if (c == '"' || c == '\'') {
                if (!q) { q = true; } else { q = false; }
            } else if (!q) {
                if (c == '(') p++;
                else if (c == ')') p--;
                else if (c == '[') b++;
                else if (c == ']') b--;
                else if (p == 0 && b == 0) {
                    if (expr.substr(i, op.size()) == op) {
                        if (op == "<" && i + 1 < expr.size() && expr[i+1] == '=') continue;
                        if (op == ">" && i + 1 < expr.size() && expr[i+1] == '=') continue;
                        if (op == "=" && i + 1 < expr.size() && expr[i+1] == '=') continue;
                        if (op == "=" && i > 0 && expr[i-1] == '=') continue;
                        return i;
                    }
                }
            }
        }
        return std::string::npos;
    };
    
    // Check ||
    size_t pos = findOp("||");
    if (pos == std::string::npos) pos = findOp(" or ");
    if (pos != std::string::npos) {
        std::string lhs = expr.substr(0, pos);
        std::string rhs = expr.substr(pos + (expr[pos] == '|' ? 2 : 4));
        std::string err_l, err_r;
        double lval = evaluate(preprocessArrayLookups(lhs, err_l), err_l);
        if (!err_l.empty()) { err = err_l; return 0.0; }
        double rval = evaluate(preprocessArrayLookups(rhs, err_r), err_r);
        if (!err_r.empty()) { err = err_r; return 0.0; }
        return (lval != 0.0 || rval != 0.0) ? 1.0 : 0.0;
    }
    
    // Check &&
    pos = findOp("&&");
    if (pos == std::string::npos) pos = findOp(" and ");
    if (pos != std::string::npos) {
        std::string lhs = expr.substr(0, pos);
        std::string rhs = expr.substr(pos + (expr[pos] == '&' ? 2 : 5));
        std::string err_l, err_r;
        double lval = evaluate(preprocessLen(preprocessArrayLookups(lhs, err_l)), err_l);
        if (!err_l.empty()) { err = err_l; return 0.0; }
        double rval = evaluate(preprocessLen(preprocessArrayLookups(rhs, err_r)), err_r);
        if (!err_r.empty()) { err = err_r; return 0.0; }
        return (lval != 0.0 && rval != 0.0) ? 1.0 : 0.0;
    }
    
    // Check ==
    pos = findOp("==");
    if (pos != std::string::npos) {
        std::string lhs = expr.substr(0, pos);
        std::string rhs = expr.substr(pos + 2);
        std::string err_l, err_r;
        double lval = evaluate(preprocessLen(preprocessArrayLookups(lhs, err_l)), err_l);
        if (!err_l.empty()) { err = err_l; return 0.0; }
        double rval = evaluate(preprocessLen(preprocessArrayLookups(rhs, err_r)), err_r);
        if (!err_r.empty()) { err = err_r; return 0.0; }
        return (lval == rval) ? 1.0 : 0.0;
    }
    
    // Check !=
    pos = findOp("!=");
    if (pos != std::string::npos) {
        std::string lhs = expr.substr(0, pos);
        std::string rhs = expr.substr(pos + 2);
        std::string err_l, err_r;
        double lval = evaluate(preprocessLen(preprocessArrayLookups(lhs, err_l)), err_l);
        if (!err_l.empty()) { err = err_l; return 0.0; }
        double rval = evaluate(preprocessLen(preprocessArrayLookups(rhs, err_r)), err_r);
        if (!err_r.empty()) { err = err_r; return 0.0; }
        return (lval != rval) ? 1.0 : 0.0;
    }
    
    // Check <=
    pos = findOp("<=");
    if (pos != std::string::npos) {
        std::string lhs = expr.substr(0, pos);
        std::string rhs = expr.substr(pos + 2);
        std::string err_l, err_r;
        double lval = evaluate(preprocessLen(preprocessArrayLookups(lhs, err_l)), err_l);
        if (!err_l.empty()) { err = err_l; return 0.0; }
        double rval = evaluate(preprocessLen(preprocessArrayLookups(rhs, err_r)), err_r);
        if (!err_r.empty()) { err = err_r; return 0.0; }
        return (lval <= rval) ? 1.0 : 0.0;
    }
    
    // Check >=
    pos = findOp(">=");
    if (pos != std::string::npos) {
        std::string lhs = expr.substr(0, pos);
        std::string rhs = expr.substr(pos + 2);
        std::string err_l, err_r;
        double lval = evaluate(preprocessLen(preprocessArrayLookups(lhs, err_l)), err_l);
        if (!err_l.empty()) { err = err_l; return 0.0; }
        double rval = evaluate(preprocessLen(preprocessArrayLookups(rhs, err_r)), err_r);
        if (!err_r.empty()) { err = err_r; return 0.0; }
        return (lval >= rval) ? 1.0 : 0.0;
    }
    
    // Check <
    pos = findOp("<");
    if (pos != std::string::npos) {
        std::string lhs = expr.substr(0, pos);
        std::string rhs = expr.substr(pos + 1);
        std::string err_l, err_r;
        double lval = evaluate(preprocessLen(preprocessArrayLookups(lhs, err_l)), err_l);
        if (!err_l.empty()) { err = err_l; return 0.0; }
        double rval = evaluate(preprocessLen(preprocessArrayLookups(rhs, err_r)), err_r);
        if (!err_r.empty()) { err = err_r; return 0.0; }
        return (lval < rval) ? 1.0 : 0.0;
    }
    
    // Check >
    pos = findOp(">");
    if (pos != std::string::npos) {
        std::string lhs = expr.substr(0, pos);
        std::string rhs = expr.substr(pos + 1);
        std::string err_l, err_r;
        double lval = evaluate(preprocessLen(preprocessArrayLookups(lhs, err_l)), err_l);
        if (!err_l.empty()) { err = err_l; return 0.0; }
        double rval = evaluate(preprocessLen(preprocessArrayLookups(rhs, err_r)), err_r);
        if (!err_r.empty()) { err = err_r; return 0.0; }
        return (lval > rval) ? 1.0 : 0.0;
    }
    
    return std::numeric_limits<double>::quiet_NaN();
}

inline std::string preprocessLen(const std::string& s) {
    if (s.find("len(") == std::string::npos) return s;
    std::string res = s;
    size_t pos = 0;
    while ((pos = res.find("len(", pos)) != std::string::npos) {
        bool prev_ok = (pos == 0 || (!std::isalnum(res[pos - 1]) && res[pos - 1] != '_'));
        if (!prev_ok) { pos += 4; continue; }
        size_t close = res.find(')', pos + 4);
        if (close == std::string::npos) break;
        std::string arg = res.substr(pos + 4, close - pos - 4);
        arg.erase(0, arg.find_first_not_of(" \t"));
        arg.erase(arg.find_last_not_of(" \t") + 1);
        
        int size_val = 0;
        bool found = false;
        bool is_scalar = false;
        // Check if arg is defined as a scalar variable in user_args
        for (const auto& ua : user_args) {
            if (ua.name == arg) {
                is_scalar = true;
                break;
            }
        }
        if (!is_scalar) {
            if (user_arrays.find(arg) != user_arrays.end()) {
                size_val = (int)user_arrays[arg].size();
                found = true;
            } else {
                bool is_arr = false;
                std::string parse_err;
                auto vec = parseArrayExpr(arg, is_arr, parse_err);
                if (is_arr && parse_err.empty()) {
                    size_val = (int)vec.size();
                    found = true;
                }
            }
        }
        if (found) {
            res.replace(pos, close - pos + 1, std::to_string(size_val));
            pos += std::to_string(size_val).size();
        } else {
            pos += 4;
        }
    }
    return res;
}

std::string preprocessVectorStats(const std::string& s) {
    static const std::vector<std::string> funcs = {"mean", "median", "mode", "var", "std"};
    std::string res = s;

    // 1. Process 2-argument vector function: dot(A, B)
    size_t pos = 0;
    while ((pos = res.find("dot(", pos)) != std::string::npos) {
        bool prev_ok = (pos == 0 || (!std::isalnum((unsigned char)res[pos - 1]) && res[pos - 1] != '_'));
        if (!prev_ok) { pos += 4; continue; }
        
        int depth = 1;
        int bracket_depth = 0;
        size_t close = pos + 4;
        size_t comma = std::string::npos;
        while (close < res.size() && depth > 0) {
            if (res[close] == '(') depth++;
            else if (res[close] == ')') depth--;
            else if (res[close] == '[') bracket_depth++;
            else if (res[close] == ']') bracket_depth--;
            else if (res[close] == ',' && depth == 1 && bracket_depth == 0) comma = close;
            if (depth > 0) close++;
        }
        if (depth > 0 || comma == std::string::npos) break;

        std::string arg1_str = res.substr(pos + 4, comma - pos - 4);
        std::string arg2_str = res.substr(comma + 1, close - comma - 1);
        
        bool is1_arr = false, is2_arr = false;
        std::string err1, err2;
        auto vec1 = parseArrayExpr(arg1_str, is1_arr, err1);
        auto vec2 = parseArrayExpr(arg2_str, is2_arr, err2);

        if (is1_arr && is2_arr && err1.empty() && err2.empty() && vec1.size() == vec2.size() && !vec1.empty()) {
            double dot_val = 0.0;
            for (size_t i = 0; i < vec1.size(); ++i) {
                dot_val += vec1[i] * vec2[i];
            }
            std::ostringstream oss;
            oss << dot_val;
            res.replace(pos, close - pos + 1, oss.str());
            pos += oss.str().size();
        } else {
            pos += 4;
        }
    }

    // 2. Process 1-argument vector stats functions (mean, median, mode, var, std)
    for (const auto& fname : funcs) {
        std::string target = fname + "(";
        size_t fpos = 0;
        while ((fpos = res.find(target, fpos)) != std::string::npos) {
            bool prev_ok = (fpos == 0 || (!std::isalnum((unsigned char)res[fpos - 1]) && res[fpos - 1] != '_'));
            if (!prev_ok) { fpos += target.size(); continue; }

            int depth = 1;
            int bracket_depth = 0;
            size_t close = fpos + target.size();
            while (close < res.size() && depth > 0) {
                if (res[close] == '(') depth++;
                else if (res[close] == ')') depth--;
                else if (res[close] == '[') bracket_depth++;
                else if (res[close] == ']') bracket_depth--;
                if (depth > 0) close++;
            }
            if (depth > 0) break;

            std::string arg = res.substr(fpos + target.size(), close - fpos - target.size());
            arg.erase(0, arg.find_first_not_of(" \t"));
            arg.erase(arg.find_last_not_of(" \t") + 1);

            bool is_arr = false;
            std::string parse_err;
            auto vec = parseArrayExpr(arg, is_arr, parse_err);

            if (is_arr && parse_err.empty() && !vec.empty()) {
                double computed_val = 0.0;
                if (fname == "mean") {
                    double sum = 0.0;
                    for (double v : vec) sum += v;
                    computed_val = sum / vec.size();
                } else if (fname == "median") {
                    std::vector<double> sorted = vec;
                    std::sort(sorted.begin(), sorted.end());
                    size_t sz = sorted.size();
                    if (sz % 2 == 0) computed_val = (sorted[sz/2 - 1] + sorted[sz/2]) / 2.0;
                    else computed_val = sorted[sz/2];
                } else if (fname == "mode") {
                    std::map<double, int> counts;
                    for (double v : vec) counts[v]++;
                    double bval = vec[0];
                    int maxc = 0;
                    for (const auto& kv : counts) {
                        if (kv.second > maxc) { maxc = kv.second; bval = kv.first; }
                    }
                    computed_val = bval;
                } else if (fname == "var") {
                    double sum = 0.0;
                    for (double v : vec) sum += v;
                    double m = sum / vec.size();
                    double vsum = 0.0;
                    for (double v : vec) vsum += (v - m) * (v - m);
                    computed_val = (vec.size() > 1) ? vsum / (vec.size() - 1) : 0.0;
                } else if (fname == "std") {
                    double sum = 0.0;
                    for (double v : vec) sum += v;
                    double m = sum / vec.size();
                    double vsum = 0.0;
                    for (double v : vec) vsum += (v - m) * (v - m);
                    double var_v = (vec.size() > 1) ? vsum / (vec.size() - 1) : 0.0;
                    computed_val = std::sqrt(var_v);
                }

                std::ostringstream oss;
                oss << computed_val;
                res.replace(fpos, close - fpos + 1, oss.str());
                fpos += oss.str().size();
            } else {
                fpos += target.size();
            }
        }
    }

    return res;
}



double evaluate(const std::string& expr_str, std::string& err) {
    err = "";
    std::string help_str;
    if (preprocessHelp(expr_str, help_str)) {
        err = help_str;
        return std::numeric_limits<double>::quiet_NaN();
    }
    
    if (!checkParentheses(expr_str, err)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    
    std::string subbed = substituteUserFuncs(expr_str, err);
    if (!err.empty()) return std::numeric_limits<double>::quiet_NaN();
    
    std::string preprocessed = preprocessExpression(subbed);
    preprocessed = preprocessPercentage(preprocessed);
    preprocessed = preprocessFactorial(preprocessed);
    preprocessed = replaceLogicalWords(preprocessed);
    preprocessed = preprocessLen(preprocessed);
    preprocessed = preprocessVectorStats(preprocessed);

    std::string rel_err;
    double rel_val = evaluateRelational(preprocessed, rel_err);
    if (rel_err.empty() && !std::isnan(rel_val)) {
        return rel_val;
    }
    if (!rel_err.empty()) {
        err = rel_err;
        return std::numeric_limits<double>::quiet_NaN();
    }
    
    te_parser tep;
    
    // 1. Register user defined variables
    for (size_t i = 0; i < user_args.size(); ++i) {
        tep.add_variable_or_function(te_variable{user_args[i].name, &user_args[i].val});
    }
    
    // 2. Register user defined constants
    for (size_t i = 0; i < user_consts.size(); ++i) {
        tep.add_variable_or_function(te_variable{user_consts[i].name, &user_consts[i].val});
    }
    
    // 3. Register history variables (e1, e2, e3...)
    for (size_t i = 0; i < history.size(); ++i) {
        tep.add_variable_or_function(te_variable{"e" + std::to_string(i + 1), &history[i]});
    }
    
    // 4. Built-in functions
    tep.add_variable_or_function(te_variable{"sin", custom_sin});
    tep.add_variable_or_function(te_variable{"cos", custom_cos});
    tep.add_variable_or_function(te_variable{"tan", custom_tan});
    tep.add_variable_or_function(te_variable{"asin", custom_asin});
    tep.add_variable_or_function(te_variable{"acos", custom_acos});
    tep.add_variable_or_function(te_variable{"atan", custom_atan});
    tep.add_variable_or_function(te_variable{"ctan", custom_ctan});
    
    tep.add_variable_or_function(te_variable{"deg2rad", custom_deg2rad});
    tep.add_variable_or_function(te_variable{"d2r", custom_deg2rad});
    tep.add_variable_or_function(te_variable{"rad2deg", custom_rad2deg});
    tep.add_variable_or_function(te_variable{"r2d", custom_rad2deg});
    
    tep.add_variable_or_function(te_variable{"log", custom_log10});
    tep.add_variable_or_function(te_variable{"log2", custom_log2});
    tep.add_variable_or_function(te_variable{"logb", custom_logb});
    tep.add_variable_or_function(te_variable{"mod", custom_mod});
    tep.add_variable_or_function(te_variable{"sgn", custom_sgn});
    tep.add_variable_or_function(te_variable{"trunc", custom_trunc});
    tep.add_variable_or_function(te_variable{"fact", custom_fact});
    tep.add_variable_or_function(te_variable{"C", custom_C});
    tep.add_variable_or_function(te_variable{"P", custom_P});
    tep.add_variable_or_function(te_variable{"gcd", custom_gcd});
    tep.add_variable_or_function(te_variable{"lcm", custom_lcm});
    tep.add_variable_or_function(te_variable{"fib", custom_fib});
    tep.add_variable_or_function(te_variable{"rUni", custom_rUni});
    tep.add_variable_or_function(te_variable{"rNor", custom_rNor});
    tep.add_variable_or_function(te_variable{"abs", custom_abs});
    tep.add_variable_or_function(te_variable{"dot", custom_dot});
    tep.add_variable_or_function(te_variable{"print", [](double x) { return x; }});
    
    tep.add_variable_or_function(te_variable{"mean", variadic_mean, static_cast<te_variable_flags>(TE_PURE | TE_VARIADIC)});
    tep.add_variable_or_function(te_variable{"median", variadic_median, static_cast<te_variable_flags>(TE_PURE | TE_VARIADIC)});
    tep.add_variable_or_function(te_variable{"mode", variadic_mode, static_cast<te_variable_flags>(TE_PURE | TE_VARIADIC)});
    tep.add_variable_or_function(te_variable{"var", variadic_var, static_cast<te_variable_flags>(TE_PURE | TE_VARIADIC)});
    tep.add_variable_or_function(te_variable{"std", variadic_std, static_cast<te_variable_flags>(TE_PURE | TE_VARIADIC)});
    
    double val = tep.evaluate(preprocessed);
    
    if (!tep.success()) {
        std::string err_msg = tep.get_last_error_message();
        if (err_msg.find("bracket") != std::string::npos || err_msg.find("parenthes") != std::string::npos) {
            err = "Mismatched Brackets";
        } else if (err_msg.find("unknown") != std::string::npos || err_msg.find("token") != std::string::npos || err_msg.find("symbol") != std::string::npos) {
            err = "Unknown Token";
        } else {
            err = "Math Error";
        }
        return std::numeric_limits<double>::quiet_NaN();
    }
    
    if (std::isnan(val) || std::isinf(val)) {
        err = "Math Error";
    }
    
    return val;
}

double evaluateInput(const std::string& expr_str, std::string& err, bool& isDefinition) {
    isDefinition = false;
    
    size_t eq_pos = expr_str.find('=');
    if (eq_pos != std::string::npos && 
        expr_str.find("==") == std::string::npos && 
        expr_str.find(">=") == std::string::npos && 
        expr_str.find("<=") == std::string::npos && 
        expr_str.find("!=") == std::string::npos) {
        
        std::string lhs = expr_str.substr(0, eq_pos);
        std::string rhs = expr_str.substr(eq_pos + 1);
        
        lhs.erase(0, lhs.find_first_not_of(" \t"));
        lhs.erase(lhs.find_last_not_of(" \t") + 1);
        rhs.erase(0, rhs.find_first_not_of(" \t"));
        rhs.erase(rhs.find_last_not_of(" \t") + 1);
        
        bool isConstDef = false;
        if (lhs.rfind("const ", 0) == 0 || lhs.rfind("const\t", 0) == 0) {
            isConstDef = true;
            lhs = lhs.substr(5);
            lhs.erase(0, lhs.find_first_not_of(" \t"));
        }

        if (lhs.empty()) {
            err = "Invalid Name";
            return std::numeric_limits<double>::quiet_NaN();
        }
        if (lhs.find('(') == std::string::npos) {
            if (!std::isalpha(lhs[0]) && lhs[0] != '_') {
                err = "Invalid Name";
                return std::numeric_limits<double>::quiet_NaN();
            }
            for (char c : lhs) {
                if (!std::isalnum(c) && c != '_') {
                    err = "Invalid Name";
                    return std::numeric_limits<double>::quiet_NaN();
                }
            }
        } else {
            size_t paren = lhs.find('(');
            std::string func_name = lhs.substr(0, paren);
            func_name.erase(0, func_name.find_first_not_of(" \t"));
            func_name.erase(func_name.find_last_not_of(" \t") + 1);
            if (func_name.empty() || (!std::isalpha(func_name[0]) && func_name[0] != '_')) {
                err = "Invalid Name";
                return std::numeric_limits<double>::quiet_NaN();
            }
            for (char c : func_name) {
                if (!std::isalnum(c) && c != '_') {
                    err = "Invalid Name";
                    return std::numeric_limits<double>::quiet_NaN();
                }
            }
        }
        
        if (!isConstDef) {
            for (const auto& cn : user_consts) {
                if (cn.name == lhs) {
                    err = "Const Error";
                    return std::numeric_limits<double>::quiet_NaN();
                }
            }
        }
        
        isDefinition = true;
        
        if (lhs.find('(') != std::string::npos && lhs.find(')') != std::string::npos) {
            user_funcs.push_back(expr_str);
            std::string test_err = "";
            substituteUserFuncs(lhs, test_err);
            if (!test_err.empty()) {
                user_funcs.pop_back();
                err = "Func Def Error";
                return std::numeric_limits<double>::quiet_NaN();
            }
            
            size_t paren = lhs.find('(');
            std::string fname = lhs.substr(0, paren + 1);
            if (std::find(autocomplete_words.begin(), autocomplete_words.end(), fname) == autocomplete_words.end()) {
                autocomplete_words.push_back(fname);
            }
            return 1.0;
        } else if (isConstDef) {
            double val = evaluate(rhs, err);
            if (err.empty() && !std::isnan(val)) {
                user_args.erase(std::remove_if(user_args.begin(), user_args.end(), [&](const UserArg& a) {
                    return a.name == lhs;
                }), user_args.end());

                bool updated = false;
                for (auto& cn : user_consts) {
                    if (cn.name == lhs) {
                        cn.val = val;
                        updated = true;
                        break;
                    }
                }
                if (!updated) {
                    user_consts.push_back({lhs, val});
                }
                
                if (std::find(autocomplete_words.begin(), autocomplete_words.end(), lhs) == autocomplete_words.end()) {
                    autocomplete_words.push_back(lhs);
                }
                return val;
            } else {
                err = "Const Def Error";
                return std::numeric_limits<double>::quiet_NaN();
            }
        } else {
            std::string op1, op, op2;
            if (parseBinaryOp(rhs, op1, op, op2)) {
                auto trim = [](std::string& s) {
                    s.erase(0, s.find_first_not_of(" \t"));
                    s.erase(s.find_last_not_of(" \t") + 1);
                };
                trim(op1); trim(op2);
                bool is1_arr = false, is2_arr = false;
                std::string dummy_err;
                std::vector<double> a1 = parseArrayExpr(op1, is1_arr, dummy_err);
                std::vector<double> a2 = parseArrayExpr(op2, is2_arr, dummy_err);
                if (op == ".*" || op == "./" || (!a1.empty() && is1_arr) || (!a2.empty() && is2_arr) || (user_arrays.find(op1) != user_arrays.end() && !user_arrays[op1].empty()) || (user_arrays.find(op2) != user_arrays.end() && !user_arrays[op2].empty())) {
                    bool is_scalar = false;
                    double scalar_res = 0;
                    std::vector<double> res = evaluateArrayBinaryOp(op1, op, op2, is_scalar, scalar_res, err);
                    if (err.empty()) {
                        if (is_scalar) {
                            user_args.erase(std::remove_if(user_args.begin(), user_args.end(), [&](const UserArg& a) {
                                return a.name == lhs;
                            }), user_args.end());
                            user_args.push_back({lhs, scalar_res});
                            if (std::find(autocomplete_words.begin(), autocomplete_words.end(), lhs) == autocomplete_words.end()) {
                                autocomplete_words.push_back(lhs);
                            }
                            return scalar_res;
                        } else {
                            user_arrays[lhs] = res;
                            if (std::find(autocomplete_words.begin(), autocomplete_words.end(), lhs) == autocomplete_words.end()) {
                                autocomplete_words.push_back(lhs);
                            }
                            return 1.0;
                        }
                    }
                    return std::numeric_limits<double>::quiet_NaN();
                }
            }

            bool is_arr = false;
            std::vector<double> arr = parseArrayExpr(rhs, is_arr, err);
            if (err.empty() && is_arr) {
                user_arrays[lhs] = arr;
                user_args.erase(std::remove_if(user_args.begin(), user_args.end(), [&](const UserArg& a) {
                    return a.name == lhs;
                }), user_args.end());
                if (std::find(autocomplete_words.begin(), autocomplete_words.end(), lhs) == autocomplete_words.end()) {
                    autocomplete_words.push_back(lhs);
                }
                return 1.0;
            }
            err = "";
            double val = evaluate(rhs, err);
            if (err.empty() && !std::isnan(val)) {
                bool found = false;
                for (auto& arg : user_args) {
                    if (arg.name == lhs) {
                        arg.val = val;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    user_args.push_back({lhs, val});
                }
                
                if (std::find(autocomplete_words.begin(), autocomplete_words.end(), lhs) == autocomplete_words.end()) {
                    autocomplete_words.push_back(lhs);
                }
                return val;
            } else {
                err = "Var Def Error";
                return std::numeric_limits<double>::quiet_NaN();
            }
        }
    }
    
    return evaluate(expr_str, err);
}

void handleTabCompletion(std::string& expression, int& cursor_pos) {
    static std::string orig_prefix = "";
    static size_t comp_start = 0;
    static int match_idx = -1;
    static std::string last_inserted = "";

    bool is_continuation = false;
    if (!orig_prefix.empty() && !last_inserted.empty() && comp_start <= expression.size()) {
        if (expression.substr(comp_start, last_inserted.size()) == last_inserted) {
            is_continuation = true;
        }
    }

    if (!is_continuation) {
        if (cursor_pos < 0 || cursor_pos > (int)expression.size()) cursor_pos = expression.size();
        size_t start = cursor_pos;
        while (start > 0 && (std::isalnum(expression[start - 1]) || expression[start - 1] == '_')) {
            start--;
        }
        std::string prefix = expression.substr(start, cursor_pos - start);
        if (prefix.empty()) return;

        orig_prefix = prefix;
        comp_start = start;
        match_idx = 0;
    } else {
        match_idx++;
    }

    std::vector<std::string> matches;
    matches.reserve(16);

    auto check_candidate = [&](const std::string& word) {
        if (word.rfind(orig_prefix, 0) == 0) {
            if (std::find(matches.begin(), matches.end(), word) == matches.end()) {
                matches.push_back(word);
            }
        }
    };

    for (const auto& w : autocomplete_words) check_candidate(w);
    for (size_t i = 0; i < history.size(); ++i) check_candidate("e" + std::to_string(i + 1));
    for (const auto& arg : user_args) check_candidate(arg.name);
    for (const auto& func : user_funcs) {
        size_t paren = func.find('(');
        if (paren != std::string::npos) {
            std::string fname = func.substr(0, paren + 1);
            if (!fname.empty()) check_candidate(fname);
        }
    }
    for (const auto& cn : user_consts) check_candidate(cn.name);

    if (matches.empty()) {
        orig_prefix = "";
        last_inserted = "";
        return;
    }

    match_idx = match_idx % matches.size();
    std::string matched_word = matches[match_idx];

    std::string new_inserted;
    if (auto_brackets && !matched_word.empty() && matched_word.back() == '(') {
        new_inserted = matched_word + ")";
        cursor_pos = comp_start + matched_word.size();
    } else {
        new_inserted = matched_word;
        cursor_pos = comp_start + new_inserted.size();
    }

    if (is_continuation) {
        expression.replace(comp_start, last_inserted.size(), new_inserted);
    } else {
        expression.replace(comp_start, orig_prefix.size(), new_inserted);
    }
    last_inserted = new_inserted;
}

std::string preprocessArrayLookups(const std::string& expr, std::string& err) {
    std::string res = expr;
    bool replaced_any = true;
    while (replaced_any) {
        replaced_any = false;
        size_t close_bracket = std::string::npos;
        size_t open_bracket = std::string::npos;
        size_t id_start = std::string::npos;
        std::string array_name = "";
        
        for (size_t i = 0; i < res.size(); ++i) {
            if (res[i] == '[') {
                open_bracket = i;
            } else if (res[i] == ']' && open_bracket != std::string::npos) {
                close_bracket = i;
                size_t temp_start = open_bracket;
                while (temp_start > 0 && (std::isalnum(res[temp_start - 1]) || res[temp_start - 1] == '_')) {
                    temp_start--;
                }
                if (temp_start < open_bracket) {
                    id_start = temp_start;
                    array_name = res.substr(id_start, open_bracket - id_start);
                    break;
                }
                open_bracket = std::string::npos;
            }
        }
        
        if (id_start != std::string::npos && open_bracket != std::string::npos && close_bracket != std::string::npos) {
            std::string index_expr = res.substr(open_bracket + 1, close_bracket - open_bracket - 1);
            if (user_arrays.find(array_name) == user_arrays.end()) {
                err = "Array " + array_name + " not defined";
                return expr;
            }
            double idx_val = evaluate(index_expr, err);
            if (!err.empty()) {
                return expr;
            }
            int idx = (int)std::round(idx_val);
            const auto& arr = user_arrays[array_name];
            if (idx < 1 || idx > (int)arr.size()) {
                err = "Index " + std::to_string(idx) + " out of bounds for array " + array_name + " (size " + std::to_string(arr.size()) + ")";
                return expr;
            }
            double element_val = arr[idx - 1];
            res.replace(id_start, (close_bracket - id_start + 1), fmtNum(element_val));
            replaced_any = true;
        }
    }
    return res;
}

std::vector<double> parseArrayExpr(const std::string& rhs, bool& is_array, std::string& err) {
    is_array = false;
    std::string trimmed = rhs;
    trimmed.erase(0, trimmed.find_first_not_of(" \t"));
    trimmed.erase(trimmed.find_last_not_of(" \t") + 1);
    
    if (trimmed.empty()) return {};
    
    bool is_scalar_var = false;
    for (const auto& ua : user_args) {
        if (ua.name == trimmed) { is_scalar_var = true; break; }
    }
    
    if (!is_scalar_var && user_arrays.find(trimmed) != user_arrays.end()) {
        is_array = true;
        return user_arrays[trimmed];
    }

    // Binary operations on arrays: e.g. 3 * t or px .* px or 5 * sin(3*t)
    std::string b_op1, b_op, b_op2;
    if (parseBinaryOp(trimmed, b_op1, b_op, b_op2)) {
        bool is_scalar = false;
        double scalar_res = 0;
        std::string bin_err;
        std::vector<double> res = evaluateArrayBinaryOp(b_op1, b_op, b_op2, is_scalar, scalar_res, bin_err);
        if (bin_err.empty() && (!res.empty() || is_scalar)) {
            if (is_scalar) {
                return {scalar_res};
            } else {
                is_array = true;
                return res;
            }
        }
    }
    
    // Parentheses unwrapping: (expr)
    if (trimmed.front() == '(' && trimmed.back() == ')') {
        int depth = 0;
        bool matching = true;
        for (size_t i = 0; i < trimmed.size() - 1; ++i) {
            if (trimmed[i] == '(') depth++;
            else if (trimmed[i] == ')') depth--;
            if (depth == 0) { matching = false; break; }
        }
        if (matching) {
            std::string inner = trimmed.substr(1, trimmed.size() - 2);
            return parseArrayExpr(inner, is_array, err);
        }
    }

    // Unary functions on arrays: func(arg)
    size_t open_p = trimmed.find('(');
    if (open_p != std::string::npos && open_p > 0 && trimmed.back() == ')') {
        int depth = 0;
        bool outer = true;
        for (size_t i = open_p; i < trimmed.size() - 1; ++i) {
            if (trimmed[i] == '(') depth++;
            else if (trimmed[i] == ')') depth--;
            if (depth == 0) { outer = false; break; }
        }
        if (outer) {
            std::string func_name = trimmed.substr(0, open_p);
            func_name.erase(0, func_name.find_first_not_of(" \t"));
            func_name.erase(func_name.find_last_not_of(" \t") + 1);
            std::string arg_str = trimmed.substr(open_p + 1, trimmed.size() - open_p - 2);
            
            bool is_arg_arr = false;
            std::string sub_err;
            std::vector<double> arg_vec = parseArrayExpr(arg_str, is_arg_arr, sub_err);
            if (is_arg_arr && sub_err.empty()) {
                is_array = true;
                std::vector<double> res(arg_vec.size());
                for (size_t i = 0; i < arg_vec.size(); ++i) {
                    double val = arg_vec[i];
                    if (func_name == "cos") res[i] = custom_cos(val);
                    else if (func_name == "sin") res[i] = custom_sin(val);
                    else if (func_name == "tan") res[i] = custom_tan(val);
                    else if (func_name == "sqrt") res[i] = sqrt(val);
                    else if (func_name == "cbrt") res[i] = cbrt(val);
                    else if (func_name == "abs") res[i] = fabs(val);
                    else if (func_name == "exp") res[i] = exp(val);
                    else if (func_name == "ln" || func_name == "log") res[i] = log(val);
                    else if (func_name == "log10") res[i] = log10(val);
                    else if (func_name == "deg2rad" || func_name == "d2r") res[i] = custom_deg2rad(val);
                    else if (func_name == "rad2deg" || func_name == "r2d") res[i] = custom_rad2deg(val);
                    else if (func_name == "floor") res[i] = floor(val);
                    else if (func_name == "ceil") res[i] = ceil(val);
                    else if (func_name == "round") res[i] = round(val);
                    else res[i] = val;
                }
                return res;
            }
        }
    }
    
    if ((trimmed.front() == '[' && trimmed.back() == ']') || (trimmed.front() == '(' && trimmed.back() == ')' && trimmed.find(',') != std::string::npos)) {
        is_array = true;
        std::string inner = trimmed.substr(1, trimmed.size() - 2);
        std::vector<double> res;
        bool in_inner_quotes = false;
        std::string current_val_str = "";
        
        for (size_t i = 0; i <= inner.size(); ++i) {
            char c = (i < inner.size()) ? inner[i] : ',';
            if (c == '"') {
                in_inner_quotes = !in_inner_quotes;
            }
            if (c == ',' && !in_inner_quotes) {
                current_val_str.erase(0, current_val_str.find_first_not_of(" \t"));
                current_val_str.erase(current_val_str.find_last_not_of(" \t") + 1);
                if (!current_val_str.empty()) {
                    std::string prep_err;
                    std::string prep = preprocessArrayLookups(current_val_str, prep_err);
                    if (!prep_err.empty()) {
                        err = prep_err;
                        return {};
                    }
                    double val = evaluate(prep, prep_err);
                    if (!prep_err.empty()) {
                        err = prep_err;
                        return {};
                    }
                    res.push_back(val);
                }
                current_val_str = "";
            } else {
                current_val_str += c;
            }
        }
        return res;
    }
    
    if (trimmed.find(':') != std::string::npos) {
        // Find colons that are range separators, not negative signs or other constructs
        size_t first_col = std::string::npos;
        size_t second_col = std::string::npos;
        
        // Scan for colons outside quotes/parens
        int pdepth = 0;
        for (size_t i = 0; i < trimmed.size(); ++i) {
            char c = trimmed[i];
            if (c == '(' || c == '[') pdepth++;
            else if (c == ')' || c == ']') { if (pdepth > 0) pdepth--; }
            else if (c == ':' && pdepth == 0) {
                if (first_col == std::string::npos) first_col = i;
                else if (second_col == std::string::npos) { second_col = i; break; }
            }
        }
        
        if (first_col != std::string::npos) {
            is_array = true;
            std::string start_str = trimmed.substr(0, first_col);
            std::string step_str = "1";
            std::string end_str = "";
            if (second_col != std::string::npos) {
                step_str = trimmed.substr(first_col + 1, second_col - first_col - 1);
                end_str = trimmed.substr(second_col + 1);
            } else {
                end_str = trimmed.substr(first_col + 1);
            }
        
        std::string prep_err;
        std::string s_start = preprocessArrayLookups(start_str, prep_err);
        double start_val = evaluate(s_start, prep_err);
        if (!prep_err.empty()) { err = prep_err; return {}; }
        prep_err.clear();
        std::string s_step = preprocessArrayLookups(step_str, prep_err);
        double step_val = evaluate(s_step, prep_err);
        if (!prep_err.empty()) { err = prep_err; return {}; }
        prep_err.clear();
        std::string s_end = preprocessArrayLookups(end_str, prep_err);
        double end_val = evaluate(s_end, prep_err);
        if (!prep_err.empty()) { err = prep_err; return {}; }
        
        std::vector<double> res;
        double eps = 1e-9;
        if (step_val > 0) {
            for (double v = start_val; v <= end_val + eps; v += step_val) {
                res.push_back(v);
            }
        } else if (step_val < 0) {
            for (double v = start_val; v >= end_val - eps; v += step_val) {
                res.push_back(v);
            }
        }
        return res;
        }
    }
    
    if (user_arrays.find(trimmed) != user_arrays.end()) {
        is_array = true;
        return user_arrays[trimmed];
    }
    
    return {};
}

std::vector<double> evaluateArrayBinaryOp(const std::string& op1_str, const std::string& op_str, const std::string& op2_str, bool& is_scalar_result, double& scalar_val, std::string& err) {
    is_scalar_result = false;
    scalar_val = 0;
    
    bool is_op1_arr = false;
    bool is_op2_arr = false;
    std::vector<double> op1_arr = parseArrayExpr(op1_str, is_op1_arr, err);
    if (!err.empty()) return {};
    std::vector<double> op2_arr = parseArrayExpr(op2_str, is_op2_arr, err);
    if (!err.empty()) return {};
    
    double op1_scalar = 0;
    double op2_scalar = 0;
    if (!is_op1_arr) {
        op1_scalar = evaluate(preprocessArrayLookups(op1_str, err), err);
        if (!err.empty()) return {};
    }
    if (!is_op2_arr) {
        op2_scalar = evaluate(preprocessArrayLookups(op2_str, err), err);
        if (!err.empty()) return {};
    }
    
    if (is_op1_arr && is_op2_arr) {
        if (op1_arr.size() != op2_arr.size()) {
            err = "Array size mismatch (" + std::to_string(op1_arr.size()) + " vs " + std::to_string(op2_arr.size()) + ")";
            return {};
        }
        
        if (op_str == "*") {
            is_scalar_result = true;
            double sum = 0;
            for (size_t i = 0; i < op1_arr.size(); ++i) {
                sum += op1_arr[i] * op2_arr[i];
            }
            scalar_val = sum;
            return {};
        }
        
        std::vector<double> res(op1_arr.size());
        for (size_t i = 0; i < op1_arr.size(); ++i) {
            if (op_str == ".*") {
                res[i] = op1_arr[i] * op2_arr[i];
            } else if (op_str == "./") {
                if (op2_arr[i] == 0.0) {
                    err = "Division by zero";
                    return {};
                }
                res[i] = op1_arr[i] / op2_arr[i];
            } else if (op_str == "+") {
                res[i] = op1_arr[i] + op2_arr[i];
            } else if (op_str == "-") {
                res[i] = op1_arr[i] - op2_arr[i];
            } else {
                err = "Invalid operator: " + op_str;
                return {};
            }
        }
        return res;
    }
    else if (is_op1_arr && !is_op2_arr) {
        std::vector<double> res(op1_arr.size());
        for (size_t i = 0; i < op1_arr.size(); ++i) {
            if (op_str == "*" || op_str == ".*") {
                res[i] = op1_arr[i] * op2_scalar;
            } else if (op_str == "/" || op_str == "./") {
                if (op2_scalar == 0.0) {
                    err = "Division by zero";
                    return {};
                }
                res[i] = op1_arr[i] / op2_scalar;
            } else if (op_str == "+") {
                res[i] = op1_arr[i] + op2_scalar;
            } else if (op_str == "-") {
                res[i] = op1_arr[i] - op2_scalar;
            } else {
                err = "Invalid operator: " + op_str;
                return {};
            }
        }
        return res;
    }
    else if (!is_op1_arr && is_op2_arr) {
        std::vector<double> res(op2_arr.size());
        for (size_t i = 0; i < op2_arr.size(); ++i) {
            if (op_str == "*" || op_str == ".*") {
                res[i] = op1_scalar * op2_arr[i];
            } else if (op_str == "+") {
                res[i] = op1_scalar + op2_arr[i];
            } else if (op_str == "-") {
                res[i] = op1_scalar - op2_arr[i];
            } else {
                err = "Invalid operator: " + op_str;
                return {};
            }
        }
        return res;
    }
    
    err = "No arrays in array op";
    return {};
}

std::string formatPrintString(const std::string& str, std::string& err) {
    if (str.size() >= 2 && str.front() == '"' && str.back() == '"') {
        std::string inner = str.substr(1, str.size() - 2);
        std::string res = "";
        size_t i = 0;
        while (i < inner.size()) {
            if (inner[i] == '{') {
                size_t close = inner.find('}', i);
                if (close == std::string::npos) {
                    err = "Mismatched braces in print";
                    return str;
                }
                std::string expr = inner.substr(i + 1, close - i - 1);
                expr.erase(0, expr.find_first_not_of(" \t"));
                expr.erase(expr.find_last_not_of(" \t") + 1);

                bool is_scalar = false;
                for (const auto& ua : user_args) {
                    if (ua.name == expr) { is_scalar = true; break; }
                }

                if (!is_scalar && user_arrays.find(expr) != user_arrays.end()) {
                    const auto& arr = user_arrays[expr];
                    std::string arr_str = "[";
                    for (size_t k = 0; k < arr.size(); ++k) {
                        arr_str += fmtNum(arr[k]);
                        if (k + 1 < arr.size()) arr_str += ", ";
                    }
                    arr_str += "]";
                    res += arr_str;
                } else {
                    std::string eval_err;
                    std::string prep_expr = preprocessLen(preprocessArrayLookups(expr, eval_err));
                    if (!eval_err.empty()) return str;
                    
                    double val = evaluate(prep_expr, eval_err);
                    if (!eval_err.empty()) return str;
                    
                    res += fmtNum(val);
                }
                i = close + 1;
            } else {
                res += inner[i++];
            }
        }
        return res;
    } else {
        std::string trimmed_str = str;
        trimmed_str.erase(0, trimmed_str.find_first_not_of(" \t"));
        trimmed_str.erase(trimmed_str.find_last_not_of(" \t") + 1);

        bool is_scalar = false;
        for (const auto& ua : user_args) {
            if (ua.name == trimmed_str) { is_scalar = true; break; }
        }

        if (!is_scalar && user_arrays.find(trimmed_str) != user_arrays.end()) {
            const auto& arr = user_arrays[trimmed_str];
            std::string arr_str = "[";
            for (size_t k = 0; k < arr.size(); ++k) {
                arr_str += fmtNum(arr[k]);
                if (k + 1 < arr.size()) arr_str += ", ";
            }
            arr_str += "]";
            return arr_str;
        }

        std::string expr = preprocessArrayLookups(str, err);
        if (!err.empty()) return str;
        double val = evaluate(expr, err);
        if (!err.empty()) return str;
        return fmtNum(val);
    }
}

bool parseArrayElementAssignment(const std::string& line, std::string& name, std::string& idx_expr, std::string& val_expr) {
    size_t open = line.find('[');
    if (open == std::string::npos || open == 0) return false;
    
    for (size_t i = 0; i < open; ++i) {
        if (!std::isalnum(line[i]) && line[i] != '_') return false;
    }
    
    size_t close = line.find(']', open);
    if (close == std::string::npos) return false;
    
    size_t eq = line.find('=', close);
    if (eq == std::string::npos) return false;
    
    for (size_t i = close + 1; i < eq; ++i) {
        if (line[i] != ' ' && line[i] != '\t') return false;
    }
    
    name = line.substr(0, open);
    idx_expr = line.substr(open + 1, close - open - 1);
    val_expr = line.substr(eq + 1);
    return true;
}

bool parseBinaryOp(const std::string& rhs, std::string& op1, std::string& op, std::string& op2) {
    std::string clean_rhs = rhs;
    clean_rhs.erase(0, clean_rhs.find_first_not_of(" \t"));
    clean_rhs.erase(clean_rhs.find_last_not_of(" \t") + 1);

    while (clean_rhs.size() >= 2 && clean_rhs.front() == '(' && clean_rhs.back() == ')') {
        int nesting = 0;
        bool valid = true;
        for (size_t i = 0; i < clean_rhs.size() - 1; ++i) {
            if (clean_rhs[i] == '(') nesting++;
            else if (clean_rhs[i] == ')') nesting--;
            if (nesting == 0) { valid = false; break; }
        }
        if (valid) {
            clean_rhs = clean_rhs.substr(1, clean_rhs.size() - 2);
            clean_rhs.erase(0, clean_rhs.find_first_not_of(" \t"));
            clean_rhs.erase(clean_rhs.find_last_not_of(" \t") + 1);
        } else {
            break;
        }
    }

    int bracket_nesting = 0;
    int paren_nesting = 0;
    
    // Pass 1: Look for lowest-precedence binary operators (+ and -) outside brackets/parens
    for (int i = (int)clean_rhs.size() - 1; i >= 0; --i) {
        char c = clean_rhs[i];
        if (c == ']') bracket_nesting++;
        else if (c == '[') bracket_nesting--;
        else if (c == ')') paren_nesting++;
        else if (c == '(') paren_nesting--;
        else if (bracket_nesting == 0 && paren_nesting == 0) {
            if (c == '+' || c == '-') {
                if (i > 0 && clean_rhs[i-1] == '.') continue;
                std::string left = clean_rhs.substr(0, i);
                left.erase(0, left.find_first_not_of(" \t"));
                if (left.empty()) continue; // Unary
                
                op1 = clean_rhs.substr(0, i);
                op = std::string(1, c);
                op2 = clean_rhs.substr(i + 1);
                return true;
            }
        }
    }
    
    // Pass 2: Look for higher-precedence binary operators (*, /, .*, ./)
    bracket_nesting = 0;
    paren_nesting = 0;
    for (int i = (int)clean_rhs.size() - 1; i >= 0; --i) {
        char c = clean_rhs[i];
        if (c == ']') bracket_nesting++;
        else if (c == '[') bracket_nesting--;
        else if (c == ')') paren_nesting++;
        else if (c == '(') paren_nesting--;
        else if (bracket_nesting == 0 && paren_nesting == 0) {
            if (i + 1 < (int)clean_rhs.size()) {
                std::string op2c = clean_rhs.substr(i, 2);
                if (op2c == ".*" || op2c == "./") {
                    op1 = clean_rhs.substr(0, i);
                    op = op2c;
                    op2 = clean_rhs.substr(i + 2);
                    std::string t = op1;
                    t.erase(0, t.find_first_not_of(" \t"));
                    if (t.empty()) continue;
                    return true;
                }
            }
            if (c == '*' || c == '/') {
                if (i > 0 && clean_rhs[i-1] == '.') continue;
                std::string left = clean_rhs.substr(0, i);
                left.erase(0, left.find_first_not_of(" \t"));
                if (left.empty()) continue;
                
                op1 = clean_rhs.substr(0, i);
                op = std::string(1, c);
                op2 = clean_rhs.substr(i + 1);
                return true;
            }
        }
    }
    
    return false;
}
