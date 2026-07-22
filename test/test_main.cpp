#include <unity.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>
#include <clocale>
#include <cmath>

// We need to include the engine components for test runner compilation
#include "../src/app_state.h"
#include "../src/help_engine.h"
#include "../src/math_funcs.h"
#include "../src/plot_engine.h"
#include "../src/script_engine.h"
#include "../src/math_engine.h"
#include "../src/help_engine.cpp"
#include "../src/math_engine.cpp"
#include "../src/plot_engine.cpp"
#include "../src/script_engine.cpp"

// Define global states for the test runner
std::vector<double> history;
std::vector<UserArg> user_args;
std::vector<UserConst> user_consts;
std::vector<std::string> user_funcs;
std::vector<CustomScriptFunc> user_script_funcs;
std::map<std::string, std::vector<double>> user_arrays;
std::vector<std::string> script_console_output;
bool use_thousands_sep = false;
std::vector<std::string> autocomplete_words = {
    "sin(", "cos(", "tan(", "ctan(", "asin(", "acos(", "atan(",
    "sinh(", "cosh(", "tanh(", "asinh(", "acosh(", "atanh(",
    "sqrt(", "ln(", "log(", "log2(", "logb(", "exp(", "cbrt(",
    "abs(", "ceil(", "floor(", "round(", "trunc(", "sgn(", "mod(",
    "mean(", "median(", "var(", "std(", "rUni(", "rNor(",
    "fact(", "C(", "P(", "gcd(", "lcm(", "fib(", "len(", "print(", "help(", "help()",
    "pi", "e", "phi",
    "plot(", "plot.show()", "plot.close()", "plot.hold(", "plot.xlim(", "plot.ylim("
};
bool degreesMode = true;
bool auto_brackets = true;

void setUp(void) {
    // Reset global state before each test
    history.clear();
    user_args.clear();
    user_funcs.clear();
    user_script_funcs.clear();
    user_consts.clear();
    autocomplete_words = {
        "sin(", "cos(", "tan(", "ctan(", "asin(", "acos(", "atan(",
        "sinh(", "cosh(", "tanh(", "asinh(", "acosh(", "atanh(",
        "sqrt(", "ln(", "log(", "log2(", "logb(", "exp(", "cbrt(",
        "abs(", "ceil(", "floor(", "round(", "trunc(", "sgn(", "mod(",
        "mean(", "median(", "var(", "std(", "rUni(", "rNor(",
        "fact(", "C(", "P(", "gcd(", "lcm(", "fib(", "len(", "print(", "help(", "help()",
        "pi", "e", "phi",
        "plot(", "plot.show()", "plot.close()", "plot.hold(", "plot.xlim(", "plot.ylim("
    };
    degreesMode = true;
    auto_brackets = true;
}

void tearDown(void) {
}

void test_basic_evaluation(void) {
    std::string err;
    user_args.push_back({"z", 1.0});
    user_args.push_back({"i", 3.0});
    double r_neq = evaluate("z != i", err);
    printf("[NEQ TEST] evaluate('z != i') = %f, err='%s'\n", r_neq, err.c_str());
    user_args.clear();
    double res = evaluate("2 + 3 * 4", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(14.0, res);
    
    double res_mod = evaluate("5 % 2", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(1.0, res_mod);
    
    double res_fact = evaluate("fact(5)", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(120.0, res_fact);
    
    double res_fact_suffix = evaluate("5!", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(120.0, res_fact_suffix);
    
    double res_pct_sub = evaluate("10 - 15%", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(8.5, res_pct_sub);
    
    double res_pct_add = evaluate("100 + 5%", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(105.0, res_pct_add);
    
    double res_pct_single = evaluate("5%", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(0.05, res_pct_single);
    
    double res_pct_infix_mod = evaluate("10 - 15 % 3", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(10.0, res_pct_infix_mod);
}

void test_degrees_trig(void) {
    std::string err;
    double res_sin = evaluate("sin(30)", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(0.5, res_sin);

    double res_cos = evaluate("cos(60)", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(0.5, res_cos);

    double res_ctan = evaluate("ctan(45)", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(1.0, res_ctan);

    // Test deg2rad / d2r
    double res_d2r = evaluate("deg2rad(180)", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(M_PI, res_d2r);

    double res_d2r_short = evaluate("d2r(90)", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(M_PI / 2.0, res_d2r_short);

    // Test rad2deg / r2d
    double res_r2d = evaluate("rad2deg(pi)", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(180.0, res_r2d);

    double res_r2d_short = evaluate("r2d(pi / 4)", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(45.0, res_r2d_short);
}

void test_repl_history(void) {
    std::string err;
    bool isDef = false;
    
    // Simulate first eval (2+3) -> e1 = 5
    double r1 = evaluateInput("2+3", err, isDef);
    TEST_ASSERT_FALSE(isDef);
    history.push_back(r1); // e1 = 5
    
    // Simulate second eval (e1 * 10) -> e2 = 50
    double r2 = evaluateInput("e1 * 10", err, isDef);
    TEST_ASSERT_FALSE(isDef);
    TEST_ASSERT_EQUAL_FLOAT(50.0, r2);
}

void test_scientific_notation_and_si_prefixes(void) {
    std::string err;
    double res_exp = evaluate("5e3 + 2.5e2", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(5250.0, res_exp);

    double res_si = evaluate("1.5k + 200", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(1700.0, res_si);

    double res_small = evaluate("100m + 10u", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(0.10001, res_small);

    // TODO item #33: 2k26 parsing as 2026, and all SI prefixes
    double res_2k26 = evaluate("2k26", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(2026.0, res_2k26);

    double res_1k5 = evaluate("1k5", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(1500.0, res_1k5);

    double res_4M7 = evaluate("4M7", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(4700000.0, res_4M7);

    double res_1n5 = evaluate("1n5", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(1.5e-9, res_1n5);
}

void test_formulas(void) {
    std::string err;
    // Formula hypot(x, y) = sqrt(x^2 + y^2)
    user_args.push_back({"x", 3.0});
    user_args.push_back({"y", 4.0});
    double res_hypot = evaluate("sqrt(x^2 + y^2)", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(5.0, res_hypot);

    // Formula area(r) = pi * r^2
    user_args.clear();
    user_args.push_back({"r", 5.0});
    double res_area = evaluate("pi * r^2", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(M_PI * 25.0, res_area);
    user_args.clear();
}

void test_user_defined_variables(void) {
    std::string err;
    bool isDef = false;
    
    // Define a variable temp = 25
    double res_def = evaluateInput("temp = 25", err, isDef);
    TEST_ASSERT_TRUE(isDef);
    TEST_ASSERT_EQUAL_FLOAT(25.0, res_def);
    
    // Use it
    double res_use = evaluateInput("temp * 2", err, isDef);
    TEST_ASSERT_FALSE(isDef);
    TEST_ASSERT_EQUAL_FLOAT(50.0, res_use);
}

void test_user_defined_functions(void) {
    std::string err;
    bool isDef = false;
    
    // Define f(x) = x^2 + 10
    evaluateInput("f(x) = x^2 + 10", err, isDef);
    TEST_ASSERT_TRUE(isDef);
    
    // Use it f(5)
    double res_use = evaluateInput("f(5)", err, isDef);
    TEST_ASSERT_FALSE(isDef);
    TEST_ASSERT_EQUAL_FLOAT(35.0, res_use);
}

void test_autocompletion(void) {
    // 1st TAB on "si" -> sin()
    std::string expr = "si";
    int cur_pos = expr.size();
    handleTabCompletion(expr, cur_pos);
    TEST_ASSERT_EQUAL_STRING("sin()", expr.c_str());
    TEST_ASSERT_EQUAL_INT(4, cur_pos);
    
    // 2nd TAB on same expression -> sinh()
    handleTabCompletion(expr, cur_pos);
    TEST_ASSERT_EQUAL_STRING("sinh()", expr.c_str());
    TEST_ASSERT_EQUAL_INT(5, cur_pos);

    // 3rd TAB -> cycles back to sin()
    handleTabCompletion(expr, cur_pos);
    TEST_ASSERT_EQUAL_STRING("sin()", expr.c_str());
    TEST_ASSERT_EQUAL_INT(4, cur_pos);
    
    // Autocomplete "fa" -> matches fact()
    std::string expr_fact = "fa";
    int cur_pos_fact = expr_fact.size();
    handleTabCompletion(expr_fact, cur_pos_fact);
    TEST_ASSERT_EQUAL_STRING("fact()", expr_fact.c_str());
    TEST_ASSERT_EQUAL_INT(5, cur_pos_fact);
    
    // Autocomplete "pr" -> matches print()
    std::string expr_pr = "pr";
    int cur_pos_pr = expr_pr.size();
    handleTabCompletion(expr_pr, cur_pos_pr);
    TEST_ASSERT_EQUAL_STRING("print()", expr_pr.c_str());
    TEST_ASSERT_EQUAL_INT(6, cur_pos_pr);
}

void test_clear_all(void) {
    std::string err;
    bool isDef = false;
    evaluateInput("a = 5", err, isDef);
    history.push_back(5.0);
    
    TEST_ASSERT_EQUAL_INT(1, history.size());
    TEST_ASSERT_EQUAL_INT(1, user_args.size());
    
    // Clear
    history.clear();
    user_args.clear();
    user_funcs.clear();
    
    TEST_ASSERT_EQUAL_INT(0, history.size());
    TEST_ASSERT_EQUAL_INT(0, user_args.size());
}

void test_si_prefixes_as_variables(void) {
    std::string err;
    bool isDef = false;
    
    // Define a variable named k
    double r1 = evaluateInput("k = 5", err, isDef);
    TEST_ASSERT_TRUE(isDef);
    TEST_ASSERT_EQUAL_FLOAT(5.0, r1);
    
    // Evaluate expression "k + 2" -> should be 7, NOT 1002!
    double r2 = evaluateInput("k + 2", err, isDef);
    TEST_ASSERT_FALSE(isDef);
    TEST_ASSERT_EQUAL_FLOAT(7.0, r2);
    
    // Evaluate expression "5k + k" -> should be 5000 + 5 = 5005
    double r3 = evaluateInput("5k + k", err, isDef);
    TEST_ASSERT_FALSE(isDef);
    TEST_ASSERT_EQUAL_FLOAT(5005.0, r3);
}

void test_const_definition(void) {
    std::string err;
    bool isDef = false;
    
    // const a = 3
    double c1 = evaluateInput("const a = 3", err, isDef);
    TEST_ASSERT_TRUE(isDef);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(3.0, c1);
    
    // const a = 2 (updating constant)
    double c2 = evaluateInput("const a = 2", err, isDef);
    TEST_ASSERT_TRUE(isDef);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(2.0, c2);
    
    // a = 10 (reassignment without const -> Const Error)
    evaluateInput("a = 10", err, isDef);
    TEST_ASSERT_EQUAL_STRING("Const Error", err.c_str());
}

void test_multiline_functions_and_semicolon() {
    std::string err;
    bool isDef = false;
    
    user_script_funcs.push_back({ "my_add", {"x", "y"}, {"z = x + y", "return z * 2"} });
    
    double res = evaluateInput("my_add(3, 4)", err, isDef);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(14.0, res);
}

void test_array_operations() {
    std::string err;
    bool is_arr = false;
    user_arrays["A"] = {1, 2, 3};
    user_arrays["B"] = {4, 5, 6};
    
    bool is_scalar = false;
    double scalar_res = 0;
    std::vector<double> res = evaluateArrayBinaryOp("A", "+", "B", is_scalar, scalar_res, err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_INT(3, res.size());
    TEST_ASSERT_EQUAL_FLOAT(5.0, res[0]);
    TEST_ASSERT_EQUAL_FLOAT(7.0, res[1]);
    TEST_ASSERT_EQUAL_FLOAT(9.0, res[2]);

    // Test vector stats functions
    double res_mean = evaluate("mean([1, 2, 3, 4, 5])", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(3.0, res_mean);

    double res_median = evaluate("median([1, 2, 5, 4, 3])", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(3.0, res_median);

    double res_mode = evaluate("mode([1, 2, 2, 3, 2])", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(2.0, res_mode);

    double res_var = evaluate("var([1, 2, 3, 4, 5])", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(2.5, res_var);

    double res_std = evaluate("std([1, 2, 3, 4, 5])", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(std::sqrt(2.5), res_std);

    double res_dot = evaluate("dot([1, 2, 3], [4, 5, 6])", err);
    TEST_ASSERT_TRUE_MESSAGE(err.empty(), err.c_str());
    TEST_ASSERT_EQUAL_FLOAT(32.0, res_dot);
}

static std::string readFileToString(const std::string& path);

void test_script_execution() {
    // 1. fib script
    std::string fib_code = "x = 1\ny = 1\nfor (i = 1; i <= 10; i++) {\n  print(x)\n  tmp = x + y\n  x = y\n  y = tmp\n}\n";
    runScript(fib_code);
    TEST_ASSERT_TRUE(script_console_output.size() >= 10);
    TEST_ASSERT_EQUAL_STRING("1", script_console_output[0].c_str());
    TEST_ASSERT_EQUAL_STRING("1", script_console_output[1].c_str());
    TEST_ASSERT_EQUAL_STRING("2", script_console_output[2].c_str());
    TEST_ASSERT_EQUAL_STRING("3", script_console_output[3].c_str());

    // 2. plot.txt script
    std::string plot_txt = readFileToString("../scripts/plot.txt");
    if (plot_txt.empty()) plot_txt = readFileToString("../../scripts/plot.txt");
    if (plot_txt.empty()) plot_txt = readFileToString("firmware/scripts/plot.txt");
    if (plot_txt.empty()) plot_txt = readFileToString("firmware/cardulator/scripts/plot.txt");
    if (plot_txt.empty()) plot_txt = readFileToString("scripts/plot.txt");
    TEST_ASSERT_FALSE_MESSAGE(plot_txt.empty(), "Failed to load firmware/scripts/plot.txt");
    runScript(plot_txt);
    TEST_ASSERT_TRUE_MESSAGE(script_console_output.size() >= 5, "plot.txt should output lines");

    // 3. life.txt script
    std::string full_life = readFileToString("../scripts/life.txt");
    if (full_life.empty()) full_life = readFileToString("../../scripts/life.txt");
    if (full_life.empty()) full_life = readFileToString("firmware/scripts/life.txt");
    if (full_life.empty()) full_life = readFileToString("firmware/cardulator/scripts/life.txt");
    if (full_life.empty()) full_life = readFileToString("scripts/life.txt");
    TEST_ASSERT_FALSE_MESSAGE(full_life.empty(), "Failed to load firmware/scripts/life.txt");
    runScript(full_life);
    TEST_ASSERT_EQUAL_INT_MESSAGE(10, script_console_output.size(), "life.txt failed to produce 10 lines");

    // 4. plot_demo.txt script
    std::string plot_demo_txt = readFileToString("../scripts/plot_demo.txt");
    if (plot_demo_txt.empty()) plot_demo_txt = readFileToString("../../scripts/plot_demo.txt");
    if (plot_demo_txt.empty()) plot_demo_txt = readFileToString("firmware/scripts/plot_demo.txt");
    if (plot_demo_txt.empty()) plot_demo_txt = readFileToString("firmware/cardulator/scripts/plot_demo.txt");
    if (plot_demo_txt.empty()) plot_demo_txt = readFileToString("scripts/plot_demo.txt");
    TEST_ASSERT_FALSE_MESSAGE(plot_demo_txt.empty(), "Failed to load firmware/scripts/plot_demo.txt");
    runScript(plot_demo_txt);
    TEST_ASSERT_TRUE_MESSAGE(script_console_output.size() >= 2, "plot_demo.txt should output lines");
}

void test_plots() {
    std::string err;
    double res = 0.0;
    plot_lines.clear();
    plot_hold = false;
    user_arrays.clear();
    
    // 1. Single plot command
    bool is_plot = handlePlotCommands("plot(sin(x))", err, res);
    if (!err.empty()) printf("PLOT ERR: %s\n", err.c_str());
    TEST_ASSERT_TRUE(is_plot);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_INT(1, plot_lines.size());
    TEST_ASSERT_TRUE(plot_lines[0].y.size() > 0);

    // 2. Plot hold on & additional plot
    handlePlotCommands("plot.hold(1)", err, res);
    TEST_ASSERT_TRUE(plot_hold);
    handlePlotCommands("plot(cos(x))", err, res);
    TEST_ASSERT_EQUAL_INT(2, plot_lines.size());

    // 3. Custom limits xlim / ylim
    handlePlotCommands("plot.xlim(-5, 5)", err, res);
    TEST_ASSERT_TRUE(plot_manual_limits);
    TEST_ASSERT_EQUAL_DOUBLE(-5.0, plot_xlim_min);
    TEST_ASSERT_EQUAL_DOUBLE(5.0, plot_xlim_max);

    // 4. Test multi-line plot demo script execution
    handlePlotCommands("plot.close()", err, res);
    std::string plot_demo_script = 
        "plot.hold(1)\n"
        "x = [-5, 5, 0.5]\n"
        "y1 = sin(x)\n"
        "plot(x, y1, \"r\", \"-\")\n"
        "y2 = cos(x)\n"
        "plot(x, y2, \"g\", \"--\")\n"
        "y3 = 0.1 * x\n"
        "plot(x, y3, \"c\", \"-.\")\n"
        "plot.show()\n";
    runScript(plot_demo_script);
    TEST_ASSERT_EQUAL_INT(3, plot_lines.size());
    TEST_ASSERT_TRUE(plot_hold);
    TEST_ASSERT_EQUAL_STRING("-", plot_lines[0].linestyle.c_str());
    TEST_ASSERT_EQUAL_STRING("--", plot_lines[1].linestyle.c_str());
    TEST_ASSERT_EQUAL_STRING("-.", plot_lines[2].linestyle.c_str());

    // 5. plot.close()
    handlePlotCommands("plot.close()", err, res);
    TEST_ASSERT_EQUAL_INT(0, plot_lines.size());
    TEST_ASSERT_FALSE(plot_hold);
}

void test_vectors_and_ranges() {
    std::string err;
    bool isDef = false;

    // 1. Literal vector assignment: q = [1,1,3,4,5]
    double r1 = evaluateInput("q = [1,1,3,4,5]", err, isDef);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_TRUE(isDef);
    TEST_ASSERT_EQUAL_INT(1, user_arrays.count("q"));
    TEST_ASSERT_EQUAL_INT(5, user_arrays["q"].size());
    TEST_ASSERT_EQUAL_DOUBLE(3.0, user_arrays["q"][2]);

    // 2. Range assignment: j = 1:2:10
    double r2 = evaluateInput("j = 1:2:10", err, isDef);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_TRUE(isDef);
    TEST_ASSERT_EQUAL_INT(1, user_arrays.count("j"));
    TEST_ASSERT_EQUAL_INT(5, user_arrays["j"].size()); // 1, 3, 5, 7, 9
    TEST_ASSERT_EQUAL_DOUBLE(1.0, user_arrays["j"][0]);
    TEST_ASSERT_EQUAL_DOUBLE(9.0, user_arrays["j"][4]);

    // 4. len(array) function test
    double rlen = evaluate("len(q)", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_DOUBLE(5.0, rlen);
}

void test_loops() {
    // 1. While loop test script
    std::string while_code = 
        "sum = 0\n"
        "i = 1\n"
        "while (i <= 5) {\n"
        "  sum += i\n"
        "  i++\n"
        "}\n"
        "return sum\n";
    runScript(while_code);
    TEST_ASSERT_TRUE_MESSAGE(script_has_returned, "While loop failed to return");
    TEST_ASSERT_EQUAL_DOUBLE(15.0, script_return_val);

    // 2. Nested For loop test script
    std::string for_code = 
        "acc = 0\n"
        "for (x = 1; x <= 3; x++) {\n"
        "  for (y = 1; y <= 3; y++) {\n"
        "    acc = acc + x * y\n"
        "  }\n"
        "}\n"
        "return acc\n";
    runScript(for_code);
    TEST_ASSERT_TRUE_MESSAGE(script_has_returned, "For loop failed to return");
    TEST_ASSERT_EQUAL_DOUBLE(36.0, script_return_val); // (1+2+3)*(1+2+3) = 6*6 = 36
}

void test_print_and_e_history(void) {
    std::string err;
    bool isDef = false;
    
    // 1. Evaluate 10 + 20 -> should push result 30.0 to history
    double r1 = evaluateInput("10 + 20", err, isDef);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(30.0, r1);
    history.push_back(r1);
    
    // 2. Evaluate print(30) -> should evaluate to 30.0
    double r_print = evaluate("print(30)", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(30.0, r_print);

    // 3. Evaluate print(e1) -> e1 should be 30.0, print(e1) should evaluate to 30.0
    double r_print_e1 = evaluate("print(e1)", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_FLOAT(30.0, r_print_e1);

    // 4. Test formatPrintString
    std::string fmt_str = formatPrintString("e1", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_STRING("30", fmt_str.c_str());

    // 5. Test formatPrintString with literal string "erwe"
    std::string fmt_str_lit = formatPrintString("\"erwe\"", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_STRING("erwe", fmt_str_lit.c_str());

    // 6. Test formatPrintString with string interpolation "val={e1}"
    std::string fmt_str_interp = formatPrintString("\"val={e1}\"", err);
    TEST_ASSERT_TRUE(err.empty());
    TEST_ASSERT_EQUAL_STRING("val=30", fmt_str_interp.c_str());
}

static std::string readFileToString(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) return "";
    std::stringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

void test_docs_scripts_files(void) {
    // 1. Test firmware/scripts/life.txt
    std::string life_code = readFileToString("../scripts/life.txt");
    if (life_code.empty()) life_code = readFileToString("../../scripts/life.txt");
    if (life_code.empty()) life_code = readFileToString("firmware/scripts/life.txt");
    if (life_code.empty()) life_code = readFileToString("firmware/cardulator/scripts/life.txt");
    if (life_code.empty()) life_code = readFileToString("scripts/life.txt");
    TEST_ASSERT_FALSE_MESSAGE(life_code.empty(), "Failed to load firmware/scripts/life.txt");
    runScript(life_code);
    TEST_ASSERT_EQUAL_INT_MESSAGE(10, script_console_output.size(), "life.txt should produce 10 lines");
    for (const auto& line : script_console_output) {
        TEST_ASSERT_EQUAL_INT(std::string::npos, line.find("Err"));
        TEST_ASSERT_EQUAL_INT(std::string::npos, line.find("Error"));
    }

    // 2. Test firmware/scripts/plot_demo.txt
    std::string plot_demo_code = readFileToString("../scripts/plot_demo.txt");
    if (plot_demo_code.empty()) plot_demo_code = readFileToString("../../scripts/plot_demo.txt");
    if (plot_demo_code.empty()) plot_demo_code = readFileToString("firmware/scripts/plot_demo.txt");
    if (plot_demo_code.empty()) plot_demo_code = readFileToString("firmware/cardulator/scripts/plot_demo.txt");
    if (plot_demo_code.empty()) plot_demo_code = readFileToString("scripts/plot_demo.txt");
    TEST_ASSERT_FALSE_MESSAGE(plot_demo_code.empty(), "Failed to load firmware/scripts/plot_demo.txt");
    runScript(plot_demo_code);
    TEST_ASSERT_TRUE_MESSAGE(script_console_output.size() >= 2, "plot_demo.txt should output lines");
    TEST_ASSERT_EQUAL_STRING("=== Generating Math Showcase ===", script_console_output.front().c_str());
    TEST_ASSERT_EQUAL_STRING("Showcase rendered successfully!", script_console_output.back().c_str());
    TEST_ASSERT_EQUAL_INT(3, plot_lines.size());

    // 3. Test firmware/scripts/plot.txt
    std::string plot_code = readFileToString("../scripts/plot.txt");
    if (plot_code.empty()) plot_code = readFileToString("../../scripts/plot.txt");
    if (plot_code.empty()) plot_code = readFileToString("firmware/scripts/plot.txt");
    if (plot_code.empty()) plot_code = readFileToString("firmware/cardulator/scripts/plot.txt");
    if (plot_code.empty()) plot_code = readFileToString("scripts/plot.txt");
    TEST_ASSERT_FALSE_MESSAGE(plot_code.empty(), "Failed to load firmware/scripts/plot.txt");
    runScript(plot_code);
    TEST_ASSERT_TRUE_MESSAGE(script_console_output.size() >= 5, "plot.txt should output lines");
    TEST_ASSERT_EQUAL_STRING("=== Starting Demo ===", script_console_output.front().c_str());
    TEST_ASSERT_EQUAL_STRING("=== Demo Complete ===", script_console_output.back().c_str());
    TEST_ASSERT_EQUAL_INT(2, plot_lines.size());
}

void test_help(void) {
    std::string h_main;
    bool is_help = preprocessHelp("help", h_main);
    TEST_ASSERT_TRUE(is_help);
    TEST_ASSERT_TRUE(h_main.find("Cardulator") != std::string::npos || h_main.find("sin") != std::string::npos);

    std::string h_print;
    is_help = preprocessHelp("help(print)", h_print);
    TEST_ASSERT_TRUE(is_help);
    TEST_ASSERT_TRUE(h_print.find("print") != std::string::npos);

    std::string h_plot;
    is_help = preprocessHelp("help(plot)", h_plot);
    TEST_ASSERT_TRUE(is_help);
    TEST_ASSERT_TRUE(h_plot.find("plot") != std::string::npos);
}

int main(int argc, char **argv) {
    std::setlocale(LC_ALL, "C");
    UNITY_BEGIN();
    RUN_TEST(test_basic_evaluation);
    RUN_TEST(test_degrees_trig);
    RUN_TEST(test_repl_history);
    RUN_TEST(test_scientific_notation_and_si_prefixes);
    RUN_TEST(test_formulas);
    RUN_TEST(test_user_defined_variables);
    RUN_TEST(test_user_defined_functions);
    RUN_TEST(test_autocompletion);
    RUN_TEST(test_clear_all);
    RUN_TEST(test_si_prefixes_as_variables);
    RUN_TEST(test_const_definition);
    RUN_TEST(test_multiline_functions_and_semicolon);
    RUN_TEST(test_array_operations);
    RUN_TEST(test_script_execution);
    RUN_TEST(test_plots);
    RUN_TEST(test_vectors_and_ranges);
    RUN_TEST(test_loops);
    RUN_TEST(test_print_and_e_history);
    RUN_TEST(test_docs_scripts_files);
    RUN_TEST(test_help);
    return UNITY_END();
}
