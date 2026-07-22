#pragma once

#include "app_state.h"
#include "math_funcs.h"
#include "help_engine.h"

bool preprocessHelp(const std::string& s, std::string& help_out);
double evaluate(const std::string& expr_str, std::string& err);
double evaluateInput(const std::string& input, std::string& err, bool& isDefinition);
std::vector<double> evaluateArrayBinaryOp(const std::string& op1_str, const std::string& op_str, const std::string& op2_str, bool& is_scalar_result, double& scalar_val, std::string& err);
std::vector<double> parseArrayExpr(const std::string& rhs, bool& is_array, std::string& err);
std::string preprocessArrayLookups(const std::string& expr, std::string& err);
std::string formatPrintString(const std::string& str, std::string& err);
bool parseArrayElementAssignment(const std::string& line, std::string& name, std::string& idx_expr, std::string& val_expr);
bool parseBinaryOp(const std::string& rhs, std::string& op1, std::string& op, std::string& op2);
void handleTabCompletion(std::string& expression, int& cursor_pos);

#include "plot_engine.h"
#include "script_engine.h"
