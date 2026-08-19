# Cardulator — Scientific REPL Calculator & Scripting Engine for M5Stack Cardputer

[Russian / Русский](README_RU.md)

![logo](logo.png)
![photo](photo.png)

---

![Build Status](https://github.com/aroum/cardulator/actions/workflows/build.yml/badge.svg)
![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32--S3-orange.svg)
![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)

**Cardulator** is a powerful, feature-rich scientific mathematical calculator and REPL scripting environment designed specifically for the **M5Stack Cardputer ADV** (ESP32-S3). Built on top of the robust [`TinyExpr-PlusPlus`](https://github.com/Blake-Madden/tinyexpr-plusplus) engine, Cardulator provides real-time syntax highlighting, rainbow bracket matching, SI prefix parsing, scientific notation, user variables and functions, multi-variable formula wizards, a C-style scripting engine with array support, matrix/vector operations, customizable hotkey binds, and interactive 2D function plotting.

---

## Key Features

- **High-Performance REPL**: Interactive evaluation loop with `Tab` autocompletion, 1-based answer history (`e1`, `e2`...), SI prefix parsing (`1k` = `1000`, `2M2` = `2,200,000`), scientific notation (`5e10`), and multi-statement lines using `;`.
- **Dynamic Syntax Highlighting**: Real-time rainbow bracket depth matching (`( )`), yellow numbers, cyan variables, magenta constants, and red syntax/error highlighting.
- **2D Plotting Engine (`STATE_PLOT` / `Fn + G`)**: Interactive Matplotlib-like 2D function and vector plotter (`plot(y)`, `plot(x, y, color, linestyle)`) with panning (WASD / Arrows), 5x turbo zoom (`Ctrl + Zoom`), auto-scaling, and `plot.hold()`.
- **Formula Library & Interactive Wizard (`Fn + F`)**: Multi-argument formula manager (up to 4 parameters) using standard `f(x,y)=expression` syntax (e.g. `hypot(x,y)=sqrt(x^2+y^2)`). In the formula selection menu, arrow keys work directly without `Fn` (as well as `Tab`/`Fn`+arrows). Inside formula editor fields, arrow navigation requires **`Fn` + Arrows** (`Fn+;` / `Fn+.`).
- **Scripting Engine (`Fn + S`)**: C-style script runner supporting `if/elif/else`, `while`, `for`, `sleep()`, 1D arrays/vectors, element-wise math (`.*`, `./`), dot products, and formatted text printing (`print("x={x}")`).
- **Customizable Hotkey Binds (`Fn + B`)**: Bind expressions or template shortcuts to `Alt + [Key]` for quick one-touch execution in REPL.
- **NVS Storage & Persistence**: Automatic background saving and loading of user variables, functions, scripts, keybindings, and system parameters across reboots.
- **100% Host Unit Testing**: Built-in native test suite running 20 comprehensive unit tests on host OS (macOS/Linux) via PlatformIO `native`.

---

## Controls & Keybindings

### 1. Navigation & Cursor Control (All Text Fields & Formulas)

Physical keys `,` `/` `;` `.` type their literal characters by default. Navigation in all input fields and formula lists is controlled strictly via the **`Fn` key** or **`Tab`**:

- **`Fn + ,`** $\rightarrow$ **Left** (Move cursor 1 character left)
- **`Fn + /`** $\rightarrow$ **Right** (Move cursor 1 character right)
- **`Fn + ;`** $\rightarrow$ **Up** (Navigate calculation history in REPL, formula list, or line/list navigation)
- **`Fn + .`** $\rightarrow$ **Down** (Navigate calculation history in REPL, formula list, or line/list navigation; or **`Tab`**)
- **`Ctrl + Left`** (`Ctrl + Fn + ,`) / **`Ctrl + Right`** (`Ctrl + Fn + /`) $\rightarrow$ Word-by-word cursor jump left/right
- **`Ctrl + Up`** / **`Ctrl + Down`** (`Ctrl + Fn + ;` / `Ctrl + Fn + .`) $\rightarrow$ Scroll REPL viewport up/down
- **`Fn + L`** $\rightarrow$ **Home** (Move cursor to beginning of line; `Ctrl + Fn + L` jumps to top of document)
- **`Fn + '`** $\rightarrow$ **End** (Move cursor to end of line; `Ctrl + Fn + '` jumps to bottom of document)
- **`Esc`** (or ``` ` ``` / `~` without Fn) $\rightarrow$ Return to the previous menu/screen (or REPL).
- **Physical G0 Button (Side Button)**: Returns to REPL (`STATE_CALC`) from any screen. If already in REPL, pressing **G0** clears current input, error status, and resets history.

### 2. Global Shortcuts & Menu Keys

| Shortcut     | Action                                                                                                                                    |
| :----------- | :---------------------------------------------------------------------------------------------------------------------------------------- |
| **`Fn + V`** | Open Variables Manager (`STATE_VARS`). View, add (`N`), edit (`E`/`Enter`), rename (`R`), delete (`Del`).                                 |
| **`Fn + S`** | Open Script Manager (`STATE_SCRIPTS`). Create, edit, run, and view script output.                                                         |
| **`Fn + G`** | Open 2D Plot Viewer (`STATE_PLOT`). Interactive plot grid and curves.                                                                     |
| **`Fn + B`** | Open Hotkey Binds Manager (`STATE_BINDS`). Assign custom macro strings to `Alt + Key`.                                                    |
| **`Fn + F`** | Open Formulas Library (`STATE_FORMULAS`). Interactive parameter wizards and custom formula creation.                                    |
| **`Fn + C`** | Open Constants Manager (`STATE_CONSTS`). View and edit built-in and user mathematical constants.                                          |
| **`Fn + P`** | Open System Parameters (`STATE_PARAMS`). Configure screen timeout, brightness, thousands separator, auto brackets, and sticky mod keys. |
| **`Fn + H`** | Open Help Overlay Popup. Displays contextual keybindings and available functions for the active screen.                                   |

---

## Mathematical Engine & Syntax

### 1. Arithmetic & Power Operators

- Addition `+`, Subtraction `-`, Multiplication `*`, Division `/`, Modulo `%` (or `mod(a, b)`).
- Exponentiation `^` (e.g., `2^10` $\rightarrow$ `1024`). Supports fractional exponents (`8^(1/3)` $\rightarrow$ `2`).
- Negation `-x` and unary plus `+x`.

### 2. Trigonometry (Degree & Radian Mode)

Default angular unit is **Degrees**.

- `sin(x)`, `cos(x)`, `tan(x)`, `ctan(x)` — Standard trigonometric functions (takes degrees).
- `asin(x)`, `acos(x)`, `atan(x)` — Inverse trigonometric functions (returns degrees).
- Radians conversions: `deg2rad(x)` (or `d2r(x)`), `rad2deg(x)` (or `r2d(x)`).

### 3. Logarithms & Exponential Functions

- `ln(x)` — Natural logarithm (base $e$).
- `log(x)` (or `log10(x)`) — Common logarithm (base 10).
- `log2(x)` — Binary logarithm (base 2).
- `logb(x, base)` — Logarithm with arbitrary base.
- `exp(x)` — Exponential function $e^x$.

### 4. Rounding & Absolute Value

- `abs(x)` — Absolute value / magnitude.
- `sqrt(x)` / `cbrt(x)` — Square root / Cube root.
- `floor(x)`, `ceil(x)`, `round(x)`, `trunc(x)`.
- `sgn(x)` — Signum function (-1 for $x < 0$, 0 for $x = 0$, 1 for $x > 0$).

### 5. Statistics & Vector Operations

- `mean(a1, a2, ...)` — Arithmetic mean of sample
- `median(a1, a2, ...)` — Median of sample
- `std(a1, a2, ...)` — Standard deviation
- `var(a1, a2, ...)` — Sample variance
- `min(a1, a2, ...)` / `max(a1, a2, ...)` — Minimum / maximum value

### 6. Combinatorics & Special Functions

- `C(n, k)` (or `Cnk(n, k)`) — Binomial coefficient (combinations $n$ choose $k$)
- `P(n, k)` — Permutations $n$ P $k$
- `fact(n)` (or `n!`) — Factorial
- `gcd(a, b, ...)` — Greatest common divisor
- `lcm(a, b, ...)` — Least common multiple
- `fib(n)` — $n$-th Fibonacci number

### 7. Built-in Constants

- `pi` — Mathematical constant $\pi \approx 3.141592653589793$
- `e` — Euler's number $e \approx 2.718281828459045$

#### Disambiguation of `e` Contexts

1. **Constant `e`**: Standalone lowercase token (e.g., `e`, `e^2`, `2*e`).
2. **Scientific Notation**: Numerical suffix exponent (e.g., `5e10` $= 5 \times 10^{10}$, `1e-5` $= 10^{-5}$).
3. **REPL History**: Sequential history reference (e.g., `e1`, `e2`, `e3`).

---

## Numerical Precision & Limitations

Cardulator performs all mathematical evaluations using **64-bit IEEE 754 Double-Precision Floating-Point** arithmetic (`double`):

- **Significant Precision**: 53-bit significand (~15–17 decimal digits of precision).
- **Exact Integers**: Exact integer representation up to $\pm 2^{53}$ ($\pm 9,007,199,254,740,992$).
- **Dynamic Range**: Numbers from $\approx \pm 2.225 \times 10^{-308}$ to $\approx \pm 1.797 \times 10^{308}$.
- **Special Values**: Full IEEE 754 support for `+inf` / `-inf` (overflow / division by zero) and `NaN` (undefined indeterminate forms).

---

## SI Prefixes

Cardulator supports standard SI prefixes directly within expressions (e.g., `1.5k + 200` $\rightarrow$ `1700`). SI prefixes can also replace the decimal separator in R-notation (e.g., `1k7` $\rightarrow$ `1700`).

### Multipliers ($\ge 1$)

| Power     | Prefix | Symbol | Example                |
| :-------- | :----- | :----- | :--------------------- |
| $10^1$    | deca   | `da`   | `1da` = `10`           |
| $10^2$    | hecto  | `h`    | `1h` = `100`           |
| $10^3$    | kilo   | `k`    | `1.5k` = `1500`        |
| $10^6$    | mega   | `M`    | `2M2` = `2,200,000`    |
| $10^9$    | giga   | `G`    | `1G` = `1,000,000,000` |
| $10^{12}$ | tera   | `T`    | `1T` = `10^{12}`       |
| $10^{15}$ | peta   | `P`    | `1P` = `10^{15}`       |
| $10^{18}$ | exa    | `E`    | `1E` = `10^{18}`       |
| $10^{21}$ | zetta  | `Z`    | `1Z` = `10^{21}`       |
| $10^{24}$ | yotta  | `Y`    | `1Y` = `10^{24}`       |

### Submultipliers ($< 1$)

| Power      | Prefix | Symbol | Example                |
| :--------- | :----- | :----- | :--------------------- |
| $10^{-1}$  | deci   | `d`    | `1d` = `0.1`           |
| $10^{-2}$  | centi  | `c`    | `1c` = `0.01`          |
| $10^{-3}$  | milli  | `m`    | `100m` = `0.1`         |
| $10^{-6}$  | micro  | `u`    | `10u` = `0.00001`      |
| $10^{-9}$  | nano   | `n`    | `1n5` = `0.0000000015` |
| $10^{-12}$ | pico   | `p`    | `1p` = `10^{-12}`      |
| $10^{-15}$ | femto  | `f`    | `1f` = `10^{-15}`      |
| $10^{-18}$ | atto   | `a`    | `1a` = `10^{-18}`      |
| $10^{-21}$ | zepto  | `z`    | `1z` = `10^{-21}`      |
| $10^{-24}$ | yocto  | `y`    | `1y` = `10^{-24}`      |

---

## Arrays, Ranges & Inline Conditions/Loops

### 1. Arrays & Ranges

- **Simple Range (step = 1)**: `start:end` (e.g., `1:10` $\rightarrow$ `[1, 2, 3, 4, 5, 6, 7, 8, 9, 10]`)
- **Range with Step**: `start:step:end` (e.g., `1:2:10` $\rightarrow$ `[1, 3, 5, 7, 9]`, `10:-1:1` $\rightarrow$ `[10, 9, ..., 1]`)
- **Indexing (1-based)**: `(1:2:10)[1]` $\rightarrow$ `1`

### 2. Inline Conditionals (If / Else)

- `if(condition, expr_true, expr_false)` (e.g., `if(5 > 3, 10, 20)` $\rightarrow$ `10`)
- Multi-branch condition: `iff(cond1, val1, cond2, val2, ..., default)` (e.g., `iff(x > 0, 1, x < 0, -1, 0)`)

### 3. Inline Mathematical Loops

- **Summation**: `sum(index, start, end, expr)` (e.g., `sum(i, 1, 10, i^2)` $\rightarrow$ $\sum_{i=1}^{10} i^2 = 385$)
- **Product**: `prod(index, start, end, expr)` (e.g., `prod(i, 1, 5, i)` $\rightarrow$ $5! = 120$)
- **Range Min/Max**: `min(index, start, end, expr)` / `max(index, start, end, expr)`

---

## Custom Variables & Functions

### 1. User Variables & Constants

- Variables: `temp = 25`, then `temp * 2` $\rightarrow$ `50`.
- Constants: `const a = 3`. Constants can be updated with explicit `const` (`const a = 2`), but assigning `a = 10` raises a `Const Error`.

### 2. User Functions

- **Single-line functions**: `f(x) = x^2` or `f(x, y) = 2*x + y`
- **Multi-line / Block functions (`fn` / `def` / `function`)**:

  ```c
  fn calculate(a, b) {
      c = a * 2;
      return c + b
  }
  ```

- **Multiple statements on one line**: Separate instructions with `;` (e.g., `x = 5; y = 10; x + y` $\rightarrow$ `15`).

---

## Scripting Engine (`Fn + S`)

The script manager allows creating, editing, and executing multi-line C-style programs with structural logic.

### 1. Syntax & Features

- **Conditionals**:

  ```c
  if (x > 10) {
      print("x is large")
  } else if (x > 5) { // or elif (x > 5)
      print("x is medium")
  } else {
      print("x is small")
  }
  ```

- **Loops**:
  - `while (cond) { ... }`
  - `for (i = 1; i <= 10; i++) { ... }`
- **Assignments & Shorthands**: `i++`, `i--`, `a += b`, `a -= b`.
- **Execution Control & Output**:
  - `sleep(ms)` — Pause execution for specified milliseconds.
  - `print("Text {expr}")` — Formatted printing with interpolated expressions inside curly braces.
- **1D Arrays in Scripts**:
  - Creation: `A = [1, 2, 3]` or `A = 1:10`.
  - 1-based Indexing: `A[1] = 99`.
  - Element-wise Math: `C = A .* B`, `C = A ./ B`, `C = A + B`, `C = A * 5`.
  - Dot Product: `dot_val = A * B`.
  - Length: `len(A)`.
- **Safety**: Execution is hard-capped at **1,000 steps** to prevent infinite loop CPU stalls (`Error: Loop limit reached`). Variables mutated during script execution remain isolated in temporary local scope and do not taint REPL global state.

---

## 2D Plotting Engine (`Fn + G`)

Visualize functions or datasets directly on Cardputer's 1.14" display.

### Commands & Controls

- `plot(x, y)` / `plot(y)` — Plot array/vector data.
- `plot(x, y, color, linestyle)` — Plot with custom color (`"r"`, `"g"`, `"b"`, `"c"`, `"m"`, `"y"`, `"k"`, `"w"`) and line style (`"-"`, `"--"`, `"-."`, `":"`, `""`).
- `plot.show()` / `plot.close()` — Open / close plot display.
- `plot.hold(1)` / `plot.hold(0)` — Retain overlay of multiple plots.
- `plot.xlim([min, max])` / `plot.ylim([min, max])` — Manual axis limit boundaries.
- **Interactive Controls**:
  - **Pan**: Arrow keys (`Fn + ; / , / . / /`) or `WASD`.
  - **Turbo Pan (5x)**: `Ctrl + Arrow` / `Ctrl + WASD`.
  - **Zoom**: `-` (Out) / `+` or `=` (In).
  - **Turbo Zoom (5x)**: `Ctrl + -` / `Ctrl + +`.
  - **Auto-Scale**: `A` key.

---

### 6. Units Conversion Engine (`conv()`)

Cardulator includes a comprehensive unit conversion engine powered by `gnu-units` alongside specialized electronics and physics converters:

- **General syntax**: `conv(val, "from_unit", ["to_unit"])` (e.g. `conv(100, "km/hr", "m/s")` $\rightarrow$ `27.777778 m/s`)
- **SI Base reduction**: `conv(5, "mile")` $\rightarrow$ `8046.72 m`
- **Assignment to variables**: `speed = conv(60, "mph", "km/hr")` or `[val, unit] = conv(100, "km/hr")`
- **Electronics & SMD Component Codes**:
  - SMD 3-digit & 4-digit Resistors: `conv("472", "smd4")` $\rightarrow$ `4720`, `conv(200, "smd3")` $\rightarrow$ `201`
  - EIA-96 Resistors: `conv(10000, "eia96")` $\rightarrow$ `01C`, `conv("01A", "ohm")` $\rightarrow$ `100 ohm`
  - RKM-notation: `conv("2R2", "ohm")` $\rightarrow$ `2.2 ohm`, `conv("R47", "ohm")` $\rightarrow$ `0.47 ohm`
  - Capacitors & Inductors: `conv("104", "uf")` $\rightarrow$ `0.1 uF`, `conv("4R7", "uh")` $\rightarrow$ `4.7 uH`
  - Wire cross-section: `conv(24, "AWG", "mm^2")` $\rightarrow$ `0.2047 mm^2`, `conv(0.205, "mm^2", "AWG")` $\rightarrow$ `24 AWG`
  - Standard Wire Gauges (with copper safety margin): `conv(2.1, "mm2", "std_mm2")` $\rightarrow$ `2.5 std_mm2`, `conv(2.5, "mm2", "std_awg")` $\rightarrow$ `13 std_awg`
  - Decibels & Power: `conv(30, "dBm", "mW")` $\rightarrow$ `1000 mW`, `conv(20, "dB", "times")` $\rightarrow$ `100 times`

---

## System Parameters (`Fn + P`)

Configure system settings stored in NVS:

- **Screen Timeout**: Auto-off delay in seconds (0 = always on). Any keypress wakes screen.
- **Backlight Brightness**: Display brightness level (0 to 255).
- **Format Mode**: Numeric output display format:
  - `NORM` (Auto default: clean standard/scientific display)
  - `FLT` (Pure float: strips exponent & trailing zeros)
  - `FIX` (Fixed point: strict decimal places with trailing zeros)
  - `SCI` (Scientific notation: e.g. `1.2345e-06`)
  - `ENG` (Engineering notation: exponent multiple of 3)
- **Precision (dp)**: Number of decimal places / significant digits (0 to 12).
- **Thousands Sep**: Toggle (`ON`/`OFF`) space grouping for thousands in results (e.g., `1 234 567.890 123`).
- **Auto Brackets**: Toggle (`ON`/`OFF`) automatic pairing for brackets `()`, `[]`, `{}` and quotes `'`, `"`. Auto-inserts `()` on `Tab` autocompletion.
- **Sticky Mod**: Toggle (`ON`/`OFF`) sticky modifier keys (`Fn`, `Shift`/`Aa`, `Opt`, `Ctrl`, `Alt`). Short tap (< 1s) latches modifier; long press (> 1s) releases on lift.
- **Block Cursor**: Toggle (`ON`/`OFF`) filled block cursor vs vertical bar.

---

## Display Specification & Error Handling

- **Display**: 1.14" TFT display (**240 × 135 pixels**).
- **Sticky Mode Status Bar**: Top 12px status bar displays active state indicator alongside color-coded modifier flags: `fn` (Red), `Aa` (Blue), `opt` (Cyan), `ctrl`/`alt` (Grey).
- **Error Handling & RGB LED**:
  Upon math or syntax error, the output text turns **bright red**, diagnostic error text is printed, and the onboard **RGB LED (`GPIO 21`)** glows bright red while holding LED power (`GPIO 38`) HIGH. Pressing any key automatically clears the error state and turns off the LED.

---

## Building & Flashing

### Prerequisites

- [PlatformIO CLI](https://platformio.org/) or PlatformIO IDE extension.

### Clone Repository

```bash
git clone --recursive https://github.com/aroum/cardulator.git
cd cardulator
```

### Run Host Native Tests

Execute native unit test suite on host OS without hardware:

```bash
pio test -e native
```

### Build & Flash to Cardputer

```bash
pio run -e cardputer -t upload
```

---

## License
 
This project is licensed under the [GNU General Public License v3.0 (GPLv3)](LICENSE).
 
- **GNU Units**: Licensed under the GNU General Public License v3.0 (GPLv3).
- **TinyExpr++**: Licensed under the zlib License.
