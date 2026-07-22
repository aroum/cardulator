#ifndef HELP_ENGINE_H
#define HELP_ENGINE_H

#include <string>
#include <vector>
#include "app_state.h"

// Parse help requests like help(), help(plot), help(sin), etc.
bool preprocessHelp(const std::string& input_expr, std::string& help_out);

// Returns title and lines for Fn+H modal popups based on state
void getHelpPopupData(AppState state, bool script_edit_mode, std::string& out_title, std::vector<std::string>& out_lines);

// Returns title and two columns (left controls, right syntax) for full-screen Help View
void getHelpViewData(std::string& out_title, std::vector<std::string>& out_left_col, std::vector<std::string>& out_right_col);

#endif // HELP_ENGINE_H
