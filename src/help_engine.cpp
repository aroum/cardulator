#include "help_engine.h"

bool preprocessHelp(const std::string& s, std::string& help_out) {
    std::string trimmed = s;
    trimmed.erase(0, trimmed.find_first_not_of(" \t"));
    trimmed.erase(trimmed.find_last_not_of(" \t") + 1);
    if (trimmed == "help()" || trimmed == "help" || trimmed == "help( )") {
        help_out = "=== Cardulator Help ===\n"
                   "Trig: sin, cos, tan, ctan, asin, acos, atan\n"
                   "Math: sqrt, cbrt, exp, ln, log, log2, logb, abs, mod\n"
                   "Round: ceil, floor, round, trunc, sgn\n"
                   "Stats: mean, median, var, std, rUni, rNor\n"
                   "Combinatorics: fact, C, P, gcd, lcm, fib\n"
                   "System: len, print, plot, help\n"
                   "Tip: type help(name) e.g. help(plot)";
        return true;
    }
    if (trimmed.rfind("help(", 0) == 0 && trimmed.back() == ')') {
        std::string target = trimmed.substr(5, trimmed.size() - 6);
        target.erase(0, target.find_first_not_of(" \t\"'"));
        target.erase(target.find_last_not_of(" \t\"'") + 1);
        if (target == "plot") {
            help_out = "=== plot() Help ===\n"
                       "Usage: plot(y) or plot(x, y, [col], [style])\n"
                       "Colors (col):\n"
                       "  'r' (red), 'g' (green), 'b' (blue)\n"
                       "  'c' (cyan), 'm' (magenta), 'y' (yellow)\n"
                       "  'k' (black), 'w' (white)\n"
                       "Styles (style):\n"
                       "  '-'  (solid line)\n"
                       "  '--' (dashed line)\n"
                       "  '-.' (dash-dot line)\n"
                       "  ':'  (dotted line)\n"
                       "Commands:\n"
                       "  plot.hold(1) / plot.hold(0) : hold overlay\n"
                       "  plot.xlim(min, max) / plot.ylim(min, max)\n"
                       "  plot.show()  : switch to plot screen\n"
                       "  plot.close() : reset plots";
        }
        else if (target == "print") {
            help_out = "=== print() Help ===\n"
                       "print(expr) : evaluate & output expr\n"
                       "print(\"text {var}\") : formatted string with var";
        }
        else if (target == "len") {
            help_out = "=== len() Help ===\n"
                       "len(vector) : returns element count of vector/array";
        }
        else if (target == "sin" || target == "cos" || target == "tan" || target == "ctan") {
            help_out = target + "(x) : Trigonometric function\n"
                       "Respects Deg/Rad mode toggle";
        }
        else if (target == "asin" || target == "acos" || target == "atan") {
            help_out = target + "(x) : Inverse trigonometric function\n"
                       "Returns angle in current Deg/Rad mode";
        }
        else if (target == "deg2rad" || target == "d2r" || target == "rad2deg" || target == "r2d") {
            help_out = "deg2rad(deg) / d2r(deg) : Convert degrees to radians\n"
                       "rad2deg(rad) / r2d(rad) : Convert radians to degrees";
        }
        else if (target == "sqrt" || target == "cbrt") {
            help_out = target + "(x) : Square root (x>=0) / Cube root";
        }
        else if (target == "ln" || target == "log" || target == "log2" || target == "logb") {
            help_out = "ln(x)    : Natural log (base e)\n"
                       "log(x)   : Base-10 log\n"
                       "log2(x)  : Base-2 log\n"
                       "logb(x,b): Custom base-b log";
        }
        else if (target == "abs" || target == "sgn" || target == "trunc" || target == "floor" || target == "ceil" || target == "round") {
            help_out = "abs(x)  : Absolute value |x|\n"
                       "sgn(x)  : Sign of x (-1, 0, 1)\n"
                       "floor(x): Round down to integer\n"
                       "ceil(x) : Round up to integer\n"
                       "round(x): Round to nearest integer\n"
                       "trunc(x): Integer part towards zero";
        }
        else if (target == "mod") {
            help_out = "mod(x, y) : Floating-point remainder (x % y)";
        }
        else if (target == "mean" || target == "median" || target == "mode" || target == "var" || target == "std") {
            help_out = target + "(a, b, c, ...) or " + target + "(vector) : Statistical calculation\n"
                       "Calculates " + target + " of values or 1D array";
        }
        else if (target == "dot") {
            help_out = "dot(v1, v2) or dot(a, b) : Dot product of vectors/scalars";
        }
        else if (target == "fact" || target == "C" || target == "P" || target == "gcd" || target == "lcm" || target == "fib") {
            help_out = "fact(n) / n! : Factorial n!\n"
                       "C(n, k)      : Combinations nCr\n"
                       "P(n, k)      : Permutations nPr\n"
                       "gcd(a, b)    : Greatest Common Divisor\n"
                       "lcm(a, b)    : Least Common Multiple\n"
                       "fib(n)       : n-th Fibonacci number";
        }
        else if (target == "rUni" || target == "rNor") {
            help_out = "rUni(min, max): Random float in range\n"
                       "rNor(mean, std): Normal random generator";
        }
        else {
            help_out = "Help: " + target + "\nGeneral math function/variable";
        }
        return true;
    }
    return false;
}

void getHelpPopupData(AppState state, bool script_edit_mode, std::string& out_title, std::vector<std::string>& out_lines) {
    out_title = "HELP: ";
    switch (state) {
        case STATE_CALC:
            out_title += "REPL Calculator";
            out_lines = {
                "Enter: Compute expression",
                "Up/Down: Input history",
                "Ctrl+Up/Down: Scroll viewport",
                "G0 btn: Back / Reset REPL",
                "Tab: Autocomplete words",
                "Fn+V: Variables Manager",
                "Fn+S: Script Manager",
                "Fn+G: Plotting mode",
                "Fn+B: Hotkey binds",
                "Fn+F: Formulas library",
                "Fn+P: System Parameters",
                "Fn+C: Clear all memory"
            };
            break;
        case STATE_VARS:
            out_title += "Variables Manager";
            out_lines = {
                "Up/Down or ; . : Select var",
                "Enter: Edit variable value",
                "N: Create new variable",
                "Del/Backspace: Delete var",
                "Fn+C: Clear all variables",
                "Esc or [: Exit to REPL"
            };
            break;
        case STATE_BINDS:
            out_title += "Hotkey Binds";
            out_lines = {
                "Up/Down or ; . : Select bind",
                "N: Create hotkey bind",
                "Alt+Key: Execute bind in REPL",
                "Del/Backspace: Delete bind",
                "Esc or [: Exit to REPL"
            };
            break;
        case STATE_FORMULAS:
            out_title += "Formulas Library";
            out_lines = {
                "Up/Down or ; . : Select formula",
                "Enter: Run wizard (max 4 args)",
                "N: Create custom formula",
                "E: Edit formula string",
                "Del/Backspace: Delete formula",
                "Wizard: Up/Down nav, Esc/` exit",
                "Esc or [: Exit to REPL"
            };
            break;
        case STATE_SCRIPTS:
            if (script_edit_mode) {
                out_title += "Script Editor";
                out_lines = {
                    "Fn+Enter: Run script & output",
                    "Enter: Insert new line",
                    "Tab: Autocomplete words",
                    "Ctrl+Left/Right: Jump word",
                    "Ctrl+Home/End: Jump document",
                    "Ctrl+C/X/V: Copy/Cut/Paste",
                    "Esc: Exit (Save prompt modal)"
                };
            } else {
                out_title += "Script Manager";
                out_lines = {
                    "Up/Down or ; . : Select script",
                    "Enter: Run selected script",
                    "E: Edit script in editor",
                    "N: Create new script file",
                    "Del/Backspace: Delete script",
                    "Esc or [: Exit to REPL"
                };
            }
            break;
        case STATE_PLOT:
            out_title += "Plot Controls";
            out_lines = {
                "plot(y), plot(x,y,col,style)",
                ", / . : Zoom in / out",
                "Arrows / WASD: Pan view",
                "a / A: Auto-scale view",
                "plot.hold(1): Overlay mode",
                "plot.xlim([min, max])",
                "Esc or [: Exit to REPL"
            };
            break;
        case STATE_PARAMS:
            out_title += "System Parameters";
            out_lines = {
                "Up/Down or ; . : Select param",
                "Left/Right or Enter: Toggle/Edit",
                "Screen Timeout: Off time (s)",
                "Brightness: Backlight (0-255)",
                "Thousands Sep: 1 000 000",
                "Auto Brackets: Auto-close ()",
                "Sticky Mod: Fn/Aa/Opt latch",
                "Esc or [: Exit to REPL"
            };
            break;
        case STATE_CONSTS:
            out_title += "Constants Manager";
            out_lines = {
                "Up/Down or ; . : Select const",
                "const a = 3: Create/update",
                "Del/Backspace: Delete const",
                "Esc or [: Exit to REPL"
            };
            break;
        default:
            out_title += "Cardulator";
            out_lines = { "Fn+H: Context help popup", "Esc: Exit modal" };
            break;
    }
}

void getHelpViewData(std::string& out_title, std::vector<std::string>& out_left_col, std::vector<std::string>& out_right_col) {
    out_title = "Cardulator | HELP";
    out_left_col = {
        "--- Controls ---",
        "Up/Dn: History/Input",
        "Ctrl+Up/Dn:Scroll view",
        "G0 btn: Back / Clear",
        "fn+V:Vars fn+C:Consts",
        "fn+B:Binds fn+F:Forms",
        "fn+S:Script fn+G:Plot",
        "fn+P:Params fn+D:DegRad",
        "fn+H: Help Overlay"
    };
    out_right_col = {
        "--- Syntax ---",
        "Tab: Auto-complete",
        "help(): List funcs",
        "help(print)",
        "a=2*pi+1",
        "f(x)=x^2",
        "e1,e2: Prev results",
        "1k=1000, 1M=1e6",
        "print(\"x={x}\")"
    };
}
