#include "plot_engine.h"
#include "math_funcs.h"

extern std::vector<double> parseArrayExpr(const std::string& rhs, bool& is_array, std::string& err);
extern double evaluate(const std::string& expr_str, std::string& err);
extern bool parseBinaryOp(const std::string& rhs, std::string& op1, std::string& op, std::string& op2);
extern std::vector<double> evaluateArrayBinaryOp(const std::string& op1_str, const std::string& op_str, const std::string& op2_str, bool& is_scalar_result, double& scalar_val, std::string& err);
extern std::string preprocessArrayLookups(const std::string& expr, std::string& err);
extern std::string preprocessLen(const std::string& s);

bool handlePlotCommands(const std::string& line, std::string& err, double& result) {
    std::string trimmed = line;
    trimmed.erase(0, trimmed.find_first_not_of(" \t"));
    trimmed.erase(trimmed.find_last_not_of(" \t") + 1);
    
    if (trimmed == "plot.show()" || trimmed == "plot.show( )") {
        appState = STATE_PLOT;
        result = 0.0;
        return true;
    }
    if (trimmed == "plot.close()" || trimmed == "plot.close( )") {
        plot_lines.clear();
        plot_hold = false;
        plot_manual_limits = false;
        appState = STATE_CALC;
        result = 0.0;
        return true;
    }
    if (trimmed.rfind("plot.hold(", 0) == 0 && trimmed.back() == ')') {
        std::string arg = trimmed.substr(10, trimmed.size() - 11);
        arg.erase(0, arg.find_first_not_of(" \t"));
        arg.erase(arg.find_last_not_of(" \t") + 1);
        if (arg == "1" || arg == "true") {
            plot_hold = true;
        } else {
            plot_hold = false;
        }
        result = 0.0;
        return true;
    }
    if (trimmed.rfind("plot.xlim(", 0) == 0 && trimmed.back() == ')') {
        std::string arg = trimmed.substr(10, trimmed.size() - 11);
        arg.erase(0, arg.find_first_not_of(" \t"));
        arg.erase(arg.find_last_not_of(" \t") + 1);
        if (arg.size() >= 2 && arg.front() == '[' && arg.back() == ']') {
            arg = arg.substr(1, arg.size() - 2);
        }
        size_t comma = arg.find(',');
        if (comma != std::string::npos) {
            std::string min_str = arg.substr(0, comma);
            std::string max_str = arg.substr(comma + 1);
            std::string eval_err;
            double min_v = evaluate(min_str, eval_err);
            double max_v = evaluate(max_str, eval_err);
            if (eval_err.empty() && !std::isnan(min_v) && !std::isnan(max_v)) {
                plot_xlim_min = min_v;
                plot_xlim_max = max_v;
                plot_manual_limits = true;
            } else {
                err = "Limit Eval Error";
            }
        } else {
            err = "Invalid xlim Format";
        }
        result = 0.0;
        return true;
    }
    if (trimmed.rfind("plot.ylim(", 0) == 0 && trimmed.back() == ')') {
        std::string arg = trimmed.substr(10, trimmed.size() - 11);
        arg.erase(0, arg.find_first_not_of(" \t"));
        arg.erase(arg.find_last_not_of(" \t") + 1);
        if (arg.size() >= 2 && arg.front() == '[' && arg.back() == ']') {
            arg = arg.substr(1, arg.size() - 2);
        }
        size_t comma = arg.find(',');
        if (comma != std::string::npos) {
            std::string min_str = arg.substr(0, comma);
            std::string max_str = arg.substr(comma + 1);
            std::string eval_err;
            double min_v = evaluate(min_str, eval_err);
            double max_v = evaluate(max_str, eval_err);
            if (eval_err.empty() && !std::isnan(min_v) && !std::isnan(max_v)) {
                plot_ylim_min = min_v;
                plot_ylim_max = max_v;
                plot_manual_limits = true;
            } else {
                err = "Limit Eval Error";
            }
        } else {
            err = "Invalid ylim Format";
        }
        result = 0.0;
        return true;
    }
    
    if (trimmed.rfind("plot(", 0) == 0 && trimmed.back() == ')') {
        std::string inner = trimmed.substr(5, trimmed.size() - 6);
        std::vector<std::string> args = splitPlotArgs(inner);
        std::string x_str = "x";
        std::string y_str = "";
        std::string color_str = "green";
        std::string style_str = "-";
        
        if (args.size() == 1) {
            y_str = args[0];
            x_str = "-10:0.2:10";
        } else {
            x_str = args[0];
            y_str = args[1];
            
            if (args.size() >= 3) {
                std::string arg3 = cleanQuotes(args[2]);
                bool is_style = (arg3 == "-" || arg3 == "--" || arg3 == "-." || arg3 == ":" || arg3 == "" || arg3 == "None");
                if (is_style) {
                    style_str = arg3;
                } else {
                    color_str = arg3;
                }
            }
            if (args.size() >= 4) {
                style_str = cleanQuotes(args[3]);
            }
        }
        
        uint16_t color_val = 0x07E0; // TFT_GREEN
        if (color_str == "red" || color_str == "r") color_val = 0xF800;
        else if (color_str == "green" || color_str == "g") color_val = 0x07E0;
        else if (color_str == "blue" || color_str == "b") color_val = 0x001F;
        else if (color_str == "cyan" || color_str == "c") color_val = 0x07FF;
        else if (color_str == "magenta" || color_str == "m") color_val = 0xF81F;
        else if (color_str == "yellow" || color_str == "y") color_val = 0xFFE0;
        else if (color_str == "black" || color_str == "k") color_val = 0x0000;
        else if (color_str == "white" || color_str == "w") color_val = 0xFFFF;
        
        bool x_is_scalar = false;
        for (const auto& ua : user_args) {
            if (ua.name == x_str) { x_is_scalar = true; break; }
        }
        std::vector<double> x_vec;
        if (!x_is_scalar && user_arrays.find(x_str) != user_arrays.end()) {
            x_vec = user_arrays[x_str];
        } else {
            bool is_arr = false;
            std::string parse_err;
            x_vec = parseArrayExpr(x_str, is_arr, parse_err);
            if (!is_arr || !parse_err.empty() || x_vec.empty()) {
                parse_err.clear();
                std::string prep_x = preprocessArrayLookups(x_str, parse_err);
                double val = evaluate(prep_x, parse_err);
                if (parse_err.empty() && !std::isnan(val)) {
                    x_vec = { val };
                } else {
                    err = "X vector parse error";
                    return true;
                }
            }
        }
        
        bool y_is_scalar = false;
        for (const auto& ua : user_args) {
            if (ua.name == y_str) { y_is_scalar = true; break; }
        }
        std::vector<double> y_vec;
        if (!y_is_scalar && user_arrays.find(y_str) != user_arrays.end()) {
            y_vec = user_arrays[y_str];
        } else {
            bool is_arr = false;
            std::string parse_err;
            std::vector<double> candidate = parseArrayExpr(y_str, is_arr, parse_err);
            std::string prep_y = preprocessArrayLookups(y_str, parse_err);
            std::string op1, op, op2;
            bool is_bin_op = parseBinaryOp(prep_y, op1, op, op2);
            if (is_arr && parse_err.empty()) {
                y_vec = candidate;
            } else if (is_bin_op) {
                bool is_scalar = false;
                double scalar_res = 0;
                std::string bin_err;
                std::vector<double> bin_res = evaluateArrayBinaryOp(op1, op, op2, is_scalar, scalar_res, bin_err);
                if (bin_err.empty() && !is_scalar) {
                    y_vec = bin_res;
                }
            }
            if (y_vec.empty()) {
                y_vec.clear();
                y_vec.reserve(x_vec.size());
                // Determine the x variable name to inject during point-by-point evaluation.
                // If x_str is a plain identifier (e.g., "t", "px"), use it so that y_str
                // expressions like sin(t) or cos(px) resolve correctly. For literal ranges
                // or expressions, fall back to "x".
                bool x_str_is_id = (!x_str.empty() && (std::isalpha((unsigned char)x_str[0]) || x_str[0] == '_'));
                if (x_str_is_id) {
                    for (char c : x_str) {
                        if (!std::isalnum((unsigned char)c) && c != '_') { x_str_is_id = false; break; }
                    }
                }
                std::string x_var_name = (x_str_is_id || user_arrays.find(x_str) != user_arrays.end()) ? x_str : "x";
                int x_idx = -1;
                for (size_t k = 0; k < user_args.size(); ++k) {
                    if (user_args[k].name == x_var_name) {
                        x_idx = (int)k;
                        break;
                    }
                }
                bool created_x = false;
                double old_x = 0.0;
                if (x_idx != -1) {
                    old_x = user_args[x_idx].val;
                } else {
                    user_args.push_back({x_var_name, 0.0});
                    x_idx = (int)user_args.size() - 1;
                    created_x = true;
                }

                for (double xv : x_vec) {
                    user_args[x_idx].val = xv;
                    std::string eval_err;
                    double yv = evaluate(preprocessLen(prep_y), eval_err);
                    if (eval_err.empty() && !std::isnan(yv)) {
                        y_vec.push_back(yv);
                    } else {
                        y_vec.push_back(std::numeric_limits<double>::quiet_NaN());
                    }
                }

                if (created_x) {
                    user_args.pop_back();
                } else {
                    user_args[x_idx].val = old_x;
                }
            }
        }
        
        if (!plot_hold) {
            plot_lines.clear();
        }
        if (plot_lines.size() >= 4) {
            err = "Plot Limit Error";
            result = std::numeric_limits<double>::quiet_NaN();
            return true;
        }
        
        plot_lines.push_back({ x_vec, y_vec, color_val, style_str });
        plot_center_x = 0.0;
        plot_center_y = 0.0;
        plot_scale = 1.0;
        plot_manual_limits = false;
        result = 1.0;
        return true;
    }
    
    return false;
}
