#include "script_engine.h"
#include "math_funcs.h"
#include "plot_engine.h"
#include <iostream>
#ifdef ARDUINO
#include <Arduino.h>
#endif

extern double evaluate(const std::string& expr_str, std::string& err);
extern double evaluateInput(const std::string& input, std::string& err, bool& isDefinition);
extern std::string preprocessArrayLookups(const std::string& expr, std::string& err);
extern std::string formatPrintString(const std::string& str, std::string& err);
extern bool parseArrayElementAssignment(const std::string& line, std::string& name, std::string& idx_expr, std::string& val_expr);
extern bool parseBinaryOp(const std::string& rhs, std::string& op1, std::string& op, std::string& op2);
extern std::vector<double> evaluateArrayBinaryOp(const std::string& op1_str, const std::string& op_str, const std::string& op2_str, bool& is_scalar_result, double& scalar_val, std::string& err);
extern std::vector<double> parseArrayExpr(const std::string& rhs, bool& is_array, std::string& err);
extern std::string preprocessLen(const std::string& s);

double script_return_val = 0.0;
bool script_has_returned = false;

void runScript(const std::string& code) {
    script_console_output.clear();
    plot_lines.clear();
    plot_hold = false;
    user_args.clear();
    user_arrays.clear();
    
    auto original_args = user_args;
    auto original_funcs = user_funcs;
    auto original_autocomplete = autocomplete_words;
    auto original_arrays = user_arrays;
    user_args.reserve(256);
    
    std::vector<std::string> lines = splitIntoStatements(code);
    
    size_t ip = 0;
    int max_steps = 10000;
    int step_count = 0;
    script_return_val = 0.0;
    script_has_returned = false;
    
    struct BlockState {
        std::string type;       // "if", "for", "while"
        bool condition_met;     // For "if" chain: has any branch been executed?
        size_t start_ip;        // For loops/conditionals: start of block header
        size_t end_ip;          // Ultimate end of the block (the matching end/endif/endfor/endwhile)
        
        std::string var;
        double end_val;
        double step_val;
        
        std::string cond_str;
        std::string step_str;
    };
    std::vector<BlockState> blocks;
    
    while (ip < lines.size() && step_count < max_steps) {
        if (script_has_returned) break;
        step_count++;
        #ifdef ARDUINO
        yield();
        #endif
        
        std::string line = lines[ip];
        line.erase(0, line.find_first_not_of(" \t\r"));
        line.erase(line.find_last_not_of(" \t\r") + 1);
        if (line.empty() || line[0] == '#' || line.rfind("//", 0) == 0) {
            ip++;
            continue;
        }
        if (line.find("X =") != std::string::npos || line.find("Y =") != std::string::npos) {
            printf("[LINE EXEC ip=%zu] %s\n", ip, line.c_str());
        }
        if (step_count < 100) {
            printf("[STEP %d ip=%zu] %s\n", step_count, ip, line.c_str());
        }
        
        // return statement
        if (line.rfind("return", 0) == 0 && (line.size() == 6 || line[6] == ' ' || line[6] == '\t')) {
            std::string ret_expr = (line.size() > 6) ? line.substr(6) : "0";
            ret_expr.erase(0, ret_expr.find_first_not_of(" \t"));
            ret_expr.erase(ret_expr.find_last_not_of(" \t") + 1);
            if (ret_expr.empty()) ret_expr = "0";
            
            std::string eval_err;
            script_return_val = evaluate(preprocessArrayLookups(ret_expr, eval_err), eval_err);
            if (!eval_err.empty()) {
                script_console_output.push_back("Error: " + eval_err);
            } else {
                script_console_output.push_back("Return: " + fmtNum(script_return_val));
            }
            script_has_returned = true;
            break;
        }

        // fn / def / function declaration
        if (line.rfind("fn ", 0) == 0 || line.rfind("def ", 0) == 0 || line.rfind("function ", 0) == 0) {
            size_t open_paren = line.find('(');
            size_t close_paren = line.find(')', open_paren);
            size_t open_brace = line.find('{', close_paren);
            if (open_paren != std::string::npos && close_paren != std::string::npos && open_brace != std::string::npos) {
                size_t kw_len = (line.rfind("fn ", 0) == 0) ? 3 : ((line.rfind("def ", 0) == 0) ? 4 : 9);
                std::string fname = line.substr(kw_len, open_paren - kw_len);
                fname.erase(0, fname.find_first_not_of(" \t"));
                fname.erase(fname.find_last_not_of(" \t") + 1);
                
                std::string params_str = line.substr(open_paren + 1, close_paren - open_paren - 1);
                std::vector<std::string> params;
                size_t p_pos = 0;
                while (p_pos < params_str.size()) {
                    size_t comma = params_str.find(',', p_pos);
                    std::string p = (comma == std::string::npos) ? params_str.substr(p_pos) : params_str.substr(p_pos, comma - p_pos);
                    p.erase(0, p.find_first_not_of(" \t"));
                    p.erase(p.find_last_not_of(" \t") + 1);
                    if (!p.empty()) params.push_back(p);
                    if (comma == std::string::npos) break;
                    p_pos = comma + 1;
                }

                size_t end_ip = findMatchingBlockEnd(lines, ip);
                std::vector<std::string> body_stmts;
                for (size_t k = ip + 1; k < end_ip && k < lines.size(); ++k) {
                    body_stmts.push_back(lines[k]);
                }

                user_script_funcs.erase(std::remove_if(user_script_funcs.begin(), user_script_funcs.end(), [&](const CustomScriptFunc& f){
                    return f.name == fname;
                }), user_script_funcs.end());

                user_script_funcs.push_back({fname, params, body_stmts});
                if (std::find(autocomplete_words.begin(), autocomplete_words.end(), fname + "(") == autocomplete_words.end()) {
                    autocomplete_words.push_back(fname + "(");
                }
                ip = end_ip + 1;
                continue;
            }
        }
        
        // plot commands
        if (line.rfind("plot", 0) == 0) {
            std::string err;
            double result = 0.0;
            if (handlePlotCommands(line, err, result)) {
                if (!err.empty()) {
                    script_console_output.push_back("L:" + std::to_string(ip + 1) + " Err: " + err);
                }
                ip++;
                continue;
            }
        }

        // print(expr)
        if (line.rfind("print(", 0) == 0 && line.back() == ')') {
            std::string expr = line.substr(6, line.size() - 7);
            std::string err;
            std::string output = formatPrintString(expr, err);
            if (!err.empty()) {
                script_console_output.push_back("L:" + std::to_string(ip + 1) + " Err: " + err);
            } else {
                script_console_output.push_back(output);
            }
            ip++;
        }

        // sleep(ms)
        else if (line.rfind("sleep(", 0) == 0 && line.back() == ')') {
            std::string ms_str = line.substr(6, line.size() - 7);
            try {
                int ms = std::stoi(ms_str);
                #ifdef ARDUINO
                delay(ms);
                #endif
            } catch (...) {}
            ip++;
        }
        // if (cond) {
        else if (line.rfind("if (", 0) == 0 && line.back() == '{') {
            size_t open_paren = line.find('(');
            size_t close_paren = line.rfind(')');
            if (open_paren != std::string::npos && close_paren != std::string::npos && close_paren > open_paren) {
                std::string cond_str = line.substr(open_paren + 1, close_paren - open_paren - 1);
                cond_str.erase(0, cond_str.find_first_not_of(" \t"));
                cond_str.erase(cond_str.find_last_not_of(" \t") + 1);
                
                size_t end_ip = findMatchingBlockEnd(lines, ip);
                if (end_ip != std::string::npos) {
                    std::string err;
                    std::string prep_cond = preprocessArrayLookups(cond_str, err);
                    double cond_val = 0;
                    if (err.empty()) {
                        cond_val = evaluate(prep_cond, err);
                    }
                    bool cond_bool = (err.empty() && !std::isnan(cond_val) && cond_val != 0.0);
                    if (cond_str.find("X[z]") != std::string::npos || cond_str.find("X[i]") != std::string::npos || cond_str.find("c ==") != std::string::npos) {
                        std::string z_err;
                        double z_val = evaluate("z", z_err);
                        double i_val = evaluate("i", z_err);
                        double t_val = evaluate("t", z_err);
                        if (t_val == 1.0) {
                            std::string args_str = "";
                            for (const auto& a : user_args) args_str += a.name + "=" + std::to_string(a.val) + " ";
                            printf("[IF DBG t=1] line='%s' prep='%s' z=%.0f i=%.0f args='%s'\n", line.c_str(), prep_cond.c_str(), z_val, i_val, args_str.c_str());
                        }
                    }
                    
                    if (!err.empty()) {
                        script_console_output.push_back("L:" + std::to_string(ip + 1) + " Err: " + err);
                        ip = end_ip + 1;
                    } else if (cond_bool) {
                        blocks.push_back({ "if", true, ip, end_ip, "", 0.0, 0.0, "", "" });
                        ip++;
                    } else {
                        blocks.push_back({ "if", false, ip, end_ip, "", 0.0, 0.0, "", "" });
                        size_t next_branch = findMatchingBlockEnd(lines, ip, true);
                        if (next_branch != std::string::npos) {
                            ip = next_branch;
                        } else {
                            ip = end_ip;
                        }
                    }
                } else {
                    script_console_output.push_back("L:" + std::to_string(ip + 1) + " Err: Mismatched if");
                    ip++;
                }
            } else {
                script_console_output.push_back("L:" + std::to_string(ip + 1) + " Err: Mismatched if");
                ip++;
            }
        }
        // } else if (cond) { / else if (cond) { / } elif (cond) { / elif (cond) {
        else if ((line.rfind("} else if (", 0) == 0 || line.rfind("else if (", 0) == 0 ||
                  line.rfind("} elif (", 0) == 0 || line.rfind("elif (", 0) == 0) && line.back() == '{') {
            if (!blocks.empty() && blocks.back().type == "if") {
                if (blocks.back().condition_met) {
                    ip = blocks.back().end_ip;
                } else {
                    size_t open_paren = line.find('(');
                    size_t close_paren = line.rfind(')');
                    if (open_paren != std::string::npos && close_paren != std::string::npos && close_paren > open_paren) {
                        std::string cond_str = line.substr(open_paren + 1, close_paren - open_paren - 1);
                        cond_str.erase(0, cond_str.find_first_not_of(" \t"));
                        cond_str.erase(cond_str.find_last_not_of(" \t") + 1);
                        
                        std::string err;
                        std::string prep_cond = preprocessArrayLookups(cond_str, err);
                        double cond_val = 0;
                        if (err.empty()) {
                            cond_val = evaluate(prep_cond, err);
                        }
                        bool cond_bool = (err.empty() && !std::isnan(cond_val) && cond_val != 0.0);
                        
                        if (!err.empty()) {
                            script_console_output.push_back("L:" + std::to_string(ip + 1) + " Err: " + err);
                            ip = blocks.back().end_ip;
                        } else if (cond_bool) {
                            blocks.back().condition_met = true;
                            ip++;
                        } else {
                            size_t next_branch = findMatchingBlockEnd(lines, ip, true);
                            if (next_branch != std::string::npos) {
                                ip = next_branch;
                            } else {
                                ip = blocks.back().end_ip;
                            }
                        }
                    } else {
                        script_console_output.push_back("L:" + std::to_string(ip + 1) + " Err: Mismatched else if");
                        ip++;
                    }
                }
            } else {
                script_console_output.push_back("L:" + std::to_string(ip + 1) + " Err: Unmatched else if");
                ip++;
            }
        }
        // } else { / else {
        else if (line == "} else {" || line == "else {") {
            if (!blocks.empty() && blocks.back().type == "if") {
                if (blocks.back().condition_met) {
                    ip = blocks.back().end_ip;
                } else {
                    blocks.back().condition_met = true;
                    ip++;
                }
            } else {
                script_console_output.push_back("L:" + std::to_string(ip + 1) + " Err: Unmatched else");
                ip++;
            }
        }
        // while (cond) {
        else if ((line.rfind("while (", 0) == 0 || line.rfind("while(", 0) == 0) && line.back() == '{') {
            bool found_block = false;
            size_t block_idx = 0;
            for (size_t b = 0; b < blocks.size(); ++b) {
                if (blocks[b].start_ip == ip) {
                    found_block = true;
                    block_idx = b;
                    break;
                }
            }
            
            size_t open_paren = line.find('(');
            size_t close_paren = line.rfind(')');
            if (open_paren != std::string::npos && close_paren != std::string::npos && close_paren > open_paren) {
                std::string cond_str = line.substr(open_paren + 1, close_paren - open_paren - 1);
                cond_str.erase(0, cond_str.find_first_not_of(" \t"));
                cond_str.erase(cond_str.find_last_not_of(" \t") + 1);
                
                size_t end_ip = found_block ? blocks[block_idx].end_ip : findMatchingBlockEnd(lines, ip);
                if (end_ip != std::string::npos) {
                    std::string err;
                    std::string prep_cond = preprocessArrayLookups(cond_str, err);
                    double cond_val = 0;
                    if (err.empty()) {
                        cond_val = evaluate(prep_cond, err);
                    }
                    bool cond_bool = (err.empty() && !std::isnan(cond_val) && cond_val != 0.0);
                    
                    if (!err.empty()) {
                        script_console_output.push_back("L:" + std::to_string(ip + 1) + " Err: " + err);
                        if (found_block) blocks.erase(blocks.begin() + block_idx);
                        ip = end_ip + 1;
                    } else if (cond_bool) {
                        if (!found_block) {
                            blocks.push_back({ "while", true, ip, end_ip, "", 0.0, 0.0, cond_str, "" });
                        }
                        ip++;
                    } else {
                        if (found_block) {
                            blocks.erase(blocks.begin() + block_idx);
                        }
                        ip = end_ip;
                    }
                } else {
                    script_console_output.push_back("L:" + std::to_string(ip + 1) + " Err: Mismatched while");
                    ip++;
                }
            } else {
                script_console_output.push_back("L:" + std::to_string(ip + 1) + " Err: Mismatched while");
                ip++;
            }
        }
        // for (init; cond; step) {
        else if ((line.rfind("for (", 0) == 0 || line.rfind("for(", 0) == 0) && line.back() == '{') {
            bool found_block = false;
            size_t block_idx = 0;
            for (size_t b = 0; b < blocks.size(); ++b) {
                if (blocks[b].start_ip == ip) {
                    found_block = true;
                    block_idx = b;
                    break;
                }
            }

            size_t open_paren = line.find('(');
            size_t close_paren = line.rfind(')');
            if (open_paren != std::string::npos && close_paren != std::string::npos && close_paren > open_paren) {
                std::string inner = line.substr(open_paren + 1, close_paren - open_paren - 1);
                size_t first_semi = inner.find(';');
                size_t second_semi = inner.find(';', first_semi + 1);
                if (first_semi != std::string::npos && second_semi != std::string::npos) {
                    std::string init_str = inner.substr(0, first_semi);
                    std::string cond_str = inner.substr(first_semi + 1, second_semi - first_semi - 1);
                    std::string step_str = inner.substr(second_semi + 1);
                    
                    auto trim = [](std::string& s) {
                        s.erase(0, s.find_first_not_of(" \t\r"));
                        s.erase(s.find_last_not_of(" \t\r") + 1);
                    };
                    trim(init_str);
                    trim(cond_str);
                    trim(step_str);
                    
                    size_t end_ip = found_block ? blocks[block_idx].end_ip : findMatchingBlockEnd(lines, ip);
                    if (end_ip != std::string::npos) {
                        std::string err;
                        bool isDef = false;
                        if (!found_block) {
                            if (!init_str.empty()) {
                                size_t eq = init_str.find('=');
                                if (eq != std::string::npos) {
                                    std::string lhs = init_str.substr(0, eq);
                                    std::string rhs = init_str.substr(eq + 1);
                                    auto trim = [](std::string& s) {
                                        s.erase(0, s.find_first_not_of(" \t"));
                                        s.erase(s.find_last_not_of(" \t") + 1);
                                    };
                                    trim(lhs); trim(rhs);
                                    std::string init_err;
                                    double val = evaluate(preprocessArrayLookups(rhs, init_err), init_err);
                                    if (init_err.empty()) {
                                        bool found = false;
                                        for (auto& a : user_args) {
                                            if (a.name == lhs) {
                                                a.val = val;
                                                found = true;
                                                break;
                                            }
                                        }
                                        if (!found) {
                                            user_args.push_back({lhs, val});
                                        }
                                    } else {
                                        err = init_err;
                                    }
                                } else {
                                    evaluateInput(preprocessArrayLookups(init_str, err), err, isDef);
                                }
                            }
                        }
                        
                        if (!err.empty()) {
                            script_console_output.push_back("L:" + std::to_string(ip + 1) + " Err: " + err);
                            if (found_block) blocks.erase(blocks.begin() + block_idx);
                            ip = end_ip + 1;
                        } else {
                            std::string cond_err;
                            double cond_val = evaluate(preprocessArrayLookups(cond_str, cond_err), cond_err);
                            bool cond_bool = (cond_err.empty() && !std::isnan(cond_val) && cond_val != 0.0);
                            
                            if (!cond_err.empty()) {
                                script_console_output.push_back("L:" + std::to_string(ip + 1) + " Err: " + cond_err);
                                if (found_block) blocks.erase(blocks.begin() + block_idx);
                                ip = end_ip + 1;
                            } else if (cond_bool) {
                                if (!found_block) {
                                    blocks.push_back({ "for", true, ip, end_ip, "", 0.0, 0.0, cond_str, step_str });
                                }
                                ip++;
                            } else {
                                if (found_block) {
                                    blocks.erase(blocks.begin() + block_idx);
                                }
                                ip = end_ip + 1;
                            }
                        }
                    } else {
                        script_console_output.push_back("L:" + std::to_string(ip + 1) + " Err: Mismatched for");
                        ip++;
                    }
                } else {
                    script_console_output.push_back("L:" + std::to_string(ip + 1) + " Err: Mismatched for syntax");
                    ip++;
                }
            } else {
                script_console_output.push_back("L:" + std::to_string(ip + 1) + " Err: Mismatched for");
                ip++;
            }
        }
        else if (isBlockEnd(line)) {
            if (!blocks.empty()) {
                size_t match_idx = std::string::npos;
                for (int b = (int)blocks.size() - 1; b >= 0; --b) {
                    if (blocks[b].end_ip == ip) {
                        match_idx = (size_t)b;
                        break;
                    }
                }
                if (match_idx != std::string::npos) {
                    auto curr = blocks[match_idx];
                    if (curr.type == "for" || curr.type == "while") {
                        if (curr.type == "for" && !curr.step_str.empty()) {
                            std::string err;
                            bool isDef = false;
                            std::vector<std::string> step_stmts = splitIntoStatements(curr.step_str);
                            for (auto st : step_stmts) {
                                st.erase(0, st.find_first_not_of(" \t"));
                                st.erase(st.find_last_not_of(" \t") + 1);
                                if (st.size() > 2 && (st.substr(st.size() - 2) == "++" || st.substr(st.size() - 2) == "--")) {
                                    std::string var_name = st.substr(0, st.size() - 2);
                                    var_name.erase(0, var_name.find_first_not_of(" \t"));
                                    var_name.erase(var_name.find_last_not_of(" \t") + 1);
                                    char op = st[st.size() - 1];
                                    bool found = false;
                                    for (auto& arg : user_args) {
                                        if (arg.name == var_name) {
                                            if (op == '+') arg.val += 1.0;
                                            else arg.val -= 1.0;
                                            found = true;
                                            break;
                                        }
                                    }
                                    if (!found) {
                                        user_args.push_back({var_name, op == '+' ? 1.0 : -1.0});
                                    }
                                } else if (st.find('=') != std::string::npos) {
                                    size_t eq = st.find('=');
                                    std::string lhs = st.substr(0, eq);
                                    std::string rhs = st.substr(eq + 1);
                                    if (!lhs.empty() && (lhs.back() == '+' || lhs.back() == '-' || lhs.back() == '*' || lhs.back() == '/')) {
                                        char op = lhs.back();
                                        lhs = lhs.substr(0, lhs.size() - 1);
                                        rhs = lhs + " " + std::string(1, op) + " (" + rhs + ")";
                                    }
                                    auto trim = [](std::string& s) {
                                        s.erase(0, s.find_first_not_of(" \t"));
                                        s.erase(s.find_last_not_of(" \t") + 1);
                                    };
                                    trim(lhs); trim(rhs);
                                    double val = evaluate(preprocessLen(preprocessArrayLookups(rhs, err)), err);
                                    if (err.empty()) {
                                        bool found = false;
                                        for (auto& a : user_args) {
                                            if (a.name == lhs) {
                                                a.val = val;
                                                found = true;
                                                break;
                                            }
                                        }
                                        if (!found) {
                                            user_args.push_back({lhs, val});
                                        }
                                    }
                                } else {
                                    evaluateInput(preprocessLen(preprocessArrayLookups(st, err)), err, isDef);
                                }
                            }
                        }
                        ip = curr.start_ip;
                    } else { // "if"
                        blocks.erase(blocks.begin() + match_idx);
                        ip++;
                    }
                } else {
                    ip++;
                }
            } else {
                ip++;
            }
        }
        // Increment / Decrement: i++ or i--
        else if (line.size() > 2 && (line.substr(line.size() - 2) == "++" || line.substr(line.size() - 2) == "--")) {
            std::string var_name = line.substr(0, line.size() - 2);
            var_name.erase(0, var_name.find_first_not_of(" \t"));
            var_name.erase(var_name.find_last_not_of(" \t") + 1);
            char op = line[line.size() - 1];
            bool found = false;
            for (auto& arg : user_args) {
                if (arg.name == var_name) {
                    if (op == '+') arg.val += 1.0;
                    else arg.val -= 1.0;
                    found = true;
                    break;
                }
            }
            if (!found) {
                user_args.push_back({var_name, op == '+' ? 1.0 : -1.0});
            }
            ip++;
        }
        // Array element assignment: A[idx] = expr
        else if (std::string name, idx_expr, val_expr; parseArrayElementAssignment(line, name, idx_expr, val_expr)) {
            if (user_arrays.find(name) == user_arrays.end()) {
                script_console_output.push_back("L:" + std::to_string(ip + 1) + " Err: Array " + name + " not defined");
                ip++;
            } else {
                std::string err;
                double idx_val = evaluate(preprocessArrayLookups(idx_expr, err), err);
                double val_val = evaluate(preprocessArrayLookups(val_expr, err), err);
                if (!err.empty()) {
                    script_console_output.push_back("L:" + std::to_string(ip + 1) + " Err: " + err);
                } else {
                    int idx = (int)std::round(idx_val);
                    auto& arr = user_arrays[name];
                    if (idx < 1 || idx > (int)arr.size()) {
                        script_console_output.push_back("L:" + std::to_string(ip + 1) + " Err: Index " + std::to_string(idx) + " out of bounds");
                    } else {
                        arr[idx - 1] = val_val;
                    }
                }
                ip++;
            }
        }
        // standard assignment: name = expr or name op= expr
        else if (size_t eq = line.find('='); eq != std::string::npos && (eq == 0 || line[eq-1] != '=') && (eq + 1 >= line.size() || line[eq+1] != '=') && (eq == 0 || (line[eq-1] != '<' && line[eq-1] != '>' && line[eq-1] != '!'))) {
            std::string lhs = line.substr(0, eq);
            std::string rhs = line.substr(eq + 1);
            if (!lhs.empty() && (lhs.back() == '+' || lhs.back() == '-' || lhs.back() == '*' || lhs.back() == '/')) {
                char op = lhs.back();
                lhs = lhs.substr(0, lhs.size() - 1);
                rhs = lhs + " " + std::string(1, op) + " (" + rhs + ")";
            }
            lhs.erase(0, lhs.find_first_not_of(" \t"));
            lhs.erase(lhs.find_last_not_of(" \t") + 1);
            rhs.erase(0, rhs.find_first_not_of(" \t"));
            rhs.erase(rhs.find_last_not_of(" \t") + 1);
            
            bool is_const_assign = false;
            for (const auto& cn : user_consts) {
                if (cn.name == lhs) {
                    is_const_assign = true;
                    break;
                }
            }
            if (is_const_assign) {
                script_console_output.push_back("L:" + std::to_string(ip + 1) + " Err: Const Error");
                ip++;
                continue;
            }
            
            std::string op1, op, op2;
            std::string err;
            bool is_arr_op = false;
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
                bool op1_is_scalar = false, op2_is_scalar = false;
                for (const auto& ua : user_args) {
                    if (ua.name == op1) op1_is_scalar = true;
                    if (ua.name == op2) op2_is_scalar = true;
                }
                if (op == ".*" || op == "./" || (is1_arr && !a1.empty()) || (is2_arr && !a2.empty()) || (!op1_is_scalar && user_arrays.find(op1) != user_arrays.end() && !user_arrays[op1].empty()) || (!op2_is_scalar && user_arrays.find(op2) != user_arrays.end() && !user_arrays[op2].empty())) {
                    is_arr_op = true;
                }
            }

            if (is_arr_op) {
                bool is_scalar = false;
                double scalar_res = 0;
                std::vector<double> res = evaluateArrayBinaryOp(op1, op, op2, is_scalar, scalar_res, err);
                if (!err.empty()) {
                    script_console_output.push_back("L:" + std::to_string(ip + 1) + " Err: " + err);
                } else if (is_scalar) {
                    user_arrays.erase(lhs);
                    bool found = false;
                    for (auto& a : user_args) {
                        if (a.name == lhs) {
                            a.val = scalar_res;
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        user_args.push_back({lhs, scalar_res});
                    }
                    if (std::find(autocomplete_words.begin(), autocomplete_words.end(), lhs) == autocomplete_words.end()) {
                        autocomplete_words.push_back(lhs);
                    }
                } else {
                    user_arrays[lhs] = res;
                }
            } else {
                bool is_arr = false;
                std::string parse_arr_err;
                std::vector<double> arr = parseArrayExpr(rhs, is_arr, parse_arr_err);
                if (is_arr && parse_arr_err.empty()) {
                    user_arrays[lhs] = arr;
                    user_args.erase(std::remove_if(user_args.begin(), user_args.end(), [&](const UserArg& a) {
                        return a.name == lhs;
                    }), user_args.end());
                } else {
                    user_arrays.erase(lhs);
                    // Pre-expand len() so that expressions like len(Y) become scalars
                    // before we scan rhs for array variable references.
                    std::string len_err;
                    std::string rhs_prepped = preprocessLen(preprocessArrayLookups(rhs, len_err));
                    // Check if rhs references a user array by name (e.g., "sin(x)" where x is array).
                    // If so, evaluate the expression element-wise substituting each array value.
                    std::string source_arr_name;
                    const std::vector<double>* source_arr = nullptr;
                    for (const auto& kv : user_arrays) {
                        if (kv.second.empty()) continue;
                        const std::string& aname = kv.first;
                        // Skip if aname is also defined as a scalar variable in user_args
                        bool is_scalar_var = false;
                        for (const auto& arg : user_args) {
                            if (arg.name == aname) { is_scalar_var = true; break; }
                        }
                        if (is_scalar_var) continue;

                        size_t pos = rhs_prepped.find(aname);
                        while (pos != std::string::npos) {
                            bool pre_ok = (pos == 0 || (!std::isalnum((unsigned char)rhs_prepped[pos-1]) && rhs_prepped[pos-1] != '_'));
                            size_t end = pos + aname.size();
                            bool post_ok = (end >= rhs_prepped.size() || (!std::isalnum((unsigned char)rhs_prepped[end]) && rhs_prepped[end] != '_'));
                            bool not_indexed = (end >= rhs_prepped.size() || rhs_prepped[end] != '[');
                            if (pre_ok && post_ok && not_indexed) {
                                source_arr_name = aname;
                                source_arr = &kv.second;
                                break;
                            }
                            pos = rhs_prepped.find(aname, pos + 1);
                        }
                        if (source_arr) break;
                    }

                    if (source_arr && !source_arr->empty()) {
                        bool name_is_id = (!source_arr_name.empty() && (std::isalpha((unsigned char)source_arr_name[0]) || source_arr_name[0] == '_'));
                        if (name_is_id) {
                            for (char c : source_arr_name) {
                                if (!std::isalnum((unsigned char)c) && c != '_') { name_is_id = false; break; }
                            }
                        }
                        if (name_is_id) {
                            int x_idx = -1;
                            for (size_t k = 0; k < user_args.size(); ++k) {
                                if (user_args[k].name == source_arr_name) { x_idx = (int)k; break; }
                            }
                            bool created_var = false;
                            double old_val = 0.0;
                            if (x_idx != -1) {
                                old_val = user_args[x_idx].val;
                            } else {
                                user_args.push_back({source_arr_name, 0.0});
                                x_idx = (int)user_args.size() - 1;
                                created_var = true;
                            }
                            std::string prep_err;
                            std::string prep_rhs = rhs_prepped;
                            std::vector<double> elem_res;
                            elem_res.reserve(source_arr->size());
                            bool all_ok = true;
                            std::string eval_err;
                            for (double val : *source_arr) {
                                user_args[x_idx].val = val;
                                eval_err.clear();
                                double y = evaluate(prep_rhs, eval_err);
                                if (!eval_err.empty()) { all_ok = false; break; }
                                elem_res.push_back(y);
                            }
                            if (created_var) user_args.pop_back();
                            else user_args[x_idx].val = old_val;

                            if (!all_ok) {
                                script_console_output.push_back("L:" + std::to_string(ip + 1) + " Err: " + eval_err);
                            } else {
                                user_arrays[lhs] = elem_res;
                                user_args.erase(std::remove_if(user_args.begin(), user_args.end(), [&](const UserArg& a) {
                                    return a.name == lhs;
                                }), user_args.end());
                            }
                        } else {
                            std::string eval_err;
                            double val = evaluate(preprocessLen(preprocessArrayLookups(rhs, eval_err)), eval_err);
                            if (!eval_err.empty()) {
                                script_console_output.push_back("L:" + std::to_string(ip + 1) + " Err: " + eval_err);
                            } else {
                                user_arrays.erase(lhs);
                                bool found = false;
                                for (auto& a : user_args) {
                                    if (a.name == lhs) {
                                        a.val = val;
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
                            }
                        }
                    } else {
                        std::string eval_err;
                        std::string prep_rhs = preprocessLen(preprocessArrayLookups(rhs, eval_err));
                        double val = evaluate(prep_rhs, eval_err);
                        if (!eval_err.empty()) {
                            script_console_output.push_back("L:" + std::to_string(ip + 1) + " Err: " + eval_err);
                        } else {
                            bool found = false;
                            for (auto& a : user_args) {
                                if (a.name == lhs) {
                                    a.val = val;
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
                        }
                    }
                }
            }
            if (script_has_returned) break;
            ip++;
            continue;
        }
        else {
            if (line.rfind("plot", 0) == 0) {
                std::string err;
                double result = 0.0;
                if (handlePlotCommands(line, err, result)) {
                    if (!err.empty()) {
                        script_console_output.push_back("L:" + std::to_string(ip + 1) + " Err: " + err);
                    }
                }
            } else {
                bool isDef = false;
                std::string eval_err;
                evaluateInput(preprocessArrayLookups(line, eval_err), eval_err, isDef);
                if (script_has_returned) break;
                if (!eval_err.empty()) {
                    script_console_output.push_back("L:" + std::to_string(ip + 1) + " Err: " + eval_err);
                }
            }
            ip++;
        }
    }
    
    if (step_count >= max_steps) {
        script_console_output.push_back("Error: Loop limit reached");
    }
    
    user_funcs = original_funcs;
    autocomplete_words = original_autocomplete;
    // Note: user_arrays intentionally not restored — scripts may define arrays
    // that affect subsequent plot rendering (e.g., x, y1) or global state.
}
