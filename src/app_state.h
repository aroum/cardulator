#pragma once

#include <string>
#include <vector>
#include <map>

struct UserArg {
    std::string name;
    double val;
};

struct UserConst {
    std::string name;
    double val;
};

struct UserFunc {
    std::string name;
    std::vector<std::string> params;
    std::string body;
};

struct CustomScriptFunc {
    std::string name;
    std::vector<std::string> params;
    std::vector<std::string> statements;
};

struct PlotLine {
    std::vector<double> x;
    std::vector<double> y;
    uint16_t color;
    std::string linestyle;
};

#ifndef UNIT_TEST
enum AppState {
    STATE_CALC,
    STATE_HELP,
    STATE_VARS,
    STATE_SCRIPTS,
    STATE_PLOT,
    STATE_BINDS,
    STATE_FORMULAS,
    STATE_PARAMS,
    STATE_CONSTS
};
extern AppState appState;
extern std::vector<PlotLine> plot_lines;
extern bool plot_hold;
extern bool plot_manual_limits;
extern double plot_xlim_min;
extern double plot_xlim_max;
extern double plot_ylim_min;
extern double plot_ylim_max;
extern double plot_center_x;
extern double plot_center_y;
extern double plot_scale;
#else
enum AppState {
    STATE_CALC,
    STATE_HELP,
    STATE_VARS,
    STATE_SCRIPTS,
    STATE_PLOT,
    STATE_BINDS,
    STATE_FORMULAS,
    STATE_PARAMS,
    STATE_CONSTS
};
inline AppState appState = STATE_CALC;
inline std::vector<PlotLine> plot_lines;
inline bool plot_hold = false;
inline bool plot_manual_limits = false;
inline double plot_xlim_min = 0.0;
inline double plot_xlim_max = 0.0;
inline double plot_ylim_min = 0.0;
inline double plot_ylim_max = 0.0;
inline double plot_center_x = 0.0;
inline double plot_center_y = 0.0;
inline double plot_scale = 1.0;
#endif

extern std::vector<CustomScriptFunc> user_script_funcs;
extern std::vector<double> history;
extern std::vector<UserArg> user_args;
extern std::vector<UserConst> user_consts;
extern std::vector<std::string> user_funcs;
extern std::map<std::string, std::vector<double>> user_arrays;
extern std::vector<std::string> autocomplete_words;
extern std::vector<std::string> script_console_output;
extern bool degreesMode;
extern bool use_thousands_sep;
extern bool auto_brackets;
extern bool sticky_mode;
