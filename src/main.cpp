#include <Arduino.h>
#include <M5Cardputer.h>
#include <utility/Keyboard/KeyboardReader/IOMatrix.h>
#include <utility/Keyboard/KeyboardReader/TCA8418.h>
#include <SD.h>
#include <SPI.h>
#ifdef ARDUINO
#include <Preferences.h>
#endif
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include "config.h"

#define VERSION "1.0"
#define SCR_W 240
#define SCR_H 135
#define EXPR_Y 22
#define EXPR_SIZE 2
#define RES_Y 70

#include "app_state.h"
#include "help_engine.h"

#define SD_SPI_SCK_PIN  40
#define SD_SPI_MISO_PIN 39
#define SD_SPI_MOSI_PIN 14
#define SD_SPI_CS_PIN   12

AppState appState = STATE_CALC;

static void resetAppState(AppState newState);
static void navigateBack();

static std::string expression = "";
static std::string resultStr = "";
static bool hasError = false;

static int screen_off_timeout = 30; // in seconds
static int backlight_brightness = 128; // 0-255
static uint32_t last_activity_time = 0;
static bool screen_is_on = true;
static bool sd_initialized = false;

#include "math_engine.h"

// Define global states declared extern in math_engine.h
std::vector<double> history;
std::vector<UserArg> user_args;
std::vector<UserConst> user_consts;
std::vector<std::string> user_funcs;
std::vector<CustomScriptFunc> user_script_funcs;
bool degreesMode = true;
bool use_thousands_sep = false;
bool auto_brackets = true;
bool sticky_mode = false;
bool show_help_popup = false;

static bool sticky_fn_active = false;
static bool sticky_shift_active = false;
static bool sticky_opt_active = false;
static bool sticky_ctrl_active = false;
static bool sticky_alt_active = false;

static uint32_t press_time_fn = 0;
static uint32_t press_time_shift = 0;
static uint32_t press_time_opt = 0;
static uint32_t press_time_ctrl = 0;
static uint32_t press_time_alt = 0;
std::vector<std::string> autocomplete_words = {
    "sin(", "cos(", "tan(", "ctan(", "asin(", "acos(", "atan(",
    "sinh(", "cosh(", "tanh(", "asinh(", "acosh(", "atanh(",
    "deg2rad(", "d2r(", "rad2deg(", "r2d(",
    "sqrt(", "ln(", "log(", "log2(", "logb(", "exp(", "cbrt(",
    "abs(", "ceil(", "floor(", "round(", "trunc(", "sgn(", "mod(",
    "mean(", "median(", "mode(", "var(", "std(", "dot(", "rUni(", "rNor(",
    "fact(", "C(", "P(", "gcd(", "lcm(", "fib(", "len(", "print(", "help(", "help()",
    "pi", "e", "phi",
    "plot(", "plot.show()", "plot.close()", "plot.hold(", "plot.xlim(", "plot.ylim("
};
std::map<std::string, std::vector<double>> user_arrays;

// Advanced Cardulator States and Structures
struct Script {
    std::string name;
    std::string content;
};
static std::vector<Script> user_scripts;
static bool script_name_prompt_mode = false;
static bool is_renaming_script = false;
static std::string script_name_edit_buf = "";

struct Bind {
    char key;
    std::string action;
};
static std::vector<Bind> user_binds;

struct Formula {
    std::string name;
    std::string expr;
    std::vector<std::string> params;
};
static std::vector<Formula> user_formulas;

// Variables Manager state
static int var_selected_idx = 0;
static bool var_edit_mode = false;
static std::string var_edit_buf = "";

// Binds Manager state
static int bind_selected_idx = 0;
static bool bind_edit_mode = false;
static std::string bind_edit_buf = "";

// Formulas Manager state
static int formula_selected_idx = 0;
static bool formula_wizard_mode = false;
static int formula_wizard_param_idx = 0;
static std::vector<double> formula_wizard_values;
static std::vector<std::string> formula_wizard_bufs;
static bool formula_wizard_has_result = false;
static std::string formula_wizard_result_str = "";
static bool formula_create_mode = false;
static std::string formula_create_buf = "";

static bool formula_edit_mode = false;
static std::string formula_edit_buf = "";
static std::string formula_edit_orig_buf = "";
static int formula_cursor_pos = 0;
static bool formula_exit_prompt_mode = false;
static int formula_exit_selected_idx = 0; // 0 = Yes (Save), 1 = No (Discard)

// Scripts Manager state
static int script_selected_idx = 0;
static bool script_edit_mode = false;
static std::string script_edit_buf = "";
static std::string script_edit_orig_buf = "";
static int script_cursor_pos = 0;
static bool script_running_mode = false;
static int script_console_scroll_offset = 0;
std::vector<std::string> script_console_output;
static bool script_exit_prompt_mode = false;
static int script_exit_selected_idx = 0; // 0 = Yes (Save), 1 = No (Discard)

// Delete Prompt state
static bool delete_confirm_prompt_mode = false;
static int delete_confirm_selected_idx = 0; // 0 = Yes (Delete), 1 = No (Cancel)

// Plot Mode state
static std::string plot_expr = "sin(x)";
double plot_center_x = 0.0;
double plot_center_y = 0.0;
double plot_scale = 1.0;

std::vector<PlotLine> plot_lines;
bool plot_hold = false;
bool plot_manual_limits = false;
double plot_xlim_min = 0.0;
double plot_xlim_max = 0.0;
double plot_ylim_min = 0.0;
double plot_ylim_max = 0.0;

// REPL specific state
static std::vector<std::string> expr_history;
static std::vector<std::string> history_results;
static int history_index = -1;
static std::string clipboard = "";
static std::string undo_buffer = "";
static std::string redo_buffer = "";
static bool select_all_active = false;
static int cursor_pos = 0;
static int history_scroll_offset = 0;

// NVS Storage Implementation
#ifdef ARDUINO
static Preferences preferences;

static void loadNVSData() {
    preferences.begin("cardulator", false);
    screen_off_timeout = preferences.getInt("scr_off", 30);
    backlight_brightness = preferences.getInt("brightness", 128);
    use_thousands_sep = preferences.getBool("thousands_sep", false);
    auto_brackets = preferences.getBool("auto_brackets", true);
    sticky_mode = preferences.getBool("sticky_mode", false);
    
    // Load variables (user_args)
    std::string vars_str = preferences.getString("vars", ::String("")).c_str();
    user_args.clear();
    size_t pos = 0;
    while (pos < vars_str.size()) {
        size_t next_semi = vars_str.find(';', pos);
        std::string pair = vars_str.substr(pos, next_semi == std::string::npos ? std::string::npos : next_semi - pos);
        size_t eq = pair.find('=');
        if (eq != std::string::npos) {
            std::string name = pair.substr(0, eq);
            std::string val_str = pair.substr(eq + 1);
            try {
                double val = std::stod(val_str);
                user_args.push_back({name, val});
            } catch (...) {}
        }
        if (next_semi == std::string::npos) break;
        pos = next_semi + 1;
    }
    
    // Load constants (user_consts)
    std::string consts_str = preferences.getString("consts", ::String("")).c_str();
    user_consts.clear();
    pos = 0;
    while (pos < consts_str.size()) {
        size_t next_semi = consts_str.find(';', pos);
        std::string pair = consts_str.substr(pos, next_semi == std::string::npos ? std::string::npos : next_semi - pos);
        size_t eq = pair.find('=');
        if (eq != std::string::npos) {
            std::string name = pair.substr(0, eq);
            std::string val_str = pair.substr(eq + 1);
            try {
                double val = std::stod(val_str);
                user_consts.push_back({name, val});
            } catch (...) {}
        }
        if (next_semi == std::string::npos) break;
        pos = next_semi + 1;
    }
    if (user_consts.empty()) {
        user_consts.push_back({"pi", 3.141592653589793});
        user_consts.push_back({"e", 2.718281828459045});
    }
    
    // Load functions (user_funcs)
    std::string funcs_str = preferences.getString("funcs", ::String("")).c_str();
    user_funcs.clear();
    pos = 0;
    while (pos < funcs_str.size()) {
        size_t next_semi = funcs_str.find(';', pos);
        std::string func = funcs_str.substr(pos, next_semi == std::string::npos ? std::string::npos : next_semi - pos);
        if (!func.empty()) {
            user_funcs.push_back(func);
        }
        if (next_semi == std::string::npos) break;
        pos = next_semi + 1;
    }
    
    // Load scripts
    std::string scr_list = preferences.getString("scr_list", ::String("")).c_str();
    user_scripts.clear();
    pos = 0;
    while (pos < scr_list.size()) {
        size_t next_semi = scr_list.find(';', pos);
        std::string name = scr_list.substr(pos, next_semi == std::string::npos ? std::string::npos : next_semi - pos);
        if (!name.empty()) {
            std::string key = "s_" + name;
            std::string content = preferences.getString(key.c_str(), ::String("")).c_str();
            user_scripts.push_back({name, content});
        }
        if (next_semi == std::string::npos) break;
        pos = next_semi + 1;
    }
    if (user_scripts.empty()) {
        user_scripts.push_back({"fib", "x = 1\ny = 1\nfor (i = 1; i <= 10; i++) {\n  print(x)\n  tmp = x + y\n  x = y\n  y = tmp\n}\n"});
    }
    
    // Load binds
    std::string binds_str = preferences.getString("binds", ::String("")).c_str();
    user_binds.clear();
    pos = 0;
    while (pos < binds_str.size()) {
        size_t next_semi = binds_str.find(';', pos);
        std::string pair = binds_str.substr(pos, next_semi == std::string::npos ? std::string::npos : next_semi - pos);
        size_t col = pair.find(':');
        if (col != std::string::npos && col > 0) {
            char key = pair[0];
            std::string action = pair.substr(col + 1);
            user_binds.push_back({key, action});
        }
        if (next_semi == std::string::npos) break;
        pos = next_semi + 1;
    }
    if (user_binds.empty()) {
        user_binds.push_back({'s', "sin("});
        user_binds.push_back({'p', "pi"});
    }
    
    // Load formulas
    std::string formulas_str = preferences.getString("formulas", ::String("")).c_str();
    user_formulas.clear();
    pos = 0;
    while (pos < formulas_str.size()) {
        size_t next_semi = formulas_str.find(';', pos);
        std::string item = formulas_str.substr(pos, next_semi == std::string::npos ? std::string::npos : next_semi - pos);
        size_t first_bar = item.find('|');
        size_t second_bar = (first_bar == std::string::npos) ? std::string::npos : item.find('|', first_bar + 1);
        if (first_bar != std::string::npos && second_bar != std::string::npos) {
            std::string name = item.substr(0, first_bar);
            std::string expr = item.substr(first_bar + 1, second_bar - first_bar - 1);
            std::string params_str = item.substr(second_bar + 1);
            
            std::vector<std::string> params;
            size_t p_pos = 0;
            while (p_pos < params_str.size()) {
                size_t next_comma = params_str.find(',', p_pos);
                std::string param = params_str.substr(p_pos, next_comma == std::string::npos ? std::string::npos : next_comma - p_pos);
                if (!param.empty()) params.push_back(param);
                if (next_comma == std::string::npos) break;
                p_pos = next_comma + 1;
            }
            user_formulas.push_back({name, expr, params});
        }
        if (next_semi == std::string::npos) break;
        pos = next_semi + 1;
    }
    if (user_formulas.empty()) {
        user_formulas.push_back({"hypot", "sqrt(x^2 + y^2)", {"x", "y"}});
        user_formulas.push_back({"area", "pi * r^2", {"r"}});
    }
    
    preferences.end();
}

static void loadSDData() {
    if (!sd_initialized) return;
    
    // Load variables
    if (SD.exists("/Cardulator/variables.txt")) {
        File f = SD.open("/Cardulator/variables.txt", FILE_READ);
        if (f) {
            user_args.clear();
            while (f.available()) {
                String line = f.readStringUntil('\n');
                std::string s_line = line.c_str();
                s_line.erase(std::remove(s_line.begin(), s_line.end(), '\r'), s_line.end());
                s_line.erase(std::remove(s_line.begin(), s_line.end(), '\n'), s_line.end());
                if (s_line.empty()) continue;
                size_t eq = s_line.find('=');
                if (eq != std::string::npos) {
                    std::string name = s_line.substr(0, eq);
                    std::string val_str = s_line.substr(eq + 1);
                    try {
                        double val = std::stod(val_str);
                        user_args.push_back({name, val});
                    } catch (...) {}
                }
            }
            f.close();
        }
    }
    
    // Load binds
    if (SD.exists("/Cardulator/binds.txt")) {
        File f = SD.open("/Cardulator/binds.txt", FILE_READ);
        if (f) {
            user_binds.clear();
            while (f.available()) {
                String line = f.readStringUntil('\n');
                std::string s_line = line.c_str();
                s_line.erase(std::remove(s_line.begin(), s_line.end(), '\r'), s_line.end());
                s_line.erase(std::remove(s_line.begin(), s_line.end(), '\n'), s_line.end());
                if (s_line.empty()) continue;
                size_t col = s_line.find(':');
                if (col != std::string::npos && col > 0) {
                    char key = s_line[0];
                    std::string action = s_line.substr(col + 1);
                    user_binds.push_back({key, action});
                }
            }
            f.close();
        }
    }
    
    // Load scripts
    if (SD.exists("/Cardulator/scripts")) {
        File dir = SD.open("/Cardulator/scripts");
        if (dir && dir.isDirectory()) {
            user_scripts.clear();
            File file = dir.openNextFile();
            while (file) {
                if (!file.isDirectory()) {
                    std::string name = file.name();
                    size_t last_slash = name.find_last_of('/');
                    if (last_slash != std::string::npos) {
                        name = name.substr(last_slash + 1);
                    }
                    if (name.size() > 4 && name.substr(name.size() - 4) == ".txt") {
                        std::string script_name = name.substr(0, name.size() - 4);
                        String content = file.readString();
                        user_scripts.push_back({script_name, content.c_str()});
                    }
                }
                file = dir.openNextFile();
            }
            dir.close();
        }
    }
    
    // Load formulas
    if (SD.exists("/Cardulator/formulas.txt")) {
        File f = SD.open("/Cardulator/formulas.txt", FILE_READ);
        if (f) {
            user_formulas.clear();
            while (f.available()) {
                String line = f.readStringUntil('\n');
                std::string item = line.c_str();
                item.erase(std::remove(item.begin(), item.end(), '\r'), item.end());
                item.erase(std::remove(item.begin(), item.end(), '\n'), item.end());
                if (item.empty()) continue;
                size_t first_bar = item.find('|');
                size_t second_bar = (first_bar == std::string::npos) ? std::string::npos : item.find('|', first_bar + 1);
                if (first_bar != std::string::npos && second_bar != std::string::npos) {
                    std::string name = item.substr(0, first_bar);
                    std::string expr = item.substr(first_bar + 1, second_bar - first_bar - 1);
                    std::string params_str = item.substr(second_bar + 1);
                    
                    std::vector<std::string> params;
                    size_t p_pos = 0;
                    while (p_pos < params_str.size()) {
                        size_t next_comma = params_str.find(',', p_pos);
                        std::string param = params_str.substr(p_pos, next_comma == std::string::npos ? std::string::npos : next_comma - p_pos);
                        if (!param.empty()) params.push_back(param);
                        if (next_comma == std::string::npos) break;
                        p_pos = next_comma + 1;
                    }
                    user_formulas.push_back({name, expr, params});
                }
            }
            f.close();
        }
    }
    
    // Load constants
    if (SD.exists("/Cardulator/constants.txt")) {
        File f = SD.open("/Cardulator/constants.txt", FILE_READ);
        if (f) {
            user_consts.clear();
            while (f.available()) {
                String line = f.readStringUntil('\n');
                std::string s_line = line.c_str();
                s_line.erase(std::remove(s_line.begin(), s_line.end(), '\r'), s_line.end());
                s_line.erase(std::remove(s_line.begin(), s_line.end(), '\n'), s_line.end());
                if (s_line.empty()) continue;
                size_t eq = s_line.find('=');
                if (eq != std::string::npos) {
                    std::string name = s_line.substr(0, eq);
                    std::string val_str = s_line.substr(eq + 1);
                    try {
                        double val = std::stod(val_str);
                        user_consts.push_back({name, val});
                    } catch (...) {}
                }
            }
            f.close();
        }
    }
    if (user_consts.empty()) {
        user_consts.push_back({"pi", 3.141592653589793});
        user_consts.push_back({"e", 2.718281828459045});
    }

    // Load settings from SD
    if (SD.exists("/Cardulator/settings.txt")) {
        File f = SD.open("/Cardulator/settings.txt", FILE_READ);
        if (f) {
            while (f.available()) {
                String line = f.readStringUntil('\n');
                std::string s_line = line.c_str();
                s_line.erase(std::remove(s_line.begin(), s_line.end(), '\r'), s_line.end());
                s_line.erase(std::remove(s_line.begin(), s_line.end(), '\n'), s_line.end());
                if (s_line.empty()) continue;
                size_t eq = s_line.find('=');
                if (eq != std::string::npos) {
                    std::string key = s_line.substr(0, eq);
                    std::string val_str = s_line.substr(eq + 1);
                    try {
                        int val = std::stoi(val_str);
                        if (key == "scr_off") {
                            screen_off_timeout = val;
                        } else if (key == "brightness") {
                            backlight_brightness = val;
                        } else if (key == "thousands_sep") {
                            use_thousands_sep = (val != 0);
                        } else if (key == "auto_brackets") {
                            auto_brackets = (val != 0);
                        } else if (key == "sticky_mode") {
                            sticky_mode = (val != 0);
                        }
                    } catch (...) {}
                }
            }
            f.close();
        }
    }
}

static void saveNVSData() {
    preferences.begin("cardulator", false);
    
    // Serialize variables
    std::string vars_str = "";
    for (const auto& arg : user_args) {
        vars_str += arg.name + "=" + std::to_string(arg.val) + ";";
    }
    preferences.putString("vars", ::String(vars_str.c_str()));
    
    // Serialize functions
    std::string funcs_str = "";
    for (const auto& func : user_funcs) {
        funcs_str += func + ";";
    }
    preferences.putString("funcs", ::String(funcs_str.c_str()));
    
    // Serialize scripts
    std::string scr_list = "";
    for (const auto& scr : user_scripts) {
        scr_list += scr.name + ";";
        std::string key = "s_" + scr.name;
        preferences.putString(key.c_str(), ::String(scr.content.c_str()));
    }
    preferences.putString("scr_list", ::String(scr_list.c_str()));
    
    // Serialize binds
    std::string binds_str = "";
    for (const auto& b : user_binds) {
        binds_str += std::string(1, b.key) + ":" + b.action + ";";
    }
    preferences.putString("binds", ::String(binds_str.c_str()));
    
    // Serialize formulas
    std::string formulas_str = "";
    for (const auto& f : user_formulas) {
        std::string params_str = "";
        for (const auto& p : f.params) {
            params_str += p + ",";
        }
        formulas_str += f.name + "|" + f.expr + "|" + params_str + ";";
    }
    preferences.putString("formulas", ::String(formulas_str.c_str()));
    
    // Serialize constants
    std::string consts_str = "";
    for (const auto& cn : user_consts) {
        consts_str += cn.name + "=" + std::to_string(cn.val) + ";";
    }
    preferences.putString("consts", ::String(consts_str.c_str()));
    
    preferences.putInt("scr_off", screen_off_timeout);
    preferences.putInt("brightness", backlight_brightness);
    preferences.putBool("thousands_sep", use_thousands_sep);
    preferences.putBool("auto_brackets", auto_brackets);
    preferences.putBool("sticky_mode", sticky_mode);
    
    preferences.end();
}

static void sdWriteIfDifferent(const std::string& path, const std::string& new_content) {
    if (!sd_initialized) return;
    bool matches = false;
    if (SD.exists(path.c_str())) {
        File f = SD.open(path.c_str(), FILE_READ);
        if (f) {
            String old = f.readString();
            f.close();
            if (old.c_str() == new_content) {
                matches = true;
            }
        }
    }
    if (!matches) {
        File f = SD.open(path.c_str(), FILE_WRITE);
        if (f) {
            f.print(new_content.c_str());
            f.close();
        }
    }
}

static void saveSDData() {
    if (!sd_initialized) return;
    
    // Save variables
    std::string vars_str = "";
    for (const auto& arg : user_args) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s=%f\n", arg.name.c_str(), arg.val);
        vars_str += buf;
    }
    sdWriteIfDifferent("/Cardulator/variables.txt", vars_str);
    
    // Save constants
    std::string consts_str = "";
    for (const auto& cn : user_consts) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s=%f\n", cn.name.c_str(), cn.val);
        consts_str += buf;
    }
    sdWriteIfDifferent("/Cardulator/constants.txt", consts_str);
    
    // Save binds
    std::string binds_str = "";
    for (const auto& b : user_binds) {
        char buf[128];
        snprintf(buf, sizeof(buf), "%c:%s\n", b.key, b.action.c_str());
        binds_str += buf;
    }
    sdWriteIfDifferent("/Cardulator/binds.txt", binds_str);
    
    // Save scripts
    for (const auto& scr : user_scripts) {
        std::string path = "/Cardulator/scripts/" + scr.name + ".txt";
        sdWriteIfDifferent(path, scr.content);
    }
    
    // Save formulas
    std::string formulas_str = "";
    for (const auto& fm : user_formulas) {
        std::string params_str = "";
        for (const auto& p : fm.params) {
            params_str += p + ",";
        }
        char buf[256];
        snprintf(buf, sizeof(buf), "%s|%s|%s\n", fm.name.c_str(), fm.expr.c_str(), params_str.c_str());
        formulas_str += buf;
    }
    sdWriteIfDifferent("/Cardulator/formulas.txt", formulas_str);

    char settings_buf[256];
    snprintf(settings_buf, sizeof(settings_buf), "scr_off=%d\nbrightness=%d\nthousands_sep=%d\nauto_brackets=%d\nsticky_mode=%d\n",
             screen_off_timeout, backlight_brightness, use_thousands_sep ? 1 : 0, auto_brackets ? 1 : 0, sticky_mode ? 1 : 0);
    sdWriteIfDifferent("/Cardulator/settings.txt", settings_buf);
}

static void saveNVSDataWrapper() {
    if (sd_initialized) {
        saveSDData();
    } else {
        saveNVSData();
    }
}

#define saveNVSData saveNVSDataWrapper

#else
static void loadNVSData() {}
static void saveNVSData() {}
static void saveSDData() {}
#endif

// Script Interpreter Implementation provided in math_engine.h





    




#ifdef ARDUINO
static M5Canvas canvas(&M5Cardputer.Display);
#endif

static void drawHighlightedExpression(const std::string& expr) {
    int n = expr.size();
    if (select_all_active && n > 0) {
        M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLUE);
        if (n > 19) {
            M5Cardputer.Display.setCursor(2, 105);
            M5Cardputer.Display.print(expr.substr(0, 19).c_str());
            M5Cardputer.Display.setCursor(2, 118);
            M5Cardputer.Display.print(expr.substr(19).c_str());
        } else {
            M5Cardputer.Display.setCursor(2, 118);
            M5Cardputer.Display.print(expr.c_str());
        }
        M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        return;
    }
    
    if (n > 19) {
        M5Cardputer.Display.setCursor(2, 105);
    } else {
        M5Cardputer.Display.setCursor(2, 118);
    }
    
    // Calculate parenthesis/brace/bracket nesting levels globally to keep colors consistent
    std::vector<int> bracket_levels(n, 0);
    int current_level = 0;
    for (int i = 0; i < n; ++i) {
        if (expr[i] == '(' || expr[i] == '{' || expr[i] == '[') {
            bracket_levels[i] = current_level;
            current_level++;
        } else if (expr[i] == ')' || expr[i] == '}' || expr[i] == ']') {
            current_level--;
            bracket_levels[i] = current_level;
        }
    }
    
    uint16_t rainbow[] = { TFT_CYAN, TFT_MAGENTA, TFT_YELLOW, 0x07E0, 0xFDA0 };
    int num_colors = 5;
    
    M5Cardputer.Display.setTextColor(TFT_WHITE);
    for (int i = 0; i < n; ) {
        if (i == 19) {
            M5Cardputer.Display.setCursor(2, 118);
        }
        
        if (i == cursor_pos && !select_all_active) {
            int cx = M5Cardputer.Display.getCursorX();
            M5Cardputer.Display.drawFastVLine(cx > 0 ? cx - 1 : 0, M5Cardputer.Display.getCursorY(), 16, TFT_CYAN);
        }
        
        if (expr[i] == '(' || expr[i] == '{' || expr[i] == '[') {
            int lvl = bracket_levels[i];
            uint16_t color = (lvl >= 0) ? rainbow[lvl % num_colors] : TFT_RED;
            M5Cardputer.Display.setTextColor(color);
            M5Cardputer.Display.print(expr[i]);
            M5Cardputer.Display.setTextColor(TFT_WHITE);
            i++;
        } else if (expr[i] == ')' || expr[i] == '}' || expr[i] == ']') {
            int lvl = bracket_levels[i];
            uint16_t color = (lvl >= 0) ? rainbow[lvl % num_colors] : TFT_RED;
            M5Cardputer.Display.setTextColor(color);
            M5Cardputer.Display.print(expr[i]);
            M5Cardputer.Display.setTextColor(TFT_WHITE);
            i++;
        } else {
            bool colored = false;
            char c = expr[i];
            
            // Check prefix
            std::string prefixes = "hkMGTPdcmunpfazyEZ";
            if (c == 'Y' || prefixes.find(c) != std::string::npos) {
                if (i > 0 && (std::isdigit(expr[i-1]) || expr[i-1] == '.')) {
                    M5Cardputer.Display.setTextColor(TFT_YELLOW);
                    M5Cardputer.Display.print(c);
                    M5Cardputer.Display.setTextColor(TFT_WHITE);
                    colored = true;
                    i++;
                }
            }
            
            if (!colored) {
                if ((c == 'e' || c == 'E') && i > 0 && std::isdigit(expr[i-1])) {
                    M5Cardputer.Display.setTextColor(TFT_YELLOW);
                    M5Cardputer.Display.print(c);
                    M5Cardputer.Display.setTextColor(TFT_WHITE);
                    colored = true;
                    i++;
                }
            }
            
            if (!colored && (std::isalpha(c) || c == '_')) {
                int start_i = i;
                std::string id = "";
                while (i < n && (std::isalnum(expr[i]) || expr[i] == '_')) {
                    id += expr[i];
                    i++;
                }
                
                bool is_const = false;
                if (id == "pi" || id == "e") {
                    is_const = true;
                } else {
                    for (const auto& cn : user_consts) {
                        if (cn.name == id) {
                            is_const = true;
                            break;
                        }
                    }
                }
                
                bool is_var = false;
                if (!is_const) {
                    if (id[0] == 'e' && id.size() > 1 && std::all_of(id.begin() + 1, id.end(), ::isdigit)) {
                        is_var = true;
                    } else {
                        for (const auto& arg : user_args) {
                            if (arg.name == id) {
                                is_var = true;
                                break;
                            }
                        }
                    }
                }
                
                if (is_const) {
                    M5Cardputer.Display.setTextColor(TFT_MAGENTA);
                } else if (is_var) {
                    M5Cardputer.Display.setTextColor(TFT_CYAN);
                } else {
                    M5Cardputer.Display.setTextColor(TFT_WHITE);
                }
                
                for (size_t k = 0; k < id.size(); ++k) {
                    if ((start_i + (int)k) == cursor_pos && !select_all_active) {
                        int cx = M5Cardputer.Display.getCursorX();
                        M5Cardputer.Display.drawFastVLine(cx > 0 ? cx - 1 : 0, M5Cardputer.Display.getCursorY(), 16, TFT_CYAN);
                    }
                    M5Cardputer.Display.print(id[k]);
                }
                M5Cardputer.Display.setTextColor(TFT_WHITE);
                colored = true;
            }
            
            if (!colored) {
                if (std::isdigit(c) || c == '.') {
                    M5Cardputer.Display.print(c);
                } else {
                    M5Cardputer.Display.print(c);
                }
                i++;
            }
        }
    }
    
    if (cursor_pos == n && !select_all_active) {
        if (n == 19) {
            M5Cardputer.Display.setCursor(2, 118);
        }
        int cx = M5Cardputer.Display.getCursorX();
        M5Cardputer.Display.drawFastVLine(cx > 0 ? cx - 1 : 0, M5Cardputer.Display.getCursorY(), 16, TFT_CYAN);
    }
}

static void drawSyntaxHighlightedText(const std::string& expr, int cur_pos, int start_x, int start_y) {
    int n = expr.size();
    M5Cardputer.Display.setCursor(start_x, start_y);
    
    std::vector<int> bracket_levels(n, 0);
    int current_level = 0;
    for (int i = 0; i < n; ++i) {
        if (expr[i] == '(' || expr[i] == '{' || expr[i] == '[') {
            bracket_levels[i] = current_level;
            current_level++;
        } else if (expr[i] == ')' || expr[i] == '}' || expr[i] == ']') {
            current_level--;
            bracket_levels[i] = current_level;
        }
    }
    
    uint16_t rainbow[] = { TFT_CYAN, TFT_MAGENTA, TFT_YELLOW, 0x07E0, 0xFDA0 };
    int num_colors = 5;
    
    M5Cardputer.Display.setTextColor(TFT_WHITE);
    for (int i = 0; i < n; ) {
        if (i == cur_pos) {
            int cx = M5Cardputer.Display.getCursorX();
            M5Cardputer.Display.drawFastVLine(cx > 0 ? cx - 1 : 0, M5Cardputer.Display.getCursorY(), 12, TFT_CYAN);
        }
        
        if (expr[i] == '(' || expr[i] == '{' || expr[i] == '[') {
            int lvl = bracket_levels[i];
            uint16_t color = (lvl >= 0) ? rainbow[lvl % num_colors] : TFT_RED;
            M5Cardputer.Display.setTextColor(color);
            M5Cardputer.Display.print(expr[i]);
            M5Cardputer.Display.setTextColor(TFT_WHITE);
            i++;
        } else if (expr[i] == ')' || expr[i] == '}' || expr[i] == ']') {
            int lvl = bracket_levels[i];
            uint16_t color = (lvl >= 0) ? rainbow[lvl % num_colors] : TFT_RED;
            M5Cardputer.Display.setTextColor(color);
            M5Cardputer.Display.print(expr[i]);
            M5Cardputer.Display.setTextColor(TFT_WHITE);
            i++;
        } else {
            bool colored = false;
            char c = expr[i];
            
            std::string prefixes = "hkMGTPdcmunpfazyEZ";
            if (c == 'Y' || prefixes.find(c) != std::string::npos) {
                if (i > 0 && (std::isdigit(expr[i-1]) || expr[i-1] == '.')) {
                    M5Cardputer.Display.setTextColor(TFT_YELLOW);
                    M5Cardputer.Display.print(c);
                    M5Cardputer.Display.setTextColor(TFT_WHITE);
                    colored = true;
                    i++;
                }
            }
            
            if (!colored) {
                if ((c == 'e' || c == 'E') && i > 0 && std::isdigit(expr[i-1])) {
                    M5Cardputer.Display.setTextColor(TFT_YELLOW);
                    M5Cardputer.Display.print(c);
                    M5Cardputer.Display.setTextColor(TFT_WHITE);
                    colored = true;
                    i++;
                }
            }
            
            if (!colored && (std::isalpha(c) || c == '_')) {
                int start_i = i;
                std::string id = "";
                while (i < n && (std::isalnum(expr[i]) || expr[i] == '_')) {
                    id += expr[i];
                    i++;
                }
                
                bool is_const = (id == "pi" || id == "e");
                bool is_var = false;
                if (!is_const) {
                    if (id[0] == 'e' && id.size() > 1 && std::all_of(id.begin() + 1, id.end(), ::isdigit)) {
                        is_var = true;
                    }
                }
                
                if (is_const) {
                    M5Cardputer.Display.setTextColor(TFT_MAGENTA);
                } else if (is_var) {
                    M5Cardputer.Display.setTextColor(TFT_CYAN);
                } else {
                    M5Cardputer.Display.setTextColor(TFT_WHITE);
                }
                
                for (size_t k = 0; k < id.size(); ++k) {
                    if ((start_i + (int)k) == cur_pos) {
                        int cx = M5Cardputer.Display.getCursorX();
                        M5Cardputer.Display.drawFastVLine(cx > 0 ? cx - 1 : 0, M5Cardputer.Display.getCursorY(), 12, TFT_CYAN);
                    }
                    M5Cardputer.Display.print(id[k]);
                }
                M5Cardputer.Display.setTextColor(TFT_WHITE);
                colored = true;
            }
            
            if (!colored) {
                if (std::isdigit(c) || c == '.') {
                    M5Cardputer.Display.setTextColor(TFT_YELLOW);
                    M5Cardputer.Display.print(c);
                    M5Cardputer.Display.setTextColor(TFT_WHITE);
                } else {
                    M5Cardputer.Display.print(c);
                }
                i++;
            }
        }
    }
    
    if (cur_pos == n) {
        int cx = M5Cardputer.Display.getCursorX();
        M5Cardputer.Display.drawFastVLine(cx > 0 ? cx - 1 : 0, M5Cardputer.Display.getCursorY(), 12, TFT_CYAN);
    }
}

static void drawStatusBar(const std::string& mode) {
    M5Cardputer.Display.fillRect(0, 0, SCR_W, 12, TFT_BLACK);
    M5Cardputer.Display.setTextColor(TFT_CYAN);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setCursor(2, 2);
    M5Cardputer.Display.print(mode.c_str());
    M5Cardputer.Display.drawLine(0, 11, SCR_W, 11, TFT_DARKGREY);

    // Draw battery status on the right (without percentages)
    int bat = M5Cardputer.Power.getBatteryLevel();
    if (bat < 0) bat = 0;
    if (bat > 100) bat = 100;

    int batX = SCR_W - 22;
    int batY = 2;
    int batW = 16;
    int batH = 8;
    M5Cardputer.Display.drawRoundRect(batX, batY, batW, batH, 1, TFT_WHITE);
    M5Cardputer.Display.drawRect(batX + batW, batY + 2, 2, 4, TFT_WHITE);

    int fillW = (bat * 12) / 100;
    if (fillW > 0) {
        uint16_t color = (bat < 20) ? TFT_RED : TFT_GREEN;
        M5Cardputer.Display.fillRect(batX + 2, batY + 2, fillW, 4, color);
    }

    int boltX = batX - 8;
    if (M5Cardputer.Power.isCharging() == m5::Power_Class::is_charging) {
        M5Cardputer.Display.drawLine(boltX + 2, batY, boltX + 4, batY + 4, TFT_YELLOW);
        M5Cardputer.Display.drawLine(boltX + 4, batY + 4, boltX + 1, batY + 4, TFT_YELLOW);
        M5Cardputer.Display.drawLine(boltX + 1, batY + 4, boltX + 3, batY + 8, TFT_YELLOW);
    }

    // Draw active modifier badges to the left of battery
    int badgeRightX = batX - 4;
    if (M5Cardputer.Power.isCharging() == m5::Power_Class::is_charging) {
        badgeRightX = boltX - 4;
    }

    auto drawBadge = [&](const char* label, uint16_t bg_color, uint16_t text_color) {
        int label_len = strlen(label);
        int badge_w = label_len * 6 + 3;
        int badge_x = badgeRightX - badge_w;
        if (badge_x > 80) {
            M5Cardputer.Display.fillRect(badge_x, 1, badge_w, 9, bg_color);
            M5Cardputer.Display.setTextColor(text_color);
            M5Cardputer.Display.setCursor(badge_x + 2, 2);
            M5Cardputer.Display.print(label);
            badgeRightX = badge_x - 3;
        }
    };

    if (sticky_fn_active) drawBadge("fn", 0xF800 /* Red */, TFT_WHITE);
    if (sticky_shift_active) drawBadge("Aa", 0x001F /* Blue */, TFT_WHITE);
    if (sticky_opt_active) drawBadge("opt", 0x07FF /* Cyan */, TFT_BLACK);
    if (sticky_ctrl_active) drawBadge("ctrl", 0x7BEF /* Grey */, TFT_WHITE);
    if (sticky_alt_active) drawBadge("alt", 0x7BEF /* Grey */, TFT_WHITE);
}

static std::vector<std::string> wrapSingleTextLine(const std::string& line, size_t max_chars) {
    std::vector<std::string> result;
    if (line.size() <= max_chars) {
        result.push_back(line);
    } else {
        size_t start = 0;
        while (start < line.size()) {
            size_t len = std::min(max_chars, line.size() - start);
            result.push_back(line.substr(start, len));
            start += len;
        }
    }
    return result;
}

static std::vector<std::string> wrapTextLines(const std::vector<std::string>& input, size_t max_chars) {
    std::vector<std::string> result;
    for (const auto& line : input) {
        if (line.size() <= max_chars) {
            result.push_back(line);
        } else {
            size_t start = 0;
            while (start < line.size()) {
                size_t len = std::min(max_chars, line.size() - start);
                result.push_back(line.substr(start, len));
                start += len;
            }
        }
    }
    return result;
}

static int help_popup_scroll_offset = 0;

static void drawHelpPopup() {
    int w = 210;
    int h = 105;
    int x = (SCR_W - w) / 2;
    int y = (SCR_H - h) / 2;
    
    M5Cardputer.Display.fillRect(x, y, w, h, 0x18E3 /* dark grey */);
    M5Cardputer.Display.drawRect(x, y, w, h, TFT_WHITE);
    M5Cardputer.Display.drawRect(x + 1, y + 1, w - 2, h - 2, TFT_CYAN);
    
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(TFT_YELLOW);
    M5Cardputer.Display.setCursor(x + 8, y + 6);
    
    std::string title;
    std::vector<std::string> raw_lines;
    getHelpPopupData(appState, script_edit_mode, title, raw_lines);
    std::vector<std::string> lines = wrapTextLines(raw_lines, 31);

    if ((int)lines.size() > 5) {
        if (help_popup_scroll_offset > (int)lines.size() - 5) {
            help_popup_scroll_offset = std::max(0, (int)lines.size() - 5);
        }
        title += " (; . ^ v)";
    } else {
        help_popup_scroll_offset = 0;
    }
    
    M5Cardputer.Display.print(title.c_str());
    M5Cardputer.Display.drawLine(x, y + 18, x + w, y + 18, TFT_DARKGREY);
    
    int lineY = y + 22;
    M5Cardputer.Display.setTextColor(TFT_WHITE);
    for (int i = help_popup_scroll_offset; i < (int)lines.size() && (i - help_popup_scroll_offset) < 5; ++i) {
        M5Cardputer.Display.setCursor(x + 8, lineY);
        M5Cardputer.Display.print(lines[i].c_str());
        lineY += 15;
    }
}

static void drawCalc() {
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    
    std::string modeStr = "Cardulator";
    if (history.size() > 0) {
        modeStr += " | e" + std::to_string(history.size() + 1);
    }
    drawStatusBar(modeStr);

    // 1. Draw scrollable REPL history viewport
    int line_height = 12;
    int max_history_lines = 7;
    int y_start = 100;
    if (expression.size() > 19) {
        y_start = 88;
        max_history_lines = 6;
    }
    
    struct REPLRenderLine {
        std::string text;
        uint16_t color;
    };
    std::vector<REPLRenderLine> flat_repl_lines;
    
    for (size_t i = 0; i < expr_history.size(); ++i) {
        // 1. Expression line (wrapped)
        std::vector<std::string> expr_lines = wrapSingleTextLine("> " + expr_history[i], 38);
        for (const auto& eline : expr_lines) {
            flat_repl_lines.push_back({eline, TFT_DARKGREY});
        }
        
        // 2. Result lines (split by \n and wrapped)
        if (i < history_results.size() && !history_results[i].empty()) {
            std::string res = history_results[i];
            size_t p = 0;
            std::vector<std::string> raw_res_lines;
            while (p < res.size()) {
                size_t nl = res.find('\n', p);
                std::string lstr = (nl == std::string::npos) ? res.substr(p) : res.substr(p, nl - p);
                raw_res_lines.push_back(lstr);
                if (nl == std::string::npos) break;
                p = nl + 1;
            }
            std::vector<std::string> wrapped_res_lines = wrapTextLines(raw_res_lines, 38);
            for (const auto& rline : wrapped_res_lines) {
                flat_repl_lines.push_back({rline, 0x07E0 /* green */});
            }
        }
    }
    
    int total_h_lines = (int)flat_repl_lines.size();
    if (history_scroll_offset > total_h_lines - 1) {
        history_scroll_offset = std::max(0, total_h_lines - 1);
    }
    if (history_scroll_offset < 0) {
        history_scroll_offset = 0;
    }
    
    int start_from = total_h_lines - 1 - history_scroll_offset;
    M5Cardputer.Display.setTextSize(1);
    
    for (int k = 0; k < max_history_lines; ++k) {
        int idx = start_from - (max_history_lines - 1 - k);
        if (idx >= 0 && idx < total_h_lines) {
            int draw_y = y_start - (max_history_lines - 1 - k) * line_height;
            M5Cardputer.Display.setCursor(2, draw_y);
            M5Cardputer.Display.setTextColor(flat_repl_lines[idx].color);
            M5Cardputer.Display.print(flat_repl_lines[idx].text.c_str());
        }
    }

    // 2. Draw separators and input field at the bottom
    if (expression.size() > 19) {
        M5Cardputer.Display.drawLine(0, 101, SCR_W, 101, TFT_DARKGREY);
    } else {
        M5Cardputer.Display.drawLine(0, 114, SCR_W, 114, TFT_DARKGREY);
    }

    M5Cardputer.Display.setTextSize(EXPR_SIZE);
    if (hasError) {
        M5Cardputer.Display.setTextColor(TFT_RED);
        std::string err_disp = resultStr;
        if (err_disp.size() > 19) {
            M5Cardputer.Display.setCursor(2, 105);
            M5Cardputer.Display.print(err_disp.substr(0, 19).c_str());
            M5Cardputer.Display.setCursor(2, 118);
            M5Cardputer.Display.print(err_disp.substr(19).c_str());
        } else {
            M5Cardputer.Display.setCursor(2, 118);
            M5Cardputer.Display.print(err_disp.c_str());
        }
    } else {
        M5Cardputer.Display.setCursor(2, 118);
        drawHighlightedExpression(expression);
    }

    // Commented out standalone result line to keep input line at bottom:
    // M5Cardputer.Display.setTextSize(2);
    // M5Cardputer.Display.setTextColor(hasError ? TFT_RED : TFT_GREEN);
    // std::string res = resultStr;
    // if (res.size() > 18) res = res.substr(0, 18) + "..";
    // M5Cardputer.Display.setCursor(2, 115);
    // M5Cardputer.Display.print(res.c_str());
}

static void drawHelpView() {
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    std::string title;
    std::vector<std::string> left_col, right_col;
    getHelpViewData(title, left_col, right_col);
    
    drawStatusBar(title.c_str());
    M5Cardputer.Display.setTextSize(1);
    int y = 15;
    
    for (size_t i = 0; i < left_col.size(); ++i) {
        M5Cardputer.Display.setCursor(2, y + (int)i * 10);
        if (i == 0) M5Cardputer.Display.setTextColor(TFT_CYAN);
        else M5Cardputer.Display.setTextColor(TFT_WHITE);
        M5Cardputer.Display.print(left_col[i].c_str());
    }
    
    for (size_t i = 0; i < right_col.size(); ++i) {
        M5Cardputer.Display.setCursor(130, y + (int)i * 10);
        if (i == 0) M5Cardputer.Display.setTextColor(TFT_CYAN);
        else M5Cardputer.Display.setTextColor(TFT_WHITE);
        M5Cardputer.Display.print(right_col[i].c_str());
    }
}

static void drawConfirmModal(const char* title, int selected_idx) {
    int popup_w = 180;
    int popup_h = 52;
    int popup_x = (SCR_W - popup_w) / 2;
    int popup_y = (SCR_H - popup_h) / 2;
    M5Cardputer.Display.fillRect(popup_x, popup_y, popup_w, popup_h, 0x18E3 /* dark grey */);
    M5Cardputer.Display.drawRect(popup_x, popup_y, popup_w, popup_h, TFT_WHITE);
    M5Cardputer.Display.drawRect(popup_x + 1, popup_y + 1, popup_w - 2, popup_h - 2, TFT_CYAN);
    
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(TFT_WHITE);
    M5Cardputer.Display.setCursor(popup_x + (popup_w - strlen(title) * 6) / 2, popup_y + 10);
    M5Cardputer.Display.print(title);

    if (selected_idx == 0) {
        M5Cardputer.Display.setTextColor(TFT_YELLOW, TFT_BLUE);
        M5Cardputer.Display.setCursor(popup_x + 35, popup_y + 32);
        M5Cardputer.Display.print(" [ Yes ] ");
        M5Cardputer.Display.setTextColor(TFT_WHITE, 0x18E3);
        M5Cardputer.Display.setCursor(popup_x + 105, popup_y + 32);
        M5Cardputer.Display.print("   No   ");
    } else {
        M5Cardputer.Display.setTextColor(TFT_WHITE, 0x18E3);
        M5Cardputer.Display.setCursor(popup_x + 35, popup_y + 32);
        M5Cardputer.Display.print("   Yes  ");
        M5Cardputer.Display.setTextColor(TFT_YELLOW, TFT_BLUE);
        M5Cardputer.Display.setCursor(popup_x + 105, popup_y + 32);
        M5Cardputer.Display.print(" [ No ] ");
    }
    M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
}

static void drawVars() {
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    drawStatusBar("Cardulator | VARIABLES");
    
    int y = 15;
    M5Cardputer.Display.setTextSize(1);
    
    size_t total_vars = history.size() + user_args.size();
    
    if (total_vars == 0) {
        M5Cardputer.Display.setTextColor(TFT_DARKGREY);
        M5Cardputer.Display.setCursor(10, y + 20);
        M5Cardputer.Display.print("No variables. Press 'N' to create.");
    } else {
        int start = 0;
        if (var_selected_idx >= 6) {
            start = var_selected_idx - 5;
        }
        for (int i = start; i < (int)total_vars && i < start + 6; ++i) {
            std::string name;
            double val = 0.0;
            if (i < (int)history.size()) {
                name = "e" + std::to_string(i + 1);
                val = history[i];
            } else {
                size_t u_idx = i - history.size();
                name = user_args[u_idx].name;
                val = user_args[u_idx].val;
            }
            if (i == var_selected_idx) {
                M5Cardputer.Display.fillRect(0, y + (i - start) * 14 - 1, SCR_W, 13, TFT_BLUE);
                M5Cardputer.Display.setTextColor(TFT_WHITE);
            } else {
                M5Cardputer.Display.setTextColor(TFT_WHITE);
            }
            M5Cardputer.Display.setCursor(5, y + (i - start) * 14);
            char line[64];
            snprintf(line, sizeof(line), "%s = %s", name.c_str(), fmtNum(val).c_str());
            M5Cardputer.Display.print(line);
        }
    }
    
    if (var_edit_mode) {
        M5Cardputer.Display.drawLine(0, 110, SCR_W, 110, TFT_DARKGREY);
        M5Cardputer.Display.setCursor(5, 115);
        M5Cardputer.Display.setTextColor(TFT_YELLOW);
        M5Cardputer.Display.print("Edit value: ");
        M5Cardputer.Display.print(var_edit_buf.c_str());
    }

    if (delete_confirm_prompt_mode) {
        drawConfirmModal("Delete item?", delete_confirm_selected_idx);
    }
}

static int const_selected_idx = 0;
static bool const_edit_mode = false;
static std::string const_edit_buf = "";

static void drawConsts() {
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    drawStatusBar("Cardulator | CONSTANTS");
    
    int y = 15;
    M5Cardputer.Display.setTextSize(1);
    
    if (user_consts.empty()) {
        M5Cardputer.Display.setTextColor(TFT_DARKGREY);
        M5Cardputer.Display.setCursor(10, y + 20);
        M5Cardputer.Display.print("No constants. Press 'N' to create.");
    } else {
        int start = 0;
        if (const_selected_idx >= 6) {
            start = const_selected_idx - 5;
        }
        for (int i = start; i < (int)user_consts.size() && i < start + 6; ++i) {
            if (i == const_selected_idx) {
                M5Cardputer.Display.fillRect(0, y + (i - start) * 14 - 1, SCR_W, 13, TFT_BLUE);
                M5Cardputer.Display.setTextColor(TFT_WHITE);
            } else {
                M5Cardputer.Display.setTextColor(TFT_WHITE);
            }
            M5Cardputer.Display.setCursor(5, y + (i - start) * 14);
            char line[64];
            snprintf(line, sizeof(line), "%s = %s", user_consts[i].name.c_str(), fmtNum(user_consts[i].val).c_str());
            M5Cardputer.Display.print(line);
        }
    }
    
    if (const_edit_mode) {
        M5Cardputer.Display.drawLine(0, 110, SCR_W, 110, TFT_DARKGREY);
        M5Cardputer.Display.setCursor(5, 115);
        M5Cardputer.Display.setTextColor(TFT_YELLOW);
        M5Cardputer.Display.print("Edit value: ");
        M5Cardputer.Display.print(const_edit_buf.c_str());
    }

    if (delete_confirm_prompt_mode) {
        drawConfirmModal("Delete constant?", delete_confirm_selected_idx);
    }
}

static void drawBinds() {
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    drawStatusBar("Cardulator | BINDS");
    
    int y = 15;
    M5Cardputer.Display.setTextSize(1);
    
    if (user_binds.empty()) {
        M5Cardputer.Display.setTextColor(TFT_DARKGREY);
        M5Cardputer.Display.setCursor(10, y + 20);
        M5Cardputer.Display.print("No binds. Press 'N' to create.");
    } else {
        int start = 0;
        if (bind_selected_idx >= 6) {
            start = bind_selected_idx - 5;
        }
        for (int i = start; i < (int)user_binds.size() && i < start + 6; ++i) {
            if (i == bind_selected_idx) {
                M5Cardputer.Display.fillRect(0, y + (i - start) * 14 - 1, SCR_W, 13, TFT_BLUE);
                M5Cardputer.Display.setTextColor(TFT_WHITE);
            } else {
                M5Cardputer.Display.setTextColor(TFT_WHITE);
            }
            M5Cardputer.Display.setCursor(5, y + (i - start) * 14);
            char line[64];
            snprintf(line, sizeof(line), "Alt + %c  =>  %s", user_binds[i].key, user_binds[i].action.c_str());
            M5Cardputer.Display.print(line);
        }
    }
    
    if (bind_edit_mode) {
        M5Cardputer.Display.drawLine(0, 110, SCR_W, 110, TFT_DARKGREY);
        M5Cardputer.Display.setCursor(5, 115);
        M5Cardputer.Display.setTextColor(TFT_YELLOW);
        M5Cardputer.Display.print("Edit action: ");
        M5Cardputer.Display.print(bind_edit_buf.c_str());
    }

    if (delete_confirm_prompt_mode) {
        drawConfirmModal("Delete bind?", delete_confirm_selected_idx);
    }
}

static void drawFormulas() {
    M5Cardputer.Display.fillScreen(TFT_BLACK);

    if (formula_edit_mode) {
        if (formula_exit_prompt_mode) {
            drawConfirmModal("Save modified formula?", formula_exit_selected_idx);
            return;
        }

        drawStatusBar("Edit Formula (Max 4 args)");
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setTextColor(TFT_WHITE);
        M5Cardputer.Display.setCursor(10, 25);
        M5Cardputer.Display.print("Format: f(a,b)=a^2+b");
        M5Cardputer.Display.setCursor(10, 45);
        M5Cardputer.Display.setTextColor(TFT_YELLOW);
        M5Cardputer.Display.print("> ");
        drawSyntaxHighlightedText(formula_edit_buf, formula_cursor_pos, M5Cardputer.Display.getCursorX(), M5Cardputer.Display.getCursorY());
        return;
    }
    
    if (formula_create_mode) {
        drawStatusBar("New Formula (Max 4 args)");
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setTextColor(TFT_WHITE);
        M5Cardputer.Display.setCursor(10, 25);
        M5Cardputer.Display.print("Format: f(a,b)=a^2+b");
        M5Cardputer.Display.setCursor(10, 45);
        M5Cardputer.Display.setTextColor(TFT_YELLOW);
        M5Cardputer.Display.print("> ");
        drawSyntaxHighlightedText(formula_create_buf, formula_cursor_pos, M5Cardputer.Display.getCursorX(), M5Cardputer.Display.getCursorY());
        return;
    }
    
    if (formula_wizard_mode && formula_selected_idx >= 0 && formula_selected_idx < (int)user_formulas.size()) {
        const auto& f = user_formulas[formula_selected_idx];
        drawStatusBar("Formula: " + f.name);
        
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setTextColor(TFT_CYAN);
        M5Cardputer.Display.setCursor(5, 16);
        M5Cardputer.Display.print("Expr: ");
        M5Cardputer.Display.setTextColor(TFT_WHITE);
        M5Cardputer.Display.print(f.expr.c_str());
        
        int y_start = 30;
        int p_count = std::min(4, (int)f.params.size());
        for (int i = 0; i < p_count; ++i) {
            int y = y_start + i * 14;
            if (i == formula_wizard_param_idx) {
                M5Cardputer.Display.fillRect(0, y - 1, SCR_W, 13, 0x0015 /* dark blue */);
                M5Cardputer.Display.setTextColor(TFT_YELLOW);
                M5Cardputer.Display.setCursor(5, y);
                M5Cardputer.Display.print("-> ");
            } else {
                M5Cardputer.Display.setTextColor(TFT_WHITE);
                M5Cardputer.Display.setCursor(5, y);
                M5Cardputer.Display.print("   ");
            }
            M5Cardputer.Display.print(f.params[i].c_str());
            M5Cardputer.Display.print(" = ");
            if (i < (int)formula_wizard_bufs.size()) {
                M5Cardputer.Display.print(formula_wizard_bufs[i].c_str());
                if (i == formula_wizard_param_idx) {
                    M5Cardputer.Display.drawFastVLine(M5Cardputer.Display.getCursorX(), M5Cardputer.Display.getCursorY(), 10, TFT_CYAN);
                }
            }
        }

        // Bottom Result Card (only shown when user presses Enter to compute result)
        if (formula_wizard_has_result) {
            int card_y = 108;
            int card_h = 24;
            int card_w = SCR_W - 8;
            int card_x = 4;
            M5Cardputer.Display.fillRect(card_x, card_y, card_w, card_h, 0x10A2 /* dark navy/grey */);
            M5Cardputer.Display.drawRect(card_x, card_y, card_w, card_h, TFT_CYAN);
            
            M5Cardputer.Display.setCursor(card_x + 6, card_y + 7);
            M5Cardputer.Display.setTextColor(TFT_GREEN);
            M5Cardputer.Display.print("Result: ");
            M5Cardputer.Display.setTextColor(TFT_YELLOW);
            M5Cardputer.Display.print(formula_wizard_result_str.c_str());
        }
        
        return;
    }
    
    // Normal Formulas List View
    drawStatusBar("Cardulator | FORMULAS");
    int y = 15;
    M5Cardputer.Display.setTextSize(1);
    
    if (user_formulas.empty()) {
        M5Cardputer.Display.setTextColor(TFT_DARKGREY);
        M5Cardputer.Display.setCursor(10, y + 20);
        M5Cardputer.Display.print("No formulas available.");
        M5Cardputer.Display.setCursor(10, y + 40);
        M5Cardputer.Display.setTextColor(TFT_YELLOW);
        M5Cardputer.Display.print("Press [N] to create a formula.");
    } else {
        int start = 0;
        if (formula_selected_idx >= 5) {
            start = formula_selected_idx - 4;
        }
        for (int i = start; i < (int)user_formulas.size() && i < start + 5; ++i) {
            int line_y = y + (i - start) * 14;
            if (i == formula_selected_idx) {
                M5Cardputer.Display.fillRect(0, line_y - 1, SCR_W, 13, TFT_BLUE);
                M5Cardputer.Display.setTextColor(TFT_YELLOW);
            } else {
                M5Cardputer.Display.setTextColor(TFT_WHITE);
            }
            M5Cardputer.Display.setCursor(5, line_y);
            char line_buf[128];
            std::string params_str = "";
            for (size_t p = 0; p < user_formulas[i].params.size() && p < 4; ++p) {
                params_str += user_formulas[i].params[p] + ",";
            }
            if (!params_str.empty()) params_str.pop_back();
            snprintf(line_buf, sizeof(line_buf), "%s(%s) = %s", user_formulas[i].name.c_str(), params_str.c_str(), user_formulas[i].expr.c_str());
            M5Cardputer.Display.print(line_buf);
        }
        
        if (delete_confirm_prompt_mode) {
            drawConfirmModal("Delete formula?", delete_confirm_selected_idx);
        }
    }
}

inline void drawDashedLine(int x0, int y0, int x1, int y1, uint16_t color, int dash_len, int gap_len) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    int count = 0;
    int cycle = dash_len + gap_len;
    while (true) {
        if (count % cycle < dash_len) {
            M5Cardputer.Display.drawPixel(x0, y0, color);
        }
        count++;
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

inline void drawDashDotLine(int x0, int y0, int x1, int y1, uint16_t color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    int count = 0;
    while (true) {
        int mod = count % 12;
        if (mod < 5 || mod == 8) {
            M5Cardputer.Display.drawPixel(x0, y0, color);
        }
        count++;
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void drawPlot() {
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    
    char status[64];
    snprintf(status, sizeof(status), "Plot | Lines: %d | Zoom: %.1f", (int)plot_lines.size(), 1.0 / plot_scale);
    drawStatusBar(status);
    
    double min_x = std::numeric_limits<double>::max();
    double max_x = std::numeric_limits<double>::lowest();
    double min_y = std::numeric_limits<double>::max();
    double max_y = std::numeric_limits<double>::lowest();
    
    if (plot_manual_limits) {
        min_x = plot_xlim_min;
        max_x = plot_xlim_max;
        min_y = plot_ylim_min;
        max_y = plot_ylim_max;
    } else {
        bool has_points = false;
        for (const auto& line : plot_lines) {
            for (size_t i = 0; i < line.x.size() && i < line.y.size(); ++i) {
                double xv = line.x[i];
                double yv = line.y[i];
                if (std::isnan(xv) || std::isinf(xv) || std::isnan(yv) || std::isinf(yv)) continue;
                if (xv < min_x) min_x = xv;
                if (xv > max_x) max_x = xv;
                if (yv < min_y) min_y = yv;
                if (yv > max_y) max_y = yv;
                has_points = true;
            }
        }
        if (!has_points) {
            min_x = -10.0; max_x = 10.0;
            min_y = -10.0; max_y = 10.0;
        } else {
            double dx = max_x - min_x;
            if (dx == 0.0) dx = 1.0;
            min_x -= dx * 0.05;
            max_x += dx * 0.05;
            
            double dy = max_y - min_y;
            if (dy == 0.0) dy = 1.0;
            min_y -= dy * 0.05;
            max_y += dy * 0.05;
        }
    }
    
    double mid_x = (min_x + max_x) / 2.0 + plot_center_x;
    double mid_y = (min_y + max_y) / 2.0 + plot_center_y;
    double half_range_x = (max_x - min_x) / 2.0 * plot_scale;
    double half_range_y = (max_y - min_y) / 2.0 * plot_scale;
    
    min_x = mid_x - half_range_x;
    max_x = mid_x + half_range_x;
    min_y = mid_y - half_range_y;
    max_y = mid_y + half_range_y;
    
    double range_x = max_x - min_x;
    double range_y = max_y - min_y;
    if (range_x == 0.0) range_x = 1.0;
    if (range_y == 0.0) range_y = 1.0;
    
    int cy = 12 + (max_y / range_y) * (SCR_H - 12);
    int cx = (0.0 - min_x) / range_x * SCR_W;
    if (cy >= 12 && cy < SCR_H) {
        M5Cardputer.Display.drawLine(0, cy, SCR_W, cy, TFT_DARKGREY);
    }
    if (cx >= 0 && cx < SCR_W) {
        M5Cardputer.Display.drawLine(cx, 12, cx, SCR_H, TFT_DARKGREY);
    }
    
    for (const auto& line : plot_lines) {
        int prev_px = -1;
        int prev_py = -1;
        for (size_t i = 0; i < line.x.size() && i < line.y.size(); ++i) {
            double xv = line.x[i];
            double yv = line.y[i];
            if (std::isnan(xv) || std::isinf(xv) || std::isnan(yv) || std::isinf(yv)) {
                prev_px = -1;
                prev_py = -1;
                continue;
            }
            int px = (xv - min_x) / range_x * SCR_W;
            int py = 12 + (max_y - yv) / range_y * (SCR_H - 12);
            
            if (line.linestyle == "None" || line.linestyle == " " || line.linestyle.empty()) {
                M5Cardputer.Display.drawPixel(px, py, line.color);
                M5Cardputer.Display.drawPixel(px-1, py, line.color);
                M5Cardputer.Display.drawPixel(px+1, py, line.color);
                M5Cardputer.Display.drawPixel(px, py-1, line.color);
                M5Cardputer.Display.drawPixel(px, py+1, line.color);
            } else {
                if (prev_px != -1) {
                    if (line.linestyle == "-") {
                        M5Cardputer.Display.drawLine(prev_px, prev_py, px, py, line.color);
                    } else if (line.linestyle == "--") {
                        drawDashedLine(prev_px, prev_py, px, py, line.color, 4, 4);
                    } else if (line.linestyle == ":") {
                        drawDashedLine(prev_px, prev_py, px, py, line.color, 1, 3);
                    } else if (line.linestyle == "-.") {
                        drawDashDotLine(prev_px, prev_py, px, py, line.color);
                    }
                }
                prev_px = px;
                prev_py = py;
            }
        }
    }
}

static void drawScripts() {
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    
    if (script_running_mode) {
        drawStatusBar("Script output");
        int y = 15;
        int max_lines = 7;
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setTextColor(TFT_WHITE);
        std::vector<std::string> wrapped_output = wrapTextLines(script_console_output, 38);
        int total = (int)wrapped_output.size();
        if (script_console_scroll_offset > total - 1) script_console_scroll_offset = std::max(0, total - 1);
        if (script_console_scroll_offset < 0) script_console_scroll_offset = 0;
        
        int start = std::max(0, total - max_lines - script_console_scroll_offset);
        for (int i = start, row = 0; i < total && row < max_lines; ++i, ++row) {
            M5Cardputer.Display.setCursor(5, y + row * 14);
            M5Cardputer.Display.print(wrapped_output[i].c_str());
        }
    } else if (script_edit_mode) {
        const auto& s = user_scripts[script_selected_idx];
        
        int n_buf = (int)script_edit_buf.size();
        std::vector<uint16_t> char_colors(n_buf, TFT_WHITE);
        std::vector<int> bracket_levels(n_buf, 0);
        int current_level = 0;

        for (int i = 0; i < n_buf; ) {
            char c = script_edit_buf[i];

            // 1. Comments
            if (c == '#' || (c == '/' && i + 1 < n_buf && script_edit_buf[i+1] == '/')) {
                while (i < n_buf && script_edit_buf[i] != '\n') {
                    char_colors[i++] = TFT_DARKGREY;
                }
                continue;
            }

            // 2. Strings
            if (c == '"' || c == '\'') {
                char quote = c;
                char_colors[i++] = 0x07E0 /* green */;
                while (i < n_buf && script_edit_buf[i] != quote && script_edit_buf[i] != '\n') {
                    char_colors[i++] = 0x07E0 /* green */;
                }
                if (i < n_buf && script_edit_buf[i] == quote) {
                    char_colors[i++] = 0x07E0 /* green */;
                }
                continue;
            }

            // 3. Brackets
            if (c == '(' || c == '{' || c == '[' || c == ')' || c == '}' || c == ']') {
                if (c == '(' || c == '{' || c == '[') {
                    bracket_levels[i] = current_level;
                    current_level++;
                } else {
                    current_level--;
                    bracket_levels[i] = current_level;
                }
                uint16_t rainbow[] = { TFT_CYAN, TFT_MAGENTA, TFT_YELLOW, 0x07E0, 0xFDA0 };
                char_colors[i] = (bracket_levels[i] >= 0) ? rainbow[bracket_levels[i] % 5] : TFT_RED;
                i++;
                continue;
            }

            // 4. Numbers
            if (std::isdigit((unsigned char)c) || (c == '.' && i + 1 < n_buf && std::isdigit((unsigned char)script_edit_buf[i+1]))) {
                while (i < n_buf && (std::isdigit((unsigned char)script_edit_buf[i]) || script_edit_buf[i] == '.' || script_edit_buf[i] == 'e' || script_edit_buf[i] == 'E' || std::isalpha((unsigned char)script_edit_buf[i]))) {
                    char_colors[i++] = TFT_YELLOW;
                }
                continue;
            }

            // 5. Identifiers & Keywords
            if (std::isalpha((unsigned char)c) || c == '_') {
                int start = i;
                while (i < n_buf && (std::isalnum((unsigned char)script_edit_buf[i]) || script_edit_buf[i] == '_' || script_edit_buf[i] == '.')) {
                    i++;
                }
                std::string word = script_edit_buf.substr(start, i - start);
                bool is_kw = (word == "if" || word == "else" || word == "elif" || word == "for" || word == "while" ||
                              word == "return" || word == "fn" || word == "def" || word == "function" ||
                              word == "print" || word == "plot" || word == "sleep" || word == "len" ||
                              word == "and" || word == "or" || word == "not" || word == "xor" ||
                              word == "plot.hold" || word == "plot.show" || word == "plot.close" ||
                              word == "plot.xlim" || word == "plot.ylim");
                uint16_t word_color = is_kw ? TFT_CYAN : TFT_WHITE;
                for (int j = start; j < i; ++j) {
                    char_colors[j] = word_color;
                }
                continue;
            }

            // Default operators / symbols
            char_colors[i++] = TFT_WHITE;
        }

        // Calculate visual row for each character and cursor
        int cur_line = 1;
        int row_ctr = 1;
        int col_ctr = 0;
        int max_cols = 38;
        int cursor_row = 1;
        int cursor_col = 0;

        for (int i = 0; i <= n_buf; ++i) {
            if (i == script_cursor_pos) {
                cursor_row = row_ctr;
                cursor_col = col_ctr;
            }
            if (i < n_buf) {
                char c = script_edit_buf[i];
                if (c == '\n') {
                    cur_line++;
                    row_ctr++;
                    col_ctr = 0;
                } else {
                    col_ctr++;
                    if (col_ctr >= max_cols) {
                        row_ctr++;
                        col_ctr = 0;
                    }
                }
            }
        }

        static int script_scroll_row = 1;
        int max_visible_rows = 8;
        if (cursor_row < script_scroll_row) {
            script_scroll_row = cursor_row;
        } else if (cursor_row >= script_scroll_row + max_visible_rows) {
            script_scroll_row = cursor_row - max_visible_rows + 1;
        }
        if (script_scroll_row < 1) script_scroll_row = 1;

        char title_buf[64];
        snprintf(title_buf, sizeof(title_buf), "Edit %s (L:%d)", s.name.c_str(), cur_line);
        drawStatusBar(title_buf);
        
        M5Cardputer.Display.setTextSize(1);
        int start_x = 4;
        int start_y = 16;
        int line_height = 12;

        row_ctr = 1;
        col_ctr = 0;
        
        for (int i = 0; i <= n_buf; ++i) {
            bool in_viewport = (row_ctr >= script_scroll_row && row_ctr < script_scroll_row + max_visible_rows);
            
            if (i == script_cursor_pos && in_viewport) {
                int draw_row = row_ctr - script_scroll_row;
                M5Cardputer.Display.drawFastVLine(start_x + col_ctr * 6, start_y + draw_row * line_height, 12, TFT_CYAN);
            }
            
            if (i < n_buf) {
                char c = script_edit_buf[i];
                if (c == '\n') {
                    row_ctr++;
                    col_ctr = 0;
                } else {
                    if (in_viewport) {
                        int draw_row = row_ctr - script_scroll_row;
                        M5Cardputer.Display.setCursor(start_x + col_ctr * 6, start_y + draw_row * line_height);
                        M5Cardputer.Display.setTextColor(char_colors[i]);
                        M5Cardputer.Display.print(c);
                    }
                    col_ctr++;
                    if (col_ctr >= max_cols) {
                        row_ctr++;
                        col_ctr = 0;
                    }
                }
            }
        }

        if (script_exit_prompt_mode) {
            drawConfirmModal("Save changes?", script_exit_selected_idx);
        }
    } else {
        drawStatusBar("Cardulator | SCRIPTS");
        int y = 15;
        M5Cardputer.Display.setTextSize(1);
        
        if (user_scripts.empty()) {
            M5Cardputer.Display.setTextColor(TFT_DARKGREY);
            M5Cardputer.Display.setCursor(10, y + 20);
            M5Cardputer.Display.print("No scripts available.");
        } else {
            int start = 0;
            if (script_selected_idx >= 6) {
                start = script_selected_idx - 5;
            }
            for (int i = start; i < (int)user_scripts.size() && i < start + 6; ++i) {
                if (i == script_selected_idx) {
                    M5Cardputer.Display.fillRect(0, y + (i - start) * 14 - 1, SCR_W, 13, TFT_BLUE);
                    M5Cardputer.Display.setTextColor(TFT_WHITE);
                } else {
                    M5Cardputer.Display.setTextColor(TFT_WHITE);
                }
                M5Cardputer.Display.setCursor(5, y + (i - start) * 14);
                M5Cardputer.Display.print(user_scripts[i].name.c_str());
            }
        }
        if (script_name_prompt_mode) {
            int popup_w = 200;
            int popup_h = 60;
            int popup_x = (SCR_W - popup_w) / 2;
            int popup_y = (SCR_H - popup_h) / 2;
            
            M5Cardputer.Display.fillRect(popup_x, popup_y, popup_w, popup_h, 0x18E3 /* dark grey */);
            M5Cardputer.Display.drawRect(popup_x, popup_y, popup_w, popup_h, TFT_CYAN);
            
            M5Cardputer.Display.setTextSize(1);
            M5Cardputer.Display.setTextColor(TFT_YELLOW);
            M5Cardputer.Display.setCursor(popup_x + 10, popup_y + 10);
            M5Cardputer.Display.print(is_renaming_script ? "Rename script:" : "New script name:");
            
            M5Cardputer.Display.setCursor(popup_x + 10, popup_y + 30);
            M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLUE);
            M5Cardputer.Display.printf("[%s]", script_name_edit_buf.c_str());
            M5Cardputer.Display.drawFastVLine(popup_x + 16 + script_name_edit_buf.size() * 6, popup_y + 30, 10, TFT_CYAN);
        }

        if (delete_confirm_prompt_mode) {
            drawConfirmModal("Delete script?", delete_confirm_selected_idx);
        }
    }
}

static void pushExprHistory(const std::string& expr) {
    expr_history.push_back(expr);
    while ((int)expr_history.size() > Config::max_history_limit) {
        expr_history.erase(expr_history.begin());
    }
}

static void pushResultHistory(double val, const std::string& res_str) {
    if (!std::isnan(val)) {
        history.push_back(val);
        while ((int)history.size() > Config::max_history_limit) {
            history.erase(history.begin());
        }
    }
    history_results.push_back(res_str);
    while ((int)history_results.size() > Config::max_history_limit) {
        history_results.erase(history_results.begin());
    }
}

inline int getLineStart(const std::string& str, int pos) {
    if (pos <= 0) return 0;
    if (pos > (int)str.size()) pos = str.size();
    int i = pos - 1;
    while (i >= 0 && str[i] != '\n') i--;
    return i + 1;
}

inline int getLineEnd(const std::string& str, int pos) {
    int n = str.size();
    if (pos < 0) return 0;
    if (pos >= n) return n;
    int i = pos;
    while (i < n && str[i] != '\n') i++;
    return i;
}

inline int getPrevWordPos(const std::string& str, int pos) {
    if (pos <= 0) return 0;
    int i = pos - 1;
    while (i > 0 && std::isspace(str[i])) i--;
    while (i > 0 && (std::isalnum(str[i - 1]) || str[i - 1] == '_')) i--;
    return i;
}

inline int getNextWordPos(const std::string& str, int pos) {
    int n = str.size();
    if (pos >= n) return n;
    int i = pos;
    while (i < n && (std::isalnum(str[i]) || str[i] == '_')) i++;
    while (i < n && std::isspace(str[i])) i++;
    return i;
}

static void handleCalcKey(Keyboard_Class::KeysState& s) {
    if (hasError) {
        hasError = false;
        resultStr = "";
        if (s.enter) return;
    }
    // 1. Process Ctrl shortcut combinations
    if (s.ctrl) {
        bool handled = false;
        for (char c : s.word) {
            switch (c) {
                case 'a': case 'A':
                    select_all_active = true;
                    handled = true;
                    break;
                case 'c': case 'C':
                    if (select_all_active || !expression.empty()) {
                        clipboard = expression;
                    }
                    handled = true;
                    break;
                case 'x': case 'X':
                    clipboard = expression;
                    undo_buffer = expression;
                    expression = "";
                    cursor_pos = 0;
                    select_all_active = false;
                    resultStr = "";
                    hasError = false;
                    handled = true;
                    break;
                case 'v': case 'V':
                    if (!clipboard.empty()) {
                        undo_buffer = expression;
                        if (select_all_active) {
                            expression = clipboard;
                            cursor_pos = expression.size();
                            select_all_active = false;
                        } else {
                            expression.insert(cursor_pos, clipboard);
                            cursor_pos += clipboard.size();
                        }
                        resultStr = "";
                        hasError = false;
                    }
                    handled = true;
                    break;
                case 'z': case 'Z':
                    if (!undo_buffer.empty()) {
                        redo_buffer = expression;
                        expression = undo_buffer;
                        cursor_pos = expression.size();
                        undo_buffer = "";
                        select_all_active = false;
                        resultStr = "";
                        hasError = false;
                    }
                    handled = true;
                    break;
                case 'y': case 'Y':
                    if (!redo_buffer.empty()) {
                        undo_buffer = expression;
                        expression = redo_buffer;
                        cursor_pos = expression.size();
                        redo_buffer = "";
                        select_all_active = false;
                        resultStr = "";
                        hasError = false;
                    }
                    handled = true;
                    break;
            }
        }
        if (s.backspace || s.del) {
            undo_buffer = expression;
            expression = "";
            cursor_pos = 0;
            select_all_active = false;
            resultStr = "";
            hasError = false;
            handled = true;
        }
        if (handled) return;
    }

    // 2. Process Alt/Option key macro binds (only if s.word is populated and it's not cursor movements)
    if ((s.alt || s.opt) && !s.left && !s.right && !s.up && !s.down) {
        for (char c : s.word) {
            for (const auto& b : user_binds) {
                if (b.key == c) {
                    expression.insert(cursor_pos, b.action);
                    cursor_pos += b.action.size();
                    select_all_active = false;
                    resultStr = "";
                    hasError = false;
                    return;
                }
            }
        }
    }

    // 3. Fn Home (Fn+H) / End (Fn+E) / Delete (Fn+Backspace)
    if (s.fn) {
        for (char c : s.word) {
            if (c == 'h' || c == 'H') {
                if (s.ctrl) cursor_pos = 0;
                else cursor_pos = getLineStart(expression, cursor_pos);
                select_all_active = false;
                return;
            }
            if (c == 'e' || c == 'E') {
                if (s.ctrl) cursor_pos = expression.size();
                else cursor_pos = getLineEnd(expression, cursor_pos);
                select_all_active = false;
                return;
            }
        }
    }

    // 4. Delete & Backspace
    if (s.del || (s.fn && s.backspace)) {
        undo_buffer = expression;
        if (select_all_active) {
            expression = "";
            cursor_pos = 0;
            select_all_active = false;
        } else if (s.ctrl) {
            int next_w = getNextWordPos(expression, cursor_pos);
            expression.erase(cursor_pos, next_w - cursor_pos);
        } else {
            if (cursor_pos < (int)expression.size()) {
                expression.erase(cursor_pos, 1);
            }
        }
        resultStr = "";
        hasError = false;
        return;
    }

    if (s.backspace) {
        if (select_all_active) {
            undo_buffer = expression;
            expression = "";
            cursor_pos = 0;
            select_all_active = false;
        } else if (s.ctrl) {
            undo_buffer = expression;
            int prev_w = getPrevWordPos(expression, cursor_pos);
            expression.erase(prev_w, cursor_pos - prev_w);
            cursor_pos = prev_w;
        } else {
            if (cursor_pos > 0) {
                expression.erase(cursor_pos - 1, 1);
                cursor_pos--;
            }
        }
        resultStr = "";
        hasError = false;
        return;
    }

    // 5. Arrow key shortcuts (Left/Right, Ctrl/Alt combinations)
    if (s.left) {
        if (s.ctrl) {
            cursor_pos = getPrevWordPos(expression, cursor_pos);
        } else if (s.alt || s.opt) {
            cursor_pos = 0;
        } else {
            if (cursor_pos > 0) cursor_pos--;
        }
        select_all_active = false;
        return;
    }
    if (s.right) {
        if (s.ctrl) {
            cursor_pos = getNextWordPos(expression, cursor_pos);
        } else if (s.alt || s.opt) {
            cursor_pos = expression.size();
        } else {
            if (cursor_pos < (int)expression.size()) cursor_pos++;
        }
        select_all_active = false;
        return;
    }
    if (s.up) {
        if (s.ctrl) {
            history_scroll_offset++;
            return;
        }
        if (!expr_history.empty()) {
            if (history_index > 0) {
                history_index--;
                expression = expr_history[history_index];
                cursor_pos = expression.size();
                select_all_active = false;
            } else if (history_index == (int)expr_history.size()) {
                history_index = (int)expr_history.size() - 1;
                expression = expr_history[history_index];
                cursor_pos = expression.size();
                select_all_active = false;
            }
        }
        return;
    }
    if (s.down) {
        if (s.ctrl) {
            if (history_scroll_offset > 0) history_scroll_offset--;
            return;
        }
        if (!expr_history.empty()) {
            if (history_index < (int)expr_history.size() - 1) {
                history_index++;
                expression = expr_history[history_index];
                cursor_pos = expression.size();
                select_all_active = false;
            } else if (history_index == (int)expr_history.size() - 1) {
                history_index = expr_history.size();
                expression = "";
                cursor_pos = 0;
                select_all_active = false;
            }
        }
        return;
    }

    if (s.tab) {
        handleTabCompletion(expression, cursor_pos);
        select_all_active = false;
        return;
    }
    if (s.fn) {
        for (char c : s.word) {
            switch (c) {
                case 'c': case 'C': 
                    expression = ""; 
                    cursor_pos = 0;
                    resultStr = ""; 
                    hasError = false; 
                    history.clear();
                    user_args.clear();
                    user_funcs.clear();
                    expr_history.clear();
                    history_results.clear();
                    history_index = -1;
                    select_all_active = false;
                    history_scroll_offset = 0;
                    break;
                case 'r': case 'R': expression.insert(cursor_pos, "sqrt("); cursor_pos += 5; select_all_active = false; break;
                case 'p': case 'P': expression.insert(cursor_pos, "pi"); cursor_pos += 2; select_all_active = false; break;
                case 'l': case 'L': expression.insert(cursor_pos, "ln("); cursor_pos += 3; select_all_active = false; break;
                case 's': case 'S': expression.insert(cursor_pos, "sin("); cursor_pos += 4; select_all_active = false; break;
                case 'o': case 'O': expression.insert(cursor_pos, "cos("); cursor_pos += 4; select_all_active = false; break;
                case 't': case 'T': expression.insert(cursor_pos, "tan("); cursor_pos += 4; select_all_active = false; break;
                case 'h': case 'H': appState = STATE_HELP; break;
                case '^':           expression.insert(cursor_pos, "^"); cursor_pos += 1; select_all_active = false; break;
            }
        }
        return;
    }
    if (s.enter) {
        if (expression.empty()) return;
        std::string err;
        bool isDef = false;
        double result = 0.0;
        
        std::string trimmed_expr = expression;
        trimmed_expr.erase(0, trimmed_expr.find_first_not_of(" \t"));
        trimmed_expr.erase(trimmed_expr.find_last_not_of(" \t") + 1);
        if (trimmed_expr.rfind("print(", 0) == 0 && trimmed_expr.back() == ')') {
            std::string print_body = trimmed_expr.substr(6, trimmed_expr.size() - 7);
            std::string print_res = formatPrintString(print_body, err);
            if (!err.empty()) {
                resultStr = err;
                hasError = true;
            } else {
                pushExprHistory(expression);
                history_index = expr_history.size();
                pushResultHistory(std::numeric_limits<double>::quiet_NaN(), print_res);
                expression = "";
                cursor_pos = 0;
                resultStr = print_res;
                hasError = false;
            }
            select_all_active = false;
            return;
        }
        if (trimmed_expr == "help()" || trimmed_expr == "help" || trimmed_expr.rfind("help(", 0) == 0) {
            std::string help_res;
            preprocessHelp(trimmed_expr, help_res);
            pushExprHistory(expression);
            history_index = expr_history.size();
            pushResultHistory(std::numeric_limits<double>::quiet_NaN(), help_res);
            expression = "";
            cursor_pos = 0;
            resultStr = help_res;
            hasError = false;
            select_all_active = false;
            return;
        }
        if (handlePlotCommands(expression, err, result)) {
            if (!err.empty()) {
                resultStr = err;
                hasError = true;
            } else {
                pushExprHistory(expression);
                history_index = expr_history.size();
                pushResultHistory(std::numeric_limits<double>::quiet_NaN(), "Plot");
                expression = "";
                cursor_pos = 0;
                resultStr = "";
                hasError = false;
                drawPlot();
            }
            select_all_active = false;
            return;
        }
        
        std::vector<std::string> stmts = splitIntoStatements(expression);
        if (stmts.size() > 1 || (!stmts.empty() && (stmts[0].rfind("fn ", 0) == 0 || stmts[0].rfind("def ", 0) == 0 || stmts[0].rfind("function ", 0) == 0))) {
            double last_res = 0.0;
            bool last_is_def = false;
            for (size_t i = 0; i < stmts.size(); ++i) {
                const auto& stmt = stmts[i];
                if (stmt.rfind("fn ", 0) == 0 || stmt.rfind("def ", 0) == 0 || stmt.rfind("function ", 0) == 0) {
                    size_t open_paren = stmt.find('(');
                    size_t close_paren = stmt.find(')', open_paren);
                    size_t open_brace = stmt.find('{', close_paren);
                    if (open_paren != std::string::npos && close_paren != std::string::npos && open_brace != std::string::npos) {
                        size_t kw_len = (stmt.rfind("fn ", 0) == 0) ? 3 : ((stmt.rfind("def ", 0) == 0) ? 4 : 9);
                        std::string fname = stmt.substr(kw_len, open_paren - kw_len);
                        fname.erase(0, fname.find_first_not_of(" \t"));
                        fname.erase(fname.find_last_not_of(" \t") + 1);
                        
                        std::string params_str = stmt.substr(open_paren + 1, close_paren - open_paren - 1);
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

                        std::vector<std::string> body_stmts;
                        size_t j = i + 1;
                        while (j < stmts.size() && stmts[j] != "}") {
                            body_stmts.push_back(stmts[j]);
                            j++;
                        }
                        i = j;

                        user_script_funcs.erase(std::remove_if(user_script_funcs.begin(), user_script_funcs.end(), [&](const CustomScriptFunc& f){
                            return f.name == fname;
                        }), user_script_funcs.end());

                        user_script_funcs.push_back({fname, params, body_stmts});
                        if (std::find(autocomplete_words.begin(), autocomplete_words.end(), fname + "(") == autocomplete_words.end()) {
                            autocomplete_words.push_back(fname + "(");
                        }
                        resultStr = "Def OK";
                        last_is_def = true;
                    }
                } else {
                    last_res = evaluateInput(stmt, err, last_is_def);
                    if (!err.empty()) break;
                }
            }

            if (!err.empty()) {
                resultStr = err;
                hasError = true;
            } else {
                pushExprHistory(expression);
                history_index = expr_history.size();
                if (!last_is_def) {
                    resultStr = fmtNum(last_res);
                    pushResultHistory(last_res, "e" + std::to_string(history.size()) + " = " + resultStr);
                } else {
                    pushResultHistory(std::numeric_limits<double>::quiet_NaN(), resultStr);
                }
                expression = "";
                cursor_pos = 0;
                hasError = false;
            }
            select_all_active = false;
            return;
        }
        
        result = evaluateInput(expression, err, isDef);
        if (!err.empty()) {
            resultStr = err;
            hasError = true;
        } else {
            pushExprHistory(expression);
            history_index = expr_history.size();

            if (isDef) {
                size_t eq_pos = expression.find('=');
                std::string lhs = expression.substr(0, eq_pos);
                lhs.erase(0, lhs.find_first_not_of(" \t"));
                lhs.erase(lhs.find_last_not_of(" \t") + 1);
                if (lhs.find('(') == std::string::npos) {
                    if (user_arrays.find(lhs) != user_arrays.end()) {
                        resultStr = lhs + " = Array[" + std::to_string(user_arrays[lhs].size()) + "]";
                    } else {
                        resultStr = lhs + " = " + fmtNum(result);
                    }
                    pushResultHistory(std::numeric_limits<double>::quiet_NaN(), resultStr);
                } else {
                    resultStr = "Def OK";
                    pushResultHistory(std::numeric_limits<double>::quiet_NaN(), resultStr);
                }
            } else {
                // Check if the expression is exactly a known constant or variable name
                std::string trimmed = expression;
                trimmed.erase(0, trimmed.find_first_not_of(" \t"));
                trimmed.erase(trimmed.find_last_not_of(" \t") + 1);
                
                std::string label = "";
                for (const auto& cn : user_consts) {
                    if (cn.name == trimmed) { label = trimmed; break; }
                }
                if (label.empty()) {
                    for (const auto& arg : user_args) {
                        if (arg.name == trimmed) { label = trimmed; break; }
                    }
                }
                if (label.empty() && trimmed.size() > 1 && trimmed[0] == 'e' &&
                    std::all_of(trimmed.begin() + 1, trimmed.end(), ::isdigit)) {
                    label = trimmed;
                }
                
                if (!label.empty()) {
                    resultStr = label + " = " + fmtNum(result);
                } else {
                    resultStr = "e" + std::to_string(history.size() + 1) + " = " + fmtNum(result);
                }
                pushResultHistory(result, resultStr);
            }
            hasError = false;
            expression = "";
            cursor_pos = 0;
            history_scroll_offset = 0;
        }
        select_all_active = false;
        return;
    }
    if (s.del || s.backspace) {
        if (select_all_active) {
            undo_buffer = expression;
            expression = "";
            cursor_pos = 0;
            select_all_active = false;
        } else {
            if (cursor_pos > 0) {
                expression.erase(cursor_pos - 1, 1);
                cursor_pos--;
            }
        }
        resultStr = "";
        hasError = false;
        return;
    }
    for (char c : s.word) {
        if (c >= 32 && c < 127) {
            if (select_all_active) {
                undo_buffer = expression;
                expression = "";
                cursor_pos = 0;
                select_all_active = false;
            }
            
            bool handled = false;
            if (auto_brackets) {
                if ((c == ')' || c == ']' || c == '}' || c == '\'' || c == '"') &&
                    cursor_pos < (int)expression.size() && expression[cursor_pos] == c) {
                    cursor_pos++;
                    handled = true;
                } else if (c == '(' || c == '[' || c == '{' || c == '\'' || c == '"') {
                    char close_c = 0;
                    if (c == '(') close_c = ')';
                    else if (c == '[') close_c = ']';
                    else if (c == '{') close_c = '}';
                    else close_c = c;
                    
                    std::string to_ins = std::string(1, c) + close_c;
                    expression.insert(cursor_pos, to_ins);
                    cursor_pos++;
                    handled = true;
                }
            }
            
            if (!handled) {
                expression.insert(cursor_pos, 1, c);
                cursor_pos++;
            }
            resultStr = "";
            hasError = false;
        }
    }
}

static void handleVarsKey(Keyboard_Class::KeysState& s) {
    std::vector<std::pair<std::string, double>> all_vars;
    for (size_t i = 0; i < history.size(); ++i) {
        all_vars.push_back({"e" + std::to_string(i + 1), history[i]});
    }
    for (const auto& arg : user_args) {
        all_vars.push_back({arg.name, arg.val});
    }

    if (delete_confirm_prompt_mode) {
        bool is_nav = s.left || s.right || s.up || s.down || s.tab;
        if (is_nav) {
            delete_confirm_selected_idx = 1 - delete_confirm_selected_idx;
            return;
        }
        for (char c : s.word) {
            if (c == 'y' || c == 'Y') {
                if (!all_vars.empty() && var_selected_idx < (int)all_vars.size()) {
                    if (var_selected_idx < (int)history.size()) {
                        history.erase(history.begin() + var_selected_idx);
                    } else {
                        int user_idx = var_selected_idx - (int)history.size();
                        user_args.erase(user_args.begin() + user_idx);
                        saveNVSData();
                    }
                    if (var_selected_idx >= (int)all_vars.size() - 1) {
                        var_selected_idx = std::max(0, (int)all_vars.size() - 2);
                    }
                }
                delete_confirm_prompt_mode = false;
                return;
            }
            if (c == 'n' || c == 'N') {
                delete_confirm_prompt_mode = false;
                return;
            }
        }
        if (s.enter) {
            if (delete_confirm_selected_idx == 0) {
                if (!all_vars.empty() && var_selected_idx < (int)all_vars.size()) {
                    if (var_selected_idx < (int)history.size()) {
                        history.erase(history.begin() + var_selected_idx);
                    } else {
                        int user_idx = var_selected_idx - (int)history.size();
                        user_args.erase(user_args.begin() + user_idx);
                        saveNVSData();
                    }
                    if (var_selected_idx >= (int)all_vars.size() - 1) {
                        var_selected_idx = std::max(0, (int)all_vars.size() - 2);
                    }
                }
            }
            delete_confirm_prompt_mode = false;
            return;
        }
        if (s.esc) {
            delete_confirm_prompt_mode = false;
            return;
        }
        return;
    }
    
    if (var_edit_mode) {
        if (s.enter) {
            if (!var_edit_buf.empty()) {
                std::string err;
                double val = evaluate(var_edit_buf, err);
                if (err.empty() && !std::isnan(val)) {
                    if (var_selected_idx < (int)history.size()) {
                        history[var_selected_idx] = val;
                    } else {
                        int user_idx = var_selected_idx - (int)history.size();
                        user_args[user_idx].val = val;
                        saveNVSData();
                    }
                }
            }
            var_edit_mode = false;
            return;
        }
        if (s.esc) {
            var_edit_mode = false;
            return;
        }
        if (s.del || s.backspace) {
            if (!var_edit_buf.empty()) var_edit_buf.pop_back();
            return;
        }
        for (char c : s.word) {
            if (c >= 32 && c < 127) var_edit_buf += c;
        }
    } else {
        bool is_up = s.up;
        bool is_down = s.down || s.tab;
        if (is_up) {
            if (var_selected_idx > 0) {
                var_selected_idx--;
            } else if (!all_vars.empty()) {
                var_selected_idx = (int)all_vars.size() - 1; // Cyclic top -> bottom
            }
            return;
        }
        if (is_down) {
            if (var_selected_idx < (int)all_vars.size() - 1) {
                var_selected_idx++;
            } else if (!all_vars.empty()) {
                var_selected_idx = 0; // Cyclic bottom -> top
            }
            return;
        }
        if (s.enter && !all_vars.empty()) {
            var_edit_mode = true;
            var_edit_buf = fmtNum(all_vars[var_selected_idx].second);
            return;
        }
        if (s.del || s.backspace) {
            if (!all_vars.empty() && var_selected_idx < (int)all_vars.size()) {
                delete_confirm_prompt_mode = true;
                delete_confirm_selected_idx = 1; // Default to No (safe)
            }
            return;
        }
        for (char c : s.word) {
            if (c == 'n' || c == 'N') {
                std::string new_name = "v" + std::to_string(user_args.size() + 1);
                user_args.push_back({new_name, 0.0});
                var_selected_idx = history.size() + user_args.size() - 1;
                var_edit_mode = true;
                var_edit_buf = "0";
                return;
            }
        }
        if (s.esc) {
            navigateBack();
        }
    }
}

static void handleConstsKey(Keyboard_Class::KeysState& s) {
    if (delete_confirm_prompt_mode) {
        bool is_nav = s.left || s.right || s.up || s.down || s.tab;
        if (is_nav) {
            delete_confirm_selected_idx = 1 - delete_confirm_selected_idx;
            return;
        }
        for (char c : s.word) {
            if (c == 'y' || c == 'Y') {
                if (!user_consts.empty() && const_selected_idx < (int)user_consts.size()) {
                    std::string name = user_consts[const_selected_idx].name;
                    if (name != "pi" && name != "e") {
                        user_consts.erase(user_consts.begin() + const_selected_idx);
                        saveNVSData();
                    }
                    if (const_selected_idx >= (int)user_consts.size()) {
                        const_selected_idx = std::max(0, (int)user_consts.size() - 1);
                    }
                }
                delete_confirm_prompt_mode = false;
                return;
            }
            if (c == 'n' || c == 'N') {
                delete_confirm_prompt_mode = false;
                return;
            }
        }
        if (s.enter) {
            if (delete_confirm_selected_idx == 0) {
                if (!user_consts.empty() && const_selected_idx < (int)user_consts.size()) {
                    std::string name = user_consts[const_selected_idx].name;
                    if (name != "pi" && name != "e") {
                        user_consts.erase(user_consts.begin() + const_selected_idx);
                        saveNVSData();
                    }
                    if (const_selected_idx >= (int)user_consts.size()) {
                        const_selected_idx = std::max(0, (int)user_consts.size() - 1);
                    }
                }
            }
            delete_confirm_prompt_mode = false;
            return;
        }
        if (s.esc) {
            delete_confirm_prompt_mode = false;
            return;
        }
        return;
    }
    if (const_edit_mode) {
        if (s.enter) {
            if (!const_edit_buf.empty()) {
                std::string err;
                double val = evaluate(const_edit_buf, err);
                if (err.empty() && !std::isnan(val)) {
                    user_consts[const_selected_idx].val = val;
                    saveNVSData();
                }
            }
            const_edit_mode = false;
            return;
        }
        if (s.esc) {
            const_edit_mode = false;
            return;
        }
        if (s.del || s.backspace) {
            if (!const_edit_buf.empty()) const_edit_buf.pop_back();
            return;
        }
        for (char c : s.word) {
            if (c >= 32 && c < 127) const_edit_buf += c;
        }
    } else {
        bool is_up = s.up;
        bool is_down = s.down || s.tab;
        if (is_up) {
            if (const_selected_idx > 0) {
                const_selected_idx--;
            } else if (!user_consts.empty()) {
                const_selected_idx = (int)user_consts.size() - 1; // Cyclic top -> bottom
            }
            return;
        }
        if (is_down) {
            if (const_selected_idx < (int)user_consts.size() - 1) {
                const_selected_idx++;
            } else if (!user_consts.empty()) {
                const_selected_idx = 0; // Cyclic bottom -> top
            }
            return;
        }
        if (s.enter && !user_consts.empty()) {
            const_edit_mode = true;
            const_edit_buf = fmtNum(user_consts[const_selected_idx].val);
            return;
        }
        if (s.del || s.backspace) {
            if (!user_consts.empty() && const_selected_idx < (int)user_consts.size()) {
                std::string name = user_consts[const_selected_idx].name;
                if (name != "pi" && name != "e") {
                    delete_confirm_prompt_mode = true;
                    delete_confirm_selected_idx = 1;
                }
            }
            return;
        }
        for (char c : s.word) {
            if (c == 'n' || c == 'N') {
                std::string new_name = "c" + std::to_string(user_consts.size() + 1);
                user_consts.push_back({new_name, 0.0});
                const_selected_idx = user_consts.size() - 1;
                const_edit_mode = true;
                const_edit_buf = "0";
                return;
            }
        }
        if (s.esc) {
            navigateBack();
        }
    }
}

static void handleBindsKey(Keyboard_Class::KeysState& s) {
    if (delete_confirm_prompt_mode) {
        bool is_nav = s.left || s.right || s.up || s.down || s.tab;
        if (is_nav) {
            delete_confirm_selected_idx = 1 - delete_confirm_selected_idx;
            return;
        }
        for (char c : s.word) {
            if (c == 'y' || c == 'Y') {
                if (!user_binds.empty() && bind_selected_idx < (int)user_binds.size()) {
                    user_binds.erase(user_binds.begin() + bind_selected_idx);
                    saveNVSData();
                    if (bind_selected_idx >= (int)user_binds.size()) {
                        bind_selected_idx = std::max(0, (int)user_binds.size() - 1);
                    }
                }
                delete_confirm_prompt_mode = false;
                return;
            }
            if (c == 'n' || c == 'N') {
                delete_confirm_prompt_mode = false;
                return;
            }
        }
        if (s.enter) {
            if (delete_confirm_selected_idx == 0) {
                if (!user_binds.empty() && bind_selected_idx < (int)user_binds.size()) {
                    user_binds.erase(user_binds.begin() + bind_selected_idx);
                    saveNVSData();
                    if (bind_selected_idx >= (int)user_binds.size()) {
                        bind_selected_idx = std::max(0, (int)user_binds.size() - 1);
                    }
                }
            }
            delete_confirm_prompt_mode = false;
            return;
        }
        if (s.esc) {
            delete_confirm_prompt_mode = false;
            return;
        }
        return;
    }
    if (bind_edit_mode) {
        if (s.enter) {
            user_binds[bind_selected_idx].action = bind_edit_buf;
            saveNVSData();
            bind_edit_mode = false;
            return;
        }
        if (s.esc) {
            bind_edit_mode = false;
            return;
        }
        if (s.del || s.backspace) {
            if (!bind_edit_buf.empty()) bind_edit_buf.pop_back();
            return;
        }
        for (char c : s.word) {
            if (c >= 32 && c < 127) bind_edit_buf += c;
        }
    } else {
        bool is_up = s.up;
        bool is_down = s.down || s.tab;
        if (is_up) {
            if (bind_selected_idx > 0) {
                bind_selected_idx--;
            } else if (!user_binds.empty()) {
                bind_selected_idx = (int)user_binds.size() - 1; // Cyclic top -> bottom
            }
            return;
        }
        if (is_down) {
            if (bind_selected_idx < (int)user_binds.size() - 1) {
                bind_selected_idx++;
            } else if (!user_binds.empty()) {
                bind_selected_idx = 0; // Cyclic bottom -> top
            }
            return;
        }
        if (s.enter && !user_binds.empty()) {
            bind_edit_mode = true;
            bind_edit_buf = user_binds[bind_selected_idx].action;
            return;
        }
        if (s.del || s.backspace) {
            if (!user_binds.empty() && bind_selected_idx < (int)user_binds.size()) {
                delete_confirm_prompt_mode = true;
                delete_confirm_selected_idx = 1;
            }
            return;
        }
        for (char c : s.word) {
            if (c == 'n' || c == 'N') {
                user_binds.push_back({'x', ""});
                bind_selected_idx = user_binds.size() - 1;
                bind_edit_mode = true;
                bind_edit_buf = "";
                return;
            }
        }
        if (s.esc) {
            navigateBack();
        }
    }
}

static void handleFormulasKey(Keyboard_Class::KeysState& s) {
    if (delete_confirm_prompt_mode) {
        bool is_nav = s.left || s.right || s.up || s.down || s.tab;
        if (is_nav) {
            delete_confirm_selected_idx = 1 - delete_confirm_selected_idx;
            return;
        }
        for (char c : s.word) {
            if (c == 'y' || c == 'Y') {
                if (!user_formulas.empty() && formula_selected_idx >= 0 && formula_selected_idx < (int)user_formulas.size()) {
                    user_formulas.erase(user_formulas.begin() + formula_selected_idx);
                    if (formula_selected_idx >= (int)user_formulas.size()) {
                        formula_selected_idx = std::max(0, (int)user_formulas.size() - 1);
                    }
                    saveNVSData();
                    saveSDData();
                }
                delete_confirm_prompt_mode = false;
                return;
            }
            if (c == 'n' || c == 'N') {
                delete_confirm_prompt_mode = false;
                return;
            }
        }
        if (s.enter) {
            if (delete_confirm_selected_idx == 0) {
                if (!user_formulas.empty() && formula_selected_idx >= 0 && formula_selected_idx < (int)user_formulas.size()) {
                    user_formulas.erase(user_formulas.begin() + formula_selected_idx);
                    if (formula_selected_idx >= (int)user_formulas.size()) {
                        formula_selected_idx = std::max(0, (int)user_formulas.size() - 1);
                    }
                    saveNVSData();
                    saveSDData();
                }
            }
            delete_confirm_prompt_mode = false;
            return;
        }
        if (s.esc) {
            delete_confirm_prompt_mode = false;
            return;
        }
        return;
    }
    if (formula_edit_mode) {
        if (formula_exit_prompt_mode) {
            bool is_nav = s.left || s.right || s.up || s.down || (std::find(s.word.begin(), s.word.end(), ';') != s.word.end()) || (std::find(s.word.begin(), s.word.end(), '.') != s.word.end());
            if (is_nav) {
                formula_exit_selected_idx = 1 - formula_exit_selected_idx;
                return;
            }
            for (char c : s.word) {
                if (c == 'y' || c == 'Y') {
                    std::string input = formula_edit_buf;
                    size_t eq = input.find('=');
                    if (eq != std::string::npos) {
                        std::string lhs = input.substr(0, eq);
                        std::string rhs = input.substr(eq + 1);
                        lhs.erase(0, lhs.find_first_not_of(" \t")); lhs.erase(lhs.find_last_not_of(" \t") + 1);
                        rhs.erase(0, rhs.find_first_not_of(" \t")); rhs.erase(rhs.find_last_not_of(" \t") + 1);
                        std::string fname = lhs;
                        std::vector<std::string> params;
                        size_t open_p = lhs.find('(');
                        size_t close_p = lhs.find(')');
                        if (open_p != std::string::npos && close_p != std::string::npos && close_p > open_p) {
                            fname = lhs.substr(0, open_p);
                            fname.erase(0, fname.find_first_not_of(" \t")); fname.erase(fname.find_last_not_of(" \t") + 1);
                            std::string pstr = lhs.substr(open_p + 1, close_p - open_p - 1);
                            size_t start = 0;
                            while (start < pstr.size()) {
                                size_t comma = pstr.find(',', start);
                                std::string p = (comma == std::string::npos) ? pstr.substr(start) : pstr.substr(start, comma - start);
                                p.erase(0, p.find_first_not_of(" \t")); p.erase(p.find_last_not_of(" \t") + 1);
                                if (!p.empty()) params.push_back(p);
                                if (comma == std::string::npos) break;
                                start = comma + 1;
                            }
                        }
                        if (params.size() > 4) params.resize(4);
                        if (!fname.empty() && !rhs.empty() && formula_selected_idx >= 0 && formula_selected_idx < (int)user_formulas.size()) {
                            user_formulas[formula_selected_idx] = {fname, rhs, params};
                            saveNVSData();
                            saveSDData();
                        }
                    }
                    formula_exit_prompt_mode = false;
                    formula_edit_mode = false;
                    return;
                }
                if (c == 'n' || c == 'N') {
                    formula_exit_prompt_mode = false;
                    formula_edit_mode = false;
                    return;
                }
            }
            if (s.enter) {
                if (formula_exit_selected_idx == 0) {
                    std::string input = formula_edit_buf;
                    size_t eq = input.find('=');
                    if (eq != std::string::npos) {
                        std::string lhs = input.substr(0, eq);
                        std::string rhs = input.substr(eq + 1);
                        lhs.erase(0, lhs.find_first_not_of(" \t")); lhs.erase(lhs.find_last_not_of(" \t") + 1);
                        rhs.erase(0, rhs.find_first_not_of(" \t")); rhs.erase(rhs.find_last_not_of(" \t") + 1);
                        std::string fname = lhs;
                        std::vector<std::string> params;
                        size_t open_p = lhs.find('(');
                        size_t close_p = lhs.find(')');
                        if (open_p != std::string::npos && close_p != std::string::npos && close_p > open_p) {
                            fname = lhs.substr(0, open_p);
                            fname.erase(0, fname.find_first_not_of(" \t")); fname.erase(fname.find_last_not_of(" \t") + 1);
                            std::string pstr = lhs.substr(open_p + 1, close_p - open_p - 1);
                            size_t start = 0;
                            while (start < pstr.size()) {
                                size_t comma = pstr.find(',', start);
                                std::string p = (comma == std::string::npos) ? pstr.substr(start) : pstr.substr(start, comma - start);
                                p.erase(0, p.find_first_not_of(" \t")); p.erase(p.find_last_not_of(" \t") + 1);
                                if (!p.empty()) params.push_back(p);
                                if (comma == std::string::npos) break;
                                start = comma + 1;
                            }
                        }
                        if (params.size() > 4) params.resize(4);
                        if (!fname.empty() && !rhs.empty() && formula_selected_idx >= 0 && formula_selected_idx < (int)user_formulas.size()) {
                            user_formulas[formula_selected_idx] = {fname, rhs, params};
                            saveNVSData();
                            saveSDData();
                        }
                    }
                }
                formula_exit_prompt_mode = false;
                formula_edit_mode = false;
                return;
            }
            if (s.esc) {
                formula_exit_prompt_mode = false;
                return;
            }
            return;
        }

        if (s.esc) {
            if (formula_edit_buf == formula_edit_orig_buf) {
                formula_edit_mode = false;
            } else {
                formula_exit_prompt_mode = true;
                formula_exit_selected_idx = 0;
            }
            return;
        }

        if (s.ctrl) {
            for (char c : s.word) {
                switch (c) {
                    case 'c': case 'C':
                        clipboard = formula_edit_buf;
                        return;
                    case 'x': case 'X':
                        clipboard = formula_edit_buf;
                        formula_edit_buf = "";
                        formula_cursor_pos = 0;
                        return;
                    case 'v': case 'V':
                        if (!clipboard.empty()) {
                            formula_edit_buf.insert(formula_cursor_pos, clipboard);
                            formula_cursor_pos += clipboard.size();
                        }
                        return;
                }
            }
        }

        if (s.left) {
            if (s.ctrl) formula_cursor_pos = getPrevWordPos(formula_edit_buf, formula_cursor_pos);
            else if (formula_cursor_pos > 0) formula_cursor_pos--;
            return;
        }
        if (s.right) {
            if (s.ctrl) formula_cursor_pos = getNextWordPos(formula_edit_buf, formula_cursor_pos);
            else if (formula_cursor_pos < (int)formula_edit_buf.size()) formula_cursor_pos++;
            return;
        }

        if (s.tab) {
            handleTabCompletion(formula_edit_buf, formula_cursor_pos);
            return;
        }

        if (s.enter) {
            if (!formula_edit_buf.empty()) {
                std::string input = formula_edit_buf;
                size_t eq = input.find('=');
                if (eq != std::string::npos) {
                    std::string lhs = input.substr(0, eq);
                    std::string rhs = input.substr(eq + 1);
                    lhs.erase(0, lhs.find_first_not_of(" \t")); lhs.erase(lhs.find_last_not_of(" \t") + 1);
                    rhs.erase(0, rhs.find_first_not_of(" \t")); rhs.erase(rhs.find_last_not_of(" \t") + 1);
                    std::string fname = lhs;
                    std::vector<std::string> params;
                    size_t open_p = lhs.find('(');
                    size_t close_p = lhs.find(')');
                    if (open_p != std::string::npos && close_p != std::string::npos && close_p > open_p) {
                        fname = lhs.substr(0, open_p);
                        fname.erase(0, fname.find_first_not_of(" \t")); fname.erase(fname.find_last_not_of(" \t") + 1);
                        std::string pstr = lhs.substr(open_p + 1, close_p - open_p - 1);
                        size_t start = 0;
                        while (start < pstr.size()) {
                            size_t comma = pstr.find(',', start);
                            std::string p = (comma == std::string::npos) ? pstr.substr(start) : pstr.substr(start, comma - start);
                            p.erase(0, p.find_first_not_of(" \t")); p.erase(p.find_last_not_of(" \t") + 1);
                            if (!p.empty()) params.push_back(p);
                            if (comma == std::string::npos) break;
                            start = comma + 1;
                        }
                    }
                    if (params.size() > 4) params.resize(4);
                    if (!fname.empty() && !rhs.empty() && formula_selected_idx >= 0 && formula_selected_idx < (int)user_formulas.size()) {
                        user_formulas[formula_selected_idx] = {fname, rhs, params};
                        saveNVSData();
                        saveSDData();
                    }
                }
            }
            formula_edit_mode = false;
            return;
        }
        if (s.del || (s.fn && s.backspace)) {
            if (s.ctrl) {
                int nw = getNextWordPos(formula_edit_buf, formula_cursor_pos);
                formula_edit_buf.erase(formula_cursor_pos, nw - formula_cursor_pos);
            } else if (formula_cursor_pos < (int)formula_edit_buf.size()) {
                formula_edit_buf.erase(formula_cursor_pos, 1);
            }
            return;
        }
        if (s.backspace) {
            if (s.ctrl) {
                int pw = getPrevWordPos(formula_edit_buf, formula_cursor_pos);
                formula_edit_buf.erase(pw, formula_cursor_pos - pw);
                formula_cursor_pos = pw;
            } else if (formula_cursor_pos > 0) {
                formula_edit_buf.erase(formula_cursor_pos - 1, 1);
                formula_cursor_pos--;
            }
            return;
        }
        for (char c : s.word) {
            if (c >= 32 && c < 127) {
                formula_edit_buf.insert(formula_cursor_pos, 1, c);
                formula_cursor_pos++;
            }
        }
        return;
    }

    if (formula_create_mode) {
        if (s.ctrl) {
            for (char c : s.word) {
                switch (c) {
                    case 'c': case 'C':
                        clipboard = formula_create_buf;
                        return;
                    case 'x': case 'X':
                        clipboard = formula_create_buf;
                        formula_create_buf = "";
                        formula_cursor_pos = 0;
                        return;
                    case 'v': case 'V':
                        if (!clipboard.empty()) {
                            formula_create_buf.insert(formula_cursor_pos, clipboard);
                            formula_cursor_pos += clipboard.size();
                        }
                        return;
                }
            }
        }

        if (s.left) {
            if (s.ctrl) formula_cursor_pos = getPrevWordPos(formula_create_buf, formula_cursor_pos);
            else if (formula_cursor_pos > 0) formula_cursor_pos--;
            return;
        }
        if (s.right) {
            if (s.ctrl) formula_cursor_pos = getNextWordPos(formula_create_buf, formula_cursor_pos);
            else if (formula_cursor_pos < (int)formula_create_buf.size()) formula_cursor_pos++;
            return;
        }

        if (s.tab) {
            handleTabCompletion(formula_create_buf, formula_cursor_pos);
            return;
        }

        if (s.enter) {
            if (!formula_create_buf.empty()) {
                std::string input = formula_create_buf;
                size_t eq = input.find('=');
                if (eq != std::string::npos) {
                    std::string lhs = input.substr(0, eq);
                    std::string rhs = input.substr(eq + 1);
                    lhs.erase(0, lhs.find_first_not_of(" \t")); lhs.erase(lhs.find_last_not_of(" \t") + 1);
                    rhs.erase(0, rhs.find_first_not_of(" \t")); rhs.erase(rhs.find_last_not_of(" \t") + 1);
                    
                    std::string fname = lhs;
                    std::vector<std::string> params;
                    size_t open_p = lhs.find('(');
                    size_t close_p = lhs.find(')');
                    if (open_p != std::string::npos && close_p != std::string::npos && close_p > open_p) {
                        fname = lhs.substr(0, open_p);
                        fname.erase(0, fname.find_first_not_of(" \t")); fname.erase(fname.find_last_not_of(" \t") + 1);
                        std::string pstr = lhs.substr(open_p + 1, close_p - open_p - 1);
                        size_t start = 0;
                        while (start < pstr.size()) {
                            size_t comma = pstr.find(',', start);
                            std::string p = (comma == std::string::npos) ? pstr.substr(start) : pstr.substr(start, comma - start);
                            p.erase(0, p.find_first_not_of(" \t")); p.erase(p.find_last_not_of(" \t") + 1);
                            if (!p.empty()) params.push_back(p);
                            if (comma == std::string::npos) break;
                            start = comma + 1;
                        }
                    }
                    if (params.size() > 4) params.resize(4);
                    if (!fname.empty() && !rhs.empty()) {
                        user_formulas.push_back({fname, rhs, params});
                        saveNVSData();
                        saveSDData();
                        formula_selected_idx = (int)user_formulas.size() - 1;
                    }
                }
            }
            formula_create_mode = false;
            return;
        }
        if (s.esc) {
            formula_create_mode = false;
            return;
        }
        if (s.del || (s.fn && s.backspace)) {
            if (s.ctrl) {
                int nw = getNextWordPos(formula_create_buf, formula_cursor_pos);
                formula_create_buf.erase(formula_cursor_pos, nw - formula_cursor_pos);
            } else if (formula_cursor_pos < (int)formula_create_buf.size()) {
                formula_create_buf.erase(formula_cursor_pos, 1);
            }
            return;
        }
        if (s.backspace) {
            if (s.ctrl) {
                int pw = getPrevWordPos(formula_create_buf, formula_cursor_pos);
                formula_create_buf.erase(pw, formula_cursor_pos - pw);
                formula_cursor_pos = pw;
            } else if (formula_cursor_pos > 0) {
                formula_create_buf.erase(formula_cursor_pos - 1, 1);
                formula_cursor_pos--;
            }
            return;
        }
        for (char c : s.word) {
            if (c >= 32 && c < 127) {
                formula_create_buf.insert(formula_cursor_pos, 1, c);
                formula_cursor_pos++;
            }
        }
        return;
    }

    if (formula_wizard_mode) {
        bool is_backtick = (std::find(s.word.begin(), s.word.end(), '`') != s.word.end()) || 
                           (std::find(s.word.begin(), s.word.end(), '~') != s.word.end());
        if (s.esc || is_backtick) {
            formula_wizard_mode = false;
            return;
        }

        if (formula_selected_idx >= 0 && formula_selected_idx < (int)user_formulas.size()) {
            const auto& f = user_formulas[formula_selected_idx];
            int p_count = std::min(4, (int)f.params.size());

            bool is_up = s.up || (std::find(s.word.begin(), s.word.end(), ';') != s.word.end());
            bool is_down = s.down || s.tab || (std::find(s.word.begin(), s.word.end(), '.') != s.word.end());

            if (is_up) {
                if (formula_wizard_param_idx > 0) {
                    formula_wizard_param_idx--;
                } else if (p_count > 0) {
                    formula_wizard_param_idx = p_count - 1; // Cyclic wrap to bottom
                }
                return;
            }
            if (is_down) {
                if (formula_wizard_param_idx < p_count - 1) {
                    formula_wizard_param_idx++;
                } else if (p_count > 0) {
                    formula_wizard_param_idx = 0; // Cyclic wrap to top
                }
                return;
            }

            auto evaluateWizardResult = [&]() {
                for (size_t i = 0; i < (size_t)p_count && i < formula_wizard_bufs.size(); ++i) {
                    std::string err;
                    double val = evaluate(formula_wizard_bufs[i].empty() ? "0" : formula_wizard_bufs[i], err);
                    if (!err.empty() || std::isnan(val)) val = 0.0;
                    formula_wizard_values[i] = val;
                    
                    user_args.erase(std::remove_if(user_args.begin(), user_args.end(), [&](const UserArg& a) {
                        return a.name == f.params[i];
                    }), user_args.end());
                    user_args.push_back({f.params[i], val});
                }
                
                std::string calc_err;
                double result = evaluate(f.expr, calc_err);
                if (!calc_err.empty() || std::isnan(result)) {
                    formula_wizard_result_str = calc_err.empty() ? "Math Error" : calc_err;
                    hasError = true;
                } else {
                    std::string args_fmt = "";
                    for (size_t i = 0; i < (size_t)p_count; ++i) {
                        args_fmt += fmtNum(formula_wizard_values[i]) + ",";
                    }
                    if (!args_fmt.empty()) args_fmt.pop_back();
                    std::string res_text = f.name + "(" + args_fmt + ") = " + fmtNum(result);
                    pushExprHistory(f.name);
                    pushResultHistory(result, res_text);
                    resultStr = res_text;
                    formula_wizard_result_str = res_text;
                    hasError = false;
                }
                formula_wizard_has_result = true;
            };

            if (s.enter) {
                evaluateWizardResult();
                if (formula_wizard_param_idx < p_count - 1) {
                    formula_wizard_param_idx++;
                }
                return;
            }
            if (s.del || s.backspace) {
                if (formula_wizard_param_idx < (int)formula_wizard_bufs.size() && !formula_wizard_bufs[formula_wizard_param_idx].empty()) {
                    formula_wizard_bufs[formula_wizard_param_idx].pop_back();
                    evaluateWizardResult();
                }
                return;
            }
            bool char_added = false;
            for (char c : s.word) {
                if (c >= 32 && c < 127) {
                    if (formula_wizard_param_idx < (int)formula_wizard_bufs.size()) {
                        formula_wizard_bufs[formula_wizard_param_idx] += c;
                        char_added = true;
                    }
                }
            }
            if (char_added) {
                evaluateWizardResult();
            }
        }
        return;
    } else {
        bool is_up = s.up || (std::find(s.word.begin(), s.word.end(), ';') != s.word.end());
        bool is_down = s.down || s.tab || (std::find(s.word.begin(), s.word.end(), '.') != s.word.end());

        if (is_up) {
            if (formula_selected_idx > 0) {
                formula_selected_idx--;
            } else if (!user_formulas.empty()) {
                formula_selected_idx = (int)user_formulas.size() - 1; // Cyclic top -> bottom
            }
            return;
        }
        if (is_down) {
            if (formula_selected_idx < (int)user_formulas.size() - 1) {
                formula_selected_idx++;
            } else if (!user_formulas.empty()) {
                formula_selected_idx = 0; // Cyclic bottom -> top
            }
            return;
        }
        for (char c : s.word) {
            if (c == 'n' || c == 'N') {
                formula_create_mode = true;
                formula_create_buf = "";
                formula_cursor_pos = 0;
                return;
            }
            if (c == 'e' || c == 'E') {
                if (!user_formulas.empty() && formula_selected_idx >= 0 && formula_selected_idx < (int)user_formulas.size()) {
                    const auto& f = user_formulas[formula_selected_idx];
                    std::string pstr = "";
                    for (size_t p = 0; p < f.params.size() && p < 4; ++p) {
                        pstr += f.params[p] + ",";
                    }
                    if (!pstr.empty()) pstr.pop_back();
                    formula_edit_buf = f.name + "(" + pstr + ") = " + f.expr;
                    formula_edit_orig_buf = formula_edit_buf;
                    formula_cursor_pos = formula_edit_buf.size();
                    formula_edit_mode = true;
                    formula_exit_prompt_mode = false;
                    formula_exit_selected_idx = 0;
                }
                return;
            }
        }
        if (s.del || s.backspace) {
            if (!user_formulas.empty() && formula_selected_idx >= 0 && formula_selected_idx < (int)user_formulas.size()) {
                delete_confirm_prompt_mode = true;
                delete_confirm_selected_idx = 1;
            }
            return;
        }
        if (s.enter && !user_formulas.empty()) {
            if (formula_selected_idx >= 0 && formula_selected_idx < (int)user_formulas.size()) {
                Formula f = user_formulas[formula_selected_idx];
                if (f.params.empty()) {
                    std::string calc_err;
                    double result = evaluate(f.expr, calc_err);
                    if (!calc_err.empty() || std::isnan(result)) {
                        resultStr = calc_err.empty() ? "Math Error" : calc_err;
                        hasError = true;
                    } else {
                        std::string res_text = f.name + " = " + fmtNum(result);
                        pushExprHistory(f.name);
                        pushResultHistory(result, res_text);
                        resultStr = res_text;
                        hasError = false;
                    }
                } else {
                    formula_wizard_mode = true;
                    formula_wizard_param_idx = 0;
                    int p_count = std::min(4, (int)f.params.size());
                    formula_wizard_values.assign(p_count, 0.0);
                    formula_wizard_bufs.assign(p_count, "");
                    formula_wizard_has_result = false;
                    formula_wizard_result_str = "";
                }
            }
            return;
        }
        if (s.esc) {
            navigateBack();
        }
    }
}

static int param_selected_idx = 0;
static bool param_edit_mode = false;
static std::string param_edit_buf = "";

static void drawParams() {
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    drawStatusBar("Cardulator | PARAMETERS");
    
    int y = 30;
    
    // Setting 0: Screen Timeout
    if (param_selected_idx == 0) {
        M5Cardputer.Display.setTextColor(TFT_YELLOW);
        M5Cardputer.Display.fillRect(0, y - 2, SCR_W, 16, 0x18E3 /* dark grey */);
    } else {
        M5Cardputer.Display.setTextColor(TFT_WHITE);
    }
    M5Cardputer.Display.setCursor(10, y);
    M5Cardputer.Display.printf("Screen Timeout: %d s", screen_off_timeout);
    if (param_selected_idx == 0 && param_edit_mode) {
        M5Cardputer.Display.setTextColor(TFT_CYAN);
        M5Cardputer.Display.setCursor(160, y);
        M5Cardputer.Display.printf("[%s]", param_edit_buf.c_str());
    }
    
    y += 20;
    
    // Setting 1: Brightness
    if (param_selected_idx == 1) {
        M5Cardputer.Display.setTextColor(TFT_YELLOW);
        M5Cardputer.Display.fillRect(0, y - 2, SCR_W, 16, 0x18E3 /* dark grey */);
    } else {
        M5Cardputer.Display.setTextColor(TFT_WHITE);
    }
    M5Cardputer.Display.setCursor(10, y);
    M5Cardputer.Display.printf("Brightness: %d", backlight_brightness);
    if (param_selected_idx == 1 && param_edit_mode) {
        M5Cardputer.Display.setTextColor(TFT_CYAN);
        M5Cardputer.Display.setCursor(160, y);
        M5Cardputer.Display.printf("[%s]", param_edit_buf.c_str());
    }
    
    // Draw visual progress bar for brightness
    M5Cardputer.Display.drawRect(10, y + 16, 220, 8, TFT_WHITE);
    M5Cardputer.Display.fillRect(11, y + 17, (backlight_brightness * 218) / 255, 6, TFT_GREEN);
    
    y += 30;

    // Setting 2: Thousands Separator
    if (param_selected_idx == 2) {
        M5Cardputer.Display.setTextColor(TFT_YELLOW);
        M5Cardputer.Display.fillRect(0, y - 2, SCR_W, 16, 0x18E3 /* dark grey */);
    } else {
        M5Cardputer.Display.setTextColor(TFT_WHITE);
    }
    M5Cardputer.Display.setCursor(10, y);
    M5Cardputer.Display.printf("Thousands Sep: %s", use_thousands_sep ? "ON" : "OFF");

    y += 18;

    // Setting 3: Auto Brackets
    if (param_selected_idx == 3) {
        M5Cardputer.Display.setTextColor(TFT_YELLOW);
        M5Cardputer.Display.fillRect(0, y - 2, SCR_W, 16, 0x18E3 /* dark grey */);
    } else {
        M5Cardputer.Display.setTextColor(TFT_WHITE);
    }
    M5Cardputer.Display.setCursor(10, y);
    M5Cardputer.Display.printf("Auto Brackets: %s", auto_brackets ? "ON" : "OFF");

    y += 18;

    // Setting 4: Sticky Mod
    if (param_selected_idx == 4) {
        M5Cardputer.Display.setTextColor(TFT_YELLOW);
        M5Cardputer.Display.fillRect(0, y - 2, SCR_W, 16, 0x18E3 /* dark grey */);
    } else {
        M5Cardputer.Display.setTextColor(TFT_WHITE);
    }
    M5Cardputer.Display.setCursor(10, y);
    M5Cardputer.Display.printf("Sticky Mod: %s", sticky_mode ? "ON" : "OFF");
}

static void handleParamsKey(Keyboard_Class::KeysState& s) {
    if (param_edit_mode) {
        if (s.enter) {
            try {
                int val = std::stoi(param_edit_buf);
                if (param_selected_idx == 0) {
                    if (val >= 0) screen_off_timeout = val;
                } else if (param_selected_idx == 1) {
                    if (val >= 0 && val <= 255) {
                        backlight_brightness = val;
                        M5Cardputer.Display.setBrightness(backlight_brightness);
                    }
                }
                saveNVSData();
            } catch (...) {}
            param_edit_mode = false;
        } else if (s.del || s.backspace) {
            if (!param_edit_buf.empty()) param_edit_buf.pop_back();
        } else if (s.esc) {
            param_edit_mode = false;
        } else {
            for (char c : s.word) {
                if (std::isdigit(c)) param_edit_buf += c;
            }
        }
        return;
    }
    
    bool is_up = s.up;
    bool is_down = s.down || s.tab;
    if (is_up) {
        if (param_selected_idx > 0) param_selected_idx--;
        else param_selected_idx = 4; // Cyclic top -> bottom
    } else if (is_down) {
        if (param_selected_idx < 4) param_selected_idx++;
        else param_selected_idx = 0; // Cyclic bottom -> top
    } else if (s.left || s.right) {
        if (param_selected_idx == 0) {
            if (s.left) screen_off_timeout = std::max(0, screen_off_timeout - 5);
            else screen_off_timeout += 5;
        } else if (param_selected_idx == 1) {
            if (s.left) backlight_brightness = std::max(0, backlight_brightness - 15);
            else backlight_brightness = std::min(255, backlight_brightness + 15);
            M5Cardputer.Display.setBrightness(backlight_brightness);
        } else if (param_selected_idx == 2) {
            use_thousands_sep = !use_thousands_sep;
        } else if (param_selected_idx == 3) {
            auto_brackets = !auto_brackets;
        } else if (param_selected_idx == 4) {
            sticky_mode = !sticky_mode;
        }
        saveNVSData();
    } else if (s.enter) {
        if (param_selected_idx == 2) {
            use_thousands_sep = !use_thousands_sep;
            saveNVSData();
        } else if (param_selected_idx == 3) {
            auto_brackets = !auto_brackets;
            saveNVSData();
        } else if (param_selected_idx == 4) {
            sticky_mode = !sticky_mode;
            saveNVSData();
        } else {
            param_edit_mode = true;
            if (param_selected_idx == 0) param_edit_buf = std::to_string(screen_off_timeout);
            else if (param_selected_idx == 1) param_edit_buf = std::to_string(backlight_brightness);
        }
    } else if (s.esc) {
        navigateBack();
    }
}

static void handlePlotKey(Keyboard_Class::KeysState& s) {
    double move_step = (plot_scale / 10.0) * (s.ctrl ? 5.0 : 1.0);
    double zoom_in_factor = s.ctrl ? pow(1.2, 5) : 1.2;
    double zoom_out_factor = s.ctrl ? pow(1.2, 5) : 1.2;

    if (s.up) {
        plot_center_y += move_step;
        return;
    }
    if (s.down) {
        plot_center_y -= move_step;
        return;
    }
    if (s.left) {
        plot_center_x -= move_step;
        return;
    }
    if (s.right) {
        plot_center_x += move_step;
        return;
    }
    if (s.esc) {
        navigateBack();
        return;
    }
    for (char c : s.word) {
        if (c == '-' || c == '_') {
            plot_scale *= zoom_out_factor;
        } else if (c == '=' || c == '+' || c == ',' || (s.fn && c == ',')) {
            plot_scale /= zoom_in_factor;
        } else if (c == '.' || (s.fn && c == '.')) {
            plot_scale *= zoom_out_factor;
        } else if (c == 'a' || c == 'A') {
            plot_center_x = 0.0;
            plot_center_y = 0.0;
            plot_scale = 1.0;
            plot_manual_limits = false;
        }
    }
}

static void handleScriptsKey(Keyboard_Class::KeysState& s) {
    if (delete_confirm_prompt_mode) {
        bool is_nav = s.left || s.right || s.up || s.down || s.tab;
        if (is_nav) {
            delete_confirm_selected_idx = 1 - delete_confirm_selected_idx;
            return;
        }
        for (char c : s.word) {
            if (c == 'y' || c == 'Y') {
                if (!user_scripts.empty() && script_selected_idx < (int)user_scripts.size()) {
                    user_scripts.erase(user_scripts.begin() + script_selected_idx);
                    saveNVSData();
                    if (script_selected_idx >= (int)user_scripts.size()) {
                        script_selected_idx = std::max(0, (int)user_scripts.size() - 1);
                    }
                }
                delete_confirm_prompt_mode = false;
                return;
            }
            if (c == 'n' || c == 'N') {
                delete_confirm_prompt_mode = false;
                return;
            }
        }
        if (s.enter) {
            if (delete_confirm_selected_idx == 0) {
                if (!user_scripts.empty() && script_selected_idx < (int)user_scripts.size()) {
                    user_scripts.erase(user_scripts.begin() + script_selected_idx);
                    saveNVSData();
                    if (script_selected_idx >= (int)user_scripts.size()) {
                        script_selected_idx = std::max(0, (int)user_scripts.size() - 1);
                    }
                }
            }
            delete_confirm_prompt_mode = false;
            return;
        }
        if (s.esc) {
            delete_confirm_prompt_mode = false;
            return;
        }
        return;
    }
    if (script_running_mode) {
        if (s.up) {
            script_console_scroll_offset++;
            return;
        }
        if (s.down) {
            if (script_console_scroll_offset > 0) script_console_scroll_offset--;
            return;
        }
        if (s.esc || s.enter || !s.word.empty()) {
            script_running_mode = false;
        }
        return;
    }
    if (script_edit_mode) {
        if (script_exit_prompt_mode) {
            if (s.left || s.right || s.up || s.down) {
                script_exit_selected_idx = 1 - script_exit_selected_idx;
                return;
            }
            for (char c : s.word) {
                if (c == 'y' || c == 'Y') {
                    user_scripts[script_selected_idx].content = script_edit_buf;
                    saveNVSData();
                    saveSDData();
                    script_exit_prompt_mode = false;
                    script_edit_mode = false;
                    return;
                }
                if (c == 'n' || c == 'N') {
                    script_exit_prompt_mode = false;
                    script_edit_mode = false;
                    return;
                }
            }
            if (s.enter) {
                if (script_exit_selected_idx == 0) {
                    user_scripts[script_selected_idx].content = script_edit_buf;
                    saveNVSData();
                    saveSDData();
                }
                script_exit_prompt_mode = false;
                script_edit_mode = false;
                return;
            }
            if (s.esc) {
                script_exit_prompt_mode = false;
                return;
            }
            return;
        }

        if (s.esc) {
            if (script_edit_buf == script_edit_orig_buf) {
                script_edit_mode = false;
            } else {
                script_exit_prompt_mode = true;
                script_exit_selected_idx = 0;
            }
            return;
        }

        // 1. Ctrl Shortcuts in Script Editor
        if (s.ctrl) {
            if (s.up || (s.fn && s.left)) {
                script_cursor_pos = 0;
                return;
            }
            if (s.down || (s.fn && s.right)) {
                script_cursor_pos = script_edit_buf.size();
                return;
            }
            for (char c : s.word) {
                switch (c) {
                    case 'a': case 'A':
                        script_cursor_pos = script_edit_buf.size();
                        return;
                    case 'c': case 'C': {
                        int lstart = getLineStart(script_edit_buf, script_cursor_pos);
                        int lend = getLineEnd(script_edit_buf, script_cursor_pos);
                        clipboard = script_edit_buf.substr(lstart, lend - lstart);
                        return;
                    }
                    case 'x': case 'X': {
                        int lstart = getLineStart(script_edit_buf, script_cursor_pos);
                        int lend = getLineEnd(script_edit_buf, script_cursor_pos);
                        if (lend < (int)script_edit_buf.size() && script_edit_buf[lend] == '\n') lend++;
                        clipboard = script_edit_buf.substr(lstart, lend - lstart);
                        script_edit_buf.erase(lstart, lend - lstart);
                        script_cursor_pos = std::min(lstart, (int)script_edit_buf.size());
                        return;
                    }
                    case 'v': case 'V': {
                        if (!clipboard.empty()) {
                            script_edit_buf.insert(script_cursor_pos, clipboard);
                            script_cursor_pos += clipboard.size();
                        }
                        return;
                    }
                }
            }
        }

        // 2. Fn Shortcuts (Home / End / Fn+Enter Run)
        if (s.fn) {
            if (s.left && !s.ctrl) {
                script_cursor_pos = getLineStart(script_edit_buf, script_cursor_pos);
                return;
            }
            if (s.right && !s.ctrl) {
                script_cursor_pos = getLineEnd(script_edit_buf, script_cursor_pos);
                return;
            }
            if (s.enter) {
                user_scripts[script_selected_idx].content = script_edit_buf;
                saveSDData();
                script_console_output.clear();
                script_console_scroll_offset = 0;
                runScript(script_edit_buf);
                script_edit_mode = false;
                script_running_mode = true;
                drawScripts();
                return;
            }
            for (char c : s.word) {
                if (c == 'l' || c == 'L') {
                    if (s.ctrl) script_cursor_pos = 0;
                    else script_cursor_pos = getLineStart(script_edit_buf, script_cursor_pos);
                    return;
                }
                if (c == '\'' || c == '"') {
                    if (s.ctrl) script_cursor_pos = script_edit_buf.size();
                    else script_cursor_pos = getLineEnd(script_edit_buf, script_cursor_pos);
                    return;
                }
            }
        }

        // 3. Arrow Keys Navigation
        if (s.left) {
            if (s.ctrl) {
                script_cursor_pos = getPrevWordPos(script_edit_buf, script_cursor_pos);
            } else {
                if (script_cursor_pos > 0) script_cursor_pos--;
            }
            return;
        }
        if (s.right) {
            if (s.ctrl) {
                script_cursor_pos = getNextWordPos(script_edit_buf, script_cursor_pos);
            } else {
                if (script_cursor_pos < (int)script_edit_buf.size()) script_cursor_pos++;
            }
            return;
        }
        if (s.up) {
            int lstart = getLineStart(script_edit_buf, script_cursor_pos);
            if (lstart > 0) {
                int col = script_cursor_pos - lstart;
                int prev_lstart = getLineStart(script_edit_buf, lstart - 1);
                int prev_lend = lstart - 1;
                script_cursor_pos = std::min(prev_lstart + col, prev_lend);
            }
            return;
        }
        if (s.down) {
            int lend = getLineEnd(script_edit_buf, script_cursor_pos);
            if (lend < (int)script_edit_buf.size()) {
                int lstart = getLineStart(script_edit_buf, script_cursor_pos);
                int col = script_cursor_pos - lstart;
                int next_lstart = lend + 1;
                int next_lend = getLineEnd(script_edit_buf, next_lstart);
                script_cursor_pos = std::min(next_lstart + col, next_lend);
            }
            return;
        }

        if (s.tab) {
            handleTabCompletion(script_edit_buf, script_cursor_pos);
            return;
        }
        if (s.enter) {
            script_edit_buf.insert(script_cursor_pos, "\n");
            script_cursor_pos++;
            return;
        }

        // 4. Delete & Backspace
        if (s.del || (s.fn && s.backspace)) {
            if (s.ctrl) {
                int nw = getNextWordPos(script_edit_buf, script_cursor_pos);
                script_edit_buf.erase(script_cursor_pos, nw - script_cursor_pos);
            } else if (script_cursor_pos < (int)script_edit_buf.size()) {
                script_edit_buf.erase(script_cursor_pos, 1);
            }
            return;
        }
        if (s.backspace) {
            if (s.ctrl) {
                int pw = getPrevWordPos(script_edit_buf, script_cursor_pos);
                script_edit_buf.erase(pw, script_cursor_pos - pw);
                script_cursor_pos = pw;
            } else if (script_cursor_pos > 0) {
                script_edit_buf.erase(script_cursor_pos - 1, 1);
                script_cursor_pos--;
            }
            return;
        }

        // 5. Character Insertion
        for (char c : s.word) {
            if (c >= 32 && c < 127) {
                bool handled = false;
                if (auto_brackets) {
                    if ((c == ')' || c == ']' || c == '}' || c == '\'' || c == '"') &&
                        script_cursor_pos < (int)script_edit_buf.size() && script_edit_buf[script_cursor_pos] == c) {
                        script_cursor_pos++;
                        handled = true;
                    } else if (c == '(' || c == '[' || c == '{' || c == '\'' || c == '"') {
                        char close_c = 0;
                        if (c == '(') close_c = ')';
                        else if (c == '[') close_c = ']';
                        else if (c == '{') close_c = '}';
                        else close_c = c;
                        
                        std::string to_ins = std::string(1, c) + close_c;
                        script_edit_buf.insert(script_cursor_pos, to_ins);
                        script_cursor_pos++;
                        handled = true;
                    }
                }
                if (!handled) {
                    script_edit_buf.insert(script_cursor_pos, 1, c);
                    script_cursor_pos++;
                }
            }
        }
        return;
    } else if (script_name_prompt_mode) {
        if (s.esc) {
            script_name_prompt_mode = false;
            return;
        }
        if (s.backspace || s.del) {
            if (!script_name_edit_buf.empty()) script_name_edit_buf.pop_back();
            return;
        }
        if (s.enter) {
            if (is_renaming_script) {
                if (!script_name_edit_buf.empty() && !user_scripts.empty()) {
                    user_scripts[script_selected_idx].name = script_name_edit_buf;
                    saveNVSData();
                }
                script_name_prompt_mode = false;
            } else {
                std::string new_name = script_name_edit_buf.empty() ? ("script_" + std::to_string(user_scripts.size() + 1)) : script_name_edit_buf;
                user_scripts.push_back({new_name, ""});
                script_selected_idx = user_scripts.size() - 1;
                script_edit_buf = "";
                script_edit_orig_buf = "";
                script_cursor_pos = 0;
                script_edit_mode = true;
                script_name_prompt_mode = false;
            }
            return;
        }
        for (char c : s.word) {
            if (std::isalnum(c) || c == '_') {
                script_name_edit_buf += c;
            }
        }
        return;
    } else {
        bool is_up = s.up;
        bool is_down = s.down || s.tab;
        if (is_up) {
            if (script_selected_idx > 0) {
                script_selected_idx--;
            } else if (!user_scripts.empty()) {
                script_selected_idx = (int)user_scripts.size() - 1; // Cyclic top -> bottom
            }
            return;
        }
        if (is_down) {
            if (script_selected_idx < (int)user_scripts.size() - 1) {
                script_selected_idx++;
            } else if (!user_scripts.empty()) {
                script_selected_idx = 0; // Cyclic bottom -> top
            }
            return;
        }
        if (s.enter && !user_scripts.empty()) {
            script_running_mode = true;
            runScript(user_scripts[script_selected_idx].content);
            return;
        }
        if (s.del || s.backspace) {
            if (!user_scripts.empty() && script_selected_idx < (int)user_scripts.size()) {
                delete_confirm_prompt_mode = true;
                delete_confirm_selected_idx = 1;
            }
            return;
        }
        for (char c : s.word) {
            if (c == 'e' || c == 'E') {
                if (!user_scripts.empty()) {
                    script_edit_mode = true;
                    script_edit_buf = user_scripts[script_selected_idx].content;
                    script_edit_orig_buf = script_edit_buf;
                    script_cursor_pos = 0;
                }
                return;
            }
            if (c == 'r' || c == 'R') {
                if (!user_scripts.empty()) {
                    script_name_prompt_mode = true;
                    is_renaming_script = true;
                    script_name_edit_buf = user_scripts[script_selected_idx].name;
                }
                return;
            }
            if (c == 'n' || c == 'N' || c == 'c' || c == 'C') {
                script_name_prompt_mode = true;
                is_renaming_script = false;
                script_name_edit_buf = "script_" + std::to_string(user_scripts.size() + 1);
                return;
            }
        }
        if (s.esc) {
            navigateBack();
        }
    }
}

static std::vector<AppState> app_state_stack;

static void resetAppState(AppState newState) {
    if (appState != newState) {
        app_state_stack.push_back(appState);
        appState = newState;
    }
    if (newState == STATE_VARS) {
        var_selected_idx = 0;
        var_edit_mode = false;
    } else if (newState == STATE_SCRIPTS) {
        script_selected_idx = 0;
        script_edit_mode = false;
        script_running_mode = false;
    } else if (newState == STATE_BINDS) {
        bind_selected_idx = 0;
        bind_edit_mode = false;
    } else if (newState == STATE_FORMULAS) {
        formula_selected_idx = 0;
        formula_wizard_mode = false;
    } else if (newState == STATE_PLOT) {
        if (!expression.empty()) plot_expr = expression;
        plot_center_x = 0.0;
        plot_center_y = 0.0;
        plot_scale = 10.0;
    } else if (newState == STATE_PARAMS) {
        param_selected_idx = 0;
        param_edit_mode = false;
    }
}

static void navigateBack() {
    if (!app_state_stack.empty()) {
        AppState prev = app_state_stack.back();
        app_state_stack.pop_back();
        appState = prev;
    } else {
        appState = STATE_CALC;
    }
}

static void blinkLED(int count) {
    for (int i = 0; i < count; i++) {
        digitalWrite(4, HIGH);
        delay(150);
        digitalWrite(4, LOW);
        delay(150);
    }
    delay(300);
}

void setup() {
    #if defined(ARDUINO)
    Serial.begin(115200);
    #endif
    #ifdef ARDUINO
    // Keep LED power rail always enabled; LED is controlled via NeoPixel data only
    pinMode(38, OUTPUT);
    digitalWrite(38, HIGH);
    #endif
    pinMode(4, OUTPUT);
    // blinkLED(1); // Stage 1: entered setup
    
    auto cfg = M5.config();
    // Do not initialize keyboard automatically to prevent hanging on standard Cardputer
    M5Cardputer.begin(cfg, false);
    
    // blinkLED(2); // Stage 2: M5Cardputer.begin completed
    
    // Determine the actual board/keyboard by probing the TCA8418 on internal I2C.
    // If the board was auto-detected as Cardputer ADV, double check it.
    bool is_adv = false;
    if (M5.getBoard() == m5::board_t::board_M5CardputerADV) {
        // Probe for TCA8418 address 0x34
        if (M5.In_I2C.start(0x34, false, 100000)) {
            M5.In_I2C.stop();
            is_adv = true;
        }
    }
    
    // blinkLED(3); // Stage 3: I2C probe completed
    
    if (is_adv) {
        M5Cardputer.Keyboard.begin(std::make_unique<TCA8418KeyboardReader>());
    } else {
        M5Cardputer.Keyboard.begin(std::make_unique<IOMatrixKeyboardReader>());
    }
    
    // blinkLED(4); // Stage 4: Keyboard reader initialized
    
    // blinkLED(5); // Stage 5: Ready for NVS data load
    
    // Initialize SD Card
    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
    if (SD.begin(SD_SPI_CS_PIN, SPI, 25000000)) {
        sd_initialized = true;
        if (!SD.exists("/Cardulator")) {
            SD.mkdir("/Cardulator");
            SD.mkdir("/Cardulator/scripts");
            
            // Save initial variables
            File f = SD.open("/Cardulator/variables.txt", FILE_WRITE);
            if (f) {
                f.println("temp=25");
                f.close();
            }
            // Save initial binds
            f = SD.open("/Cardulator/binds.txt", FILE_WRITE);
            if (f) {
                f.println("s:sin(");
                f.println("p:pi");
                f.close();
            }
            // Save initial script
            f = SD.open("/Cardulator/scripts/fib.txt", FILE_WRITE);
            if (f) {
                f.print("x = 1\ny = 1\nfor (i = 1; i <= 10; i++) {\n  print(x)\n  tmp = x + y\n  x = y\n  y = tmp\n}\n");
                f.close();
            }
            // Save initial formulas
            f = SD.open("/Cardulator/formulas.txt", FILE_WRITE);
            if (f) {
                f.println("hypot|sqrt(x^2 + y^2)|x,y;");
                f.println("area|pi * r^2|r;");
                f.close();
            }
            // Save initial settings
            f = SD.open("/Cardulator/settings.txt", FILE_WRITE);
            if (f) {
                f.println("scr_off=30");
                f.println("brightness=128");
                f.println("thousands_sep=0");
                f.close();
            }
        }
    }
    
    if (sd_initialized) {
        loadSDData();
        // blinkLED(6); // Stage 6: SD data loaded
    } else {
        loadNVSData();
        // blinkLED(6); // Stage 6: NVS data loaded
    }
    
    M5Cardputer.Display.setBrightness(backlight_brightness);
    last_activity_time = millis();
    screen_is_on = true;

    M5Cardputer.Display.setRotation(1);
#ifdef ARDUINO
    canvas.createSprite(SCR_W, SCR_H);
#endif
    // Math & Calculator Splash Screen (Animated Grid, Plot Curve & Glowing Title)
    {
        uint16_t DARK_BLUE = 0x084A;
        uint16_t DARK_CYAN = 0x0210;
        
#ifdef ARDUINO
        M5Canvas splashSpr(&M5Cardputer.Display);
        bool hasSpr = splashSpr.createSprite(SCR_W, SCR_H);
        M5GFX* displayTarget = hasSpr ? (M5GFX*)&splashSpr : (M5GFX*)&M5Cardputer.Display;
#endif

        for (int frame = 0; frame < 18; ++frame) {
#ifdef ARDUINO
            displayTarget->fillScreen(TFT_BLACK);
            
            // Draw background mathematical coordinate grid
            for (int x = 0; x <= SCR_W; x += 24) {
                displayTarget->drawFastVLine(x, 0, SCR_H, DARK_CYAN);
            }
            for (int y = 0; y <= SCR_H; y += 18) {
                displayTarget->drawFastHLine(0, y, SCR_W, DARK_CYAN);
            }
            // Axis lines
            displayTarget->drawFastHLine(0, 75, SCR_W, DARK_BLUE);
            displayTarget->drawFastVLine(120, 0, SCR_H, DARK_BLUE);

            // Animated Sine Wave Plot Curve (Math symbol)
            int prev_px = -1, prev_py = -1;
            float phase = frame * 0.3f;
            for (int px = 0; px < SCR_W; px += 3) {
                float x_val = (px - 120) * 0.05f;
                float y_val = sinf(x_val * 2.0f + phase) * cosf(x_val * 0.5f);
                int py = 75 - (int)(y_val * 28.0f);
                if (prev_px != -1 && py >= 0 && py < SCR_H) {
                    displayTarget->drawLine(prev_px, prev_py, px, py, TFT_MAGENTA);
                }
                prev_px = px;
                prev_py = py;
            }

            // Glowing Cardulator Title Box
            int box_w = 170, box_h = 42;
            int box_x = (SCR_W - box_w) / 2;
            int box_y = 18;
            displayTarget->fillRect(box_x, box_y, box_w, box_h, 0x0808 /* dark purple/navy */);
            displayTarget->drawRect(box_x, box_y, box_w, box_h, TFT_CYAN);
            displayTarget->drawRect(box_x - 1, box_y - 1, box_w + 2, box_h + 2, TFT_BLUE);

            displayTarget->setTextDatum(middle_center);
            displayTarget->setTextColor(TFT_CYAN);
            displayTarget->setTextSize(2);
            displayTarget->drawString("CARDULATOR", SCR_W / 2, box_y + 14);

            displayTarget->setTextSize(1);
            displayTarget->setTextColor(TFT_YELLOW);
            displayTarget->drawString("ADVANCED MATH & PLOT ENGINE", SCR_W / 2, box_y + 30);

            // Subtitle & Help Notice
            displayTarget->setTextDatum(bottom_center);
            displayTarget->setTextColor(TFT_WHITE);
            displayTarget->drawString("Press Fn+H for Help Overlay", SCR_W / 2, SCR_H - 14);

            displayTarget->setTextColor(TFT_DARKGREY);
            displayTarget->drawString("v" VERSION "  |  aroum", SCR_W / 2, SCR_H - 3);

            displayTarget->setTextDatum(top_left);
#else
            M5Cardputer.Display.fillScreen(TFT_BLACK);
            
            // Draw background mathematical coordinate grid
            for (int x = 0; x <= SCR_W; x += 24) {
                M5Cardputer.Display.drawFastVLine(x, 0, SCR_H, DARK_CYAN);
            }
            for (int y = 0; y <= SCR_H; y += 18) {
                M5Cardputer.Display.drawFastHLine(0, y, SCR_W, DARK_CYAN);
            }
            // Axis lines
            M5Cardputer.Display.drawFastHLine(0, 75, SCR_W, DARK_BLUE);
            M5Cardputer.Display.drawFastVLine(120, 0, SCR_H, DARK_BLUE);

            // Animated Sine Wave Plot Curve (Math symbol)
            int prev_px = -1, prev_py = -1;
            float phase = frame * 0.3f;
            for (int px = 0; px < SCR_W; px += 3) {
                float x_val = (px - 120) * 0.05f;
                float y_val = sinf(x_val * 2.0f + phase) * cosf(x_val * 0.5f);
                int py = 75 - (int)(y_val * 28.0f);
                if (prev_px != -1 && py >= 0 && py < SCR_H) {
                    M5Cardputer.Display.drawLine(prev_px, prev_py, px, py, TFT_MAGENTA);
                }
                prev_px = px;
                prev_py = py;
            }

            // Glowing Cardulator Title Box
            int box_w = 170, box_h = 42;
            int box_x = (SCR_W - box_w) / 2;
            int box_y = 18;
            M5Cardputer.Display.fillRect(box_x, box_y, box_w, box_h, 0x0808 /* dark purple/navy */);
            M5Cardputer.Display.drawRect(box_x, box_y, box_w, box_h, TFT_CYAN);
            M5Cardputer.Display.drawRect(box_x - 1, box_y - 1, box_w + 2, box_h + 2, TFT_BLUE);

            M5Cardputer.Display.setTextDatum(middle_center);
            M5Cardputer.Display.setTextColor(TFT_CYAN);
            M5Cardputer.Display.setTextSize(2);
            M5Cardputer.Display.drawString("CARDULATOR", SCR_W / 2, box_y + 14);

            M5Cardputer.Display.setTextSize(1);
            M5Cardputer.Display.setTextColor(TFT_YELLOW);
            M5Cardputer.Display.drawString("ADVANCED MATH & PLOT ENGINE", SCR_W / 2, box_y + 30);

            // Subtitle & Help Notice
            M5Cardputer.Display.setTextDatum(bottom_center);
            M5Cardputer.Display.setTextColor(TFT_WHITE);
            M5Cardputer.Display.drawString("Press Fn+H for Help Overlay", SCR_W / 2, SCR_H - 14);

            M5Cardputer.Display.setTextColor(TFT_DARKGREY);
            M5Cardputer.Display.drawString("v" VERSION "  |  aroum", SCR_W / 2, SCR_H - 3);

            M5Cardputer.Display.setTextDatum(top_left);
#endif

#ifdef ARDUINO
            if (hasSpr) {
                splashSpr.pushSprite(0, 0);
            }
#endif
            delay(40);
        }
#ifdef ARDUINO
        if (hasSpr) splashSpr.deleteSprite();
#endif
        delay(200);
    }
    drawCalc();
}

static void processKeyEvent(Keyboard_Class::KeysState s) {
    // Global Fn Mode Switching and Fn+H popup
    if (s.fn) {
        bool handled = false;
        for (char c : s.word) {
            switch (c) {
                case 'v': case 'V': resetAppState(STATE_VARS); handled = true; break;
                case 's': case 'S': resetAppState(STATE_SCRIPTS); handled = true; break;
                case 'g': case 'G': resetAppState(STATE_PLOT); handled = true; break;
                case 'b': case 'B': resetAppState(STATE_BINDS); handled = true; break;
                case 'f': case 'F': resetAppState(STATE_FORMULAS); handled = true; break;
                case 'h': case 'H': show_help_popup = true; drawHelpPopup(); return;
                case 'p': case 'P': resetAppState(STATE_PARAMS); handled = true; break;
                case 'c': case 'C': resetAppState(STATE_CONSTS); handled = true; break;
            }
        }
        if (handled) return;
    }

    // Direct Arrow and Esc Navigation Shortcuts (without Fn) for Non-REPL Screens
    bool in_edit_mode = false;
    if (appState == STATE_VARS && var_edit_mode) in_edit_mode = true;
    if (appState == STATE_PARAMS && param_edit_mode) in_edit_mode = true;
    if (appState == STATE_SCRIPTS && script_edit_mode) in_edit_mode = true;
    if (appState == STATE_FORMULAS && (formula_wizard_mode || formula_create_mode || formula_edit_mode)) in_edit_mode = true;

    for (char c : s.word) {
        if (c == '`' || c == '~') s.esc = true;
        if (appState == STATE_CALC || in_edit_mode) {
            // In REPL and text edit modes, arrow shortcuts require Fn key! Plain ;, ., ,, / type literal characters
            if (s.fn) {
                if (c == ';') s.up = true;
                if (c == '.') s.down = true;
                if (c == ',') s.left = true;
                if (c == '/') s.right = true;
            }
        } else {
            if (c == ';') s.up = true;
            if (c == '.') s.down = true;
            if (c == ',') s.left = true;
            if (c == '/') s.right = true;
            if (c == '[') s.esc = true;
            if (appState != STATE_PLOT) {
                if (c == 'w' || c == 'W') s.up = true;
                if (c == 's' || c == 'S') s.down = true;
                if (c == 'a' || c == 'A') s.left = true;
                if (c == 'd' || c == 'D') s.right = true;
            }
        }
    }

    M5Cardputer.Display.startWrite();
    switch (appState) {
        case STATE_CALC:
            handleCalcKey(s);
            drawCalc();
            break;
            
        case STATE_HELP:
            if (s.word.size() > 0 || s.enter || s.del || s.fn || s.tab || s.esc) {
                navigateBack();
            }
            drawHelpView();
            break;
            
        case STATE_VARS:
            handleVarsKey(s);
            drawVars();
            break;
            
        case STATE_SCRIPTS:
            handleScriptsKey(s);
            drawScripts();
            break;
            
        case STATE_PLOT:
            handlePlotKey(s);
            drawPlot();
            break;
            
        case STATE_BINDS:
            handleBindsKey(s);
            drawBinds();
            break;
            
        case STATE_FORMULAS:
            handleFormulasKey(s);
            drawFormulas();
            break;
            
        case STATE_PARAMS:
            handleParamsKey(s);
            drawParams();
            break;
            
        case STATE_CONSTS:
            handleConstsKey(s);
            drawConsts();
            break;
    }
    M5Cardputer.Display.endWrite();
}

void loop() {
    uint32_t now = millis();
    static uint32_t last_key_poll = 0;
    if (now - last_key_poll < 5) return;
    last_key_poll = now;

    if (screen_off_timeout > 0 && screen_is_on && (now - last_activity_time > (uint32_t)screen_off_timeout * 1000)) {
        M5Cardputer.Display.setBrightness(0);
        screen_is_on = false;
#ifdef ARDUINO
        neopixelWrite(21, 0, 0, 0);
#endif
    }

    M5.update();
    M5Cardputer.Keyboard.updateKeyList();
    M5Cardputer.Keyboard.updateKeysState();
    
    #if defined(ARDUINO)
    if (Serial.available()) {
        std::string line = Serial.readStringUntil('\n').c_str();
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        if (!line.empty()) {
            Serial.printf("[CDC IN] %s\n", line.c_str());
            if (line == "STATE") {
                const char* state_names[] = { "CALC", "HELP", "VARS", "SCRIPTS", "PLOT", "BINDS", "FORMULAS", "PARAMS", "CONSTS" };
                Serial.printf("[CDC STATE] %s (popup=%d)\n", state_names[appState], show_help_popup ? 1 : 0);
            } else if (line == "EXPR") {
                Serial.printf("[CDC EXPR] expr='%s' res='%s' err=%d\n", expression.c_str(), resultStr.c_str(), hasError ? 1 : 0);
            } else if (line.rfind("KEY:", 0) == 0) {
                std::string k = line.substr(4);
                Keyboard_Class::KeysState ks;
                if (k == "ENTER") { ks.enter = true; }
                else if (k == "ESC") { ks.esc = true; }
                else if (k == "UP") { ks.up = true; }
                else if (k == "DOWN") { ks.down = true; }
                else if (k == "LEFT") { ks.left = true; }
                else if (k == "RIGHT") { ks.right = true; }
                else if (k == "DEL" || k == "BACKSPACE") { ks.backspace = true; }
                else if (k == "FN+H") { ks.fn = true; ks.word.push_back('h'); }
                else if (k == "FN+V") { ks.fn = true; ks.word.push_back('v'); }
                else if (k == "FN+F") { ks.fn = true; ks.word.push_back('f'); }
                else if (k == "FN+S") { ks.fn = true; ks.word.push_back('s'); }
                else if (k == "CTRL+UP") { ks.ctrl = true; ks.up = true; }
                else if (k == "CTRL+DOWN") { ks.ctrl = true; ks.down = true; }
                else {
                    for (char c : k) ks.word.push_back(c);
                }
                
                // Route synthetic key state through standard key processing pipeline
                processKeyEvent(ks);
                const char* state_names[] = { "CALC", "HELP", "VARS", "SCRIPTS", "PLOT", "BINDS", "FORMULAS", "PARAMS", "CONSTS" };
                Serial.printf("[CDC KEY OK] state=%s popup=%d expr='%s'\n", state_names[appState], show_help_popup ? 1 : 0, expression.c_str());
            } else {
                bool in_edit = (appState != STATE_CALC);
                if (in_edit) {
                    Keyboard_Class::KeysState ks;
                    for (char c : line) ks.word.push_back(c);
                    processKeyEvent(ks);
                    const char* state_names[] = { "CALC", "HELP", "VARS", "SCRIPTS", "PLOT", "BINDS", "FORMULAS", "PARAMS", "CONSTS" };
                    Serial.printf("[CDC EDIT INPUT OK] state=%s popup=%d\n", state_names[appState], show_help_popup ? 1 : 0);
                } else {
                    // Send line into expression and simulate Enter key press
                    expression = line;
                    cursor_pos = expression.size();
                    Keyboard_Class::KeysState ks;
                    ks.enter = true;
                    processKeyEvent(ks);
                    const char* state_names[] = { "CALC", "HELP", "VARS", "SCRIPTS", "PLOT", "BINDS", "FORMULAS", "PARAMS", "CONSTS" };
                    Serial.printf("[CDC EVAL OK] state=%s popup=%d expr='%s' res='%s' err=%d\n", state_names[appState], show_help_popup ? 1 : 0, expression.c_str(), resultStr.c_str(), hasError ? 1 : 0);
                }
            }
        }
    }
    #endif

    // G0 button always returns to REPL (or clears screen/history if already in REPL)
    if (M5.BtnA.wasPressed()) {
        if (!screen_is_on) {
            M5Cardputer.Display.setBrightness(backlight_brightness);
            screen_is_on = true;
            last_activity_time = now;
            return;
        }
        last_activity_time = now;
        if (show_help_popup) {
            show_help_popup = false;
        }
        if (appState == STATE_CALC) {
            expression = "";
            cursor_pos = 0;
            resultStr = "";
            hasError = false;
            select_all_active = false;
            history_scroll_offset = 0;
            drawCalc();
        } else {
            resetAppState(STATE_CALC);
            drawCalc();
        }
        return;
    }
    
    if (M5Cardputer.Keyboard.isPressed()) {
        if (!screen_is_on) {
            M5Cardputer.Display.setBrightness(backlight_brightness);
            screen_is_on = true;
            last_activity_time = now;
            delay(150);
            M5Cardputer.Keyboard.updateKeyList();
            M5Cardputer.Keyboard.updateKeysState();
            return;
        }
        last_activity_time = now;
    }

    if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) return;
    Keyboard_Class::KeysState s = M5Cardputer.Keyboard.keysState();

    // Sticky Modifier Mode Logic
    static bool prev_raw_fn = false;
    static bool prev_raw_shift = false;
    static bool prev_raw_opt = false;
    static bool prev_raw_ctrl = false;
    static bool prev_raw_alt = false;

    if (sticky_mode) {
        if (s.fn && !prev_raw_fn) sticky_fn_active = !sticky_fn_active;
        if (s.shift && !prev_raw_shift) sticky_shift_active = !sticky_shift_active;
        if (s.opt && !prev_raw_opt) sticky_opt_active = !sticky_opt_active;
        if (s.ctrl && !prev_raw_ctrl) sticky_ctrl_active = !sticky_ctrl_active;
        if (s.alt && !prev_raw_alt) sticky_alt_active = !sticky_alt_active;

        prev_raw_fn = s.fn;
        prev_raw_shift = s.shift;
        prev_raw_opt = s.opt;
        prev_raw_ctrl = s.ctrl;
        prev_raw_alt = s.alt;

        s.fn = s.fn || sticky_fn_active;
        s.shift = s.shift || sticky_shift_active;
        s.opt = s.opt || sticky_opt_active;
        s.ctrl = s.ctrl || sticky_ctrl_active;
        s.alt = s.alt || sticky_alt_active;
    }

    // Scroll or dismiss Help Popup overlay
    if (show_help_popup) {
        bool is_backtick = (std::find(s.word.begin(), s.word.end(), '`') != s.word.end()) || 
                           (std::find(s.word.begin(), s.word.end(), '~') != s.word.end());
        if (s.esc || is_backtick) {
            show_help_popup = false;
            help_popup_scroll_offset = 0;
            switch (appState) {
                case STATE_CALC: drawCalc(); break;
                case STATE_HELP: drawHelpView(); break;
                case STATE_VARS: drawVars(); break;
                case STATE_SCRIPTS: drawScripts(); break;
                case STATE_PLOT: drawPlot(); break;
                case STATE_BINDS: drawBinds(); break;
                case STATE_FORMULAS: drawFormulas(); break;
                case STATE_PARAMS: drawParams(); break;
                case STATE_CONSTS: drawConsts(); break;
            }
            return;
        }
        
        bool page_up = s.up || (std::find(s.word.begin(), s.word.end(), ';') != s.word.end());
        bool page_down = s.down || (std::find(s.word.begin(), s.word.end(), '.') != s.word.end());
        
        if (page_up) {
            if (help_popup_scroll_offset > 0) help_popup_scroll_offset--;
            drawHelpPopup();
            return;
        }
        if (page_down) {
            help_popup_scroll_offset++;
            drawHelpPopup();
            return;
        }
        
        drawHelpPopup();
        return;
    }

    if (s.fn && s.word.empty()) {
        const auto& list = M5Cardputer.Keyboard.keyList();
        for (const auto& pos : list) {
            char val_first = _key_value_map[pos.y][pos.x].value_first;
            char val_third = _key_value_map[pos.y][pos.x].value_third;
            if (val_third == KEY_NONE && val_first != KEY_FN) {
                s.word.push_back(val_first);
            }
        }
    }

    processKeyEvent(s);
    
    // Reset sticky modifier states after consuming action key
    if (sticky_mode && !s.word.empty() && !prev_raw_fn && !prev_raw_shift && !prev_raw_opt && !prev_raw_ctrl && !prev_raw_alt) {
        sticky_fn_active = false;
        sticky_shift_active = false;
        sticky_opt_active = false;
        sticky_ctrl_active = false;
        sticky_alt_active = false;
    }
    
    static int last_led_state = -1;
    bool current_led_state = (hasError);
    if ((int)current_led_state != last_led_state) {
        #ifdef ARDUINO
        if (current_led_state) {
            neopixelWrite(21, 90, 0, 0); // Red at max safe brightness (90)
        } else {
            neopixelWrite(21, 0, 0, 0);   // LED off (power rail stays HIGH)
        }
        #endif
        last_led_state = (int)current_led_state;
    }
    
    delay(5);
}
