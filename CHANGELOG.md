# Changelog

All notable changes to this project will be documented in this file.  The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),  and this project adheres to [Calendar Versioning](https://calver.org/) (CalVer) with the format `YY.MM.DD`.

## [26.10.28] - 2026-10-28

### Added
- **Variadic Aggregation Functions**:
  - Added variadic `sum(a1, a2, ...)`, `prod(a1, a2, ...)`, `min(a1, a2, ...)`, `max(a1, a2, ...)`, `gcd(a1, a2, ...)`, `lcm(a1, a2, ...)` supporting up to 24 arguments.
  - Added array aggregate support for `prod(arr)` and `mode(arr)`.
- **Inline Loops & Conditionals**:
  - Parametric inline loops: `sum(i, 1, 10, i^2)`, `prod(i, 1, 5, i)`, `min(i, 1, 5, expr)`, `max(i, 1, 5, expr)`.
  - Inline conditionals: `if(cond, true_val, false_val)` and multi-branch `iff(c1, v1, c2, v2, ..., def)`.
  - Inline range indexing: `(1:2:10)[2]`.
- **Temperature Unit Aliases in Units Adapter**:
  - Added aliases for temperature conversions: `tempC`, `tempF`, `tempK`, `tempR`, `degR`, `celsius`, `fahrenheit`, `rankine`.
- **REPL Array Formatting**:
  - Direct display of array/vector evaluation results in REPL output history (`[1, 2, 3]`).

### Fixed
- **Domain & Overflow Bounds**:
  - Fixed `custom_logb` parameter order to `logb(x, base)` matching documentation.
  - Enforced integer validation on `fact(n)` and `fib(n)` and added overflow limits ($n \le 170$ for `fact`, $n \le 1476$ for `fib`).
  - Rewrote `C(n, k)` and `P(n, k)` using multiplicative $O(k)$ evaluation without intermediate factorial overflows.
  - Differentiated between `Division by Zero` and `Overflow Error`.
- **Units Conversion Priority**:
  - Fixed unreachable code in dB conversion for `times_amp` and `times_v` ($20 \log_{10}$), adding positive ratio validation ($> 0$).
- **Script Engine Variable Scoping**:
  - Removed premature wiping of global variables in `runScript()`.
  - Evaluated function arguments before binding parameters in custom script functions and restored caller's `user_args` scope upon return.
- **Autocompletion**:
  - Fixed typo in autocompletion list (`arccosech` instead of duplicate `arccosec`).
  - Added `Cnk(`, `sum(`, `prod(`, `min(`, `max(`, `if(`, `iff(` to `autocomplete_words`.

## [26.10.25] - 2026-10-25

### Added
- **Full Trigonometric & Hyperbolic Function Suite**:
  - Direct trigonometric functions: `sin`,  `cos`,  `tan` (`tg`),  `cot` (`ctg`,  `ctan`),  `sec`,  `csc` (`cosec`).
  - Inverse trigonometric functions: `asin` (`arcsin`),  `acos` (`arccos`),  `atan` (`arctan`,  `atg`,  `arctg`),  `acot` (`arccot`,  `actg`,  `arcctg`,  `actan`,  `arcctan`),  `asec` (`arcsec`),  `acsc` (`arccsc`,  `acosec`,  `arccosec`).
  - Hyperbolic functions: `sinh` (`sh`),  `cosh` (`ch`),  `tanh` (`th`),  `coth` (`cth`,  `ctgh`,  `ctanh`),  `sech` (`sch`),  `csch` (`cosech`).
  - Inverse hyperbolic functions: `asinh` (`ahsin`,  `arcsinh`,  `arsinh`,  `arsh`),  `acosh` (`ahcos`,  `arccosh`,  `arcosh`,  `arch`),  `atanh` (`ahtan`,  `arctanh`,  `artanh`,  `arth`,  `artg`,  `arctgh`),  `acoth` (`ahcot`,  `arccoth`,  `arcoth`,  `arcth`,  `ahcoth`),  `asech` (`ahsec`,  `arcsech`,  `arsech`,  `ahsech`),  `acsch` (`ahcsc`,  `arccsch`,  `arcsch`,  `acosech`,  `arccosech`,  `ahcsch`).
- **Comprehensive Domain and Singularity Error Messages**:
  - Replaced generic math errors with clear,  actionable messages specifying exact valid domains:
    - `asin(2)` $\rightarrow$ `Domain Error: asin(x) requires -1 <= x <= 1`
    - `asec(0.5)` $\rightarrow$ `Domain Error: asec(x) requires |x| >= 1`
    - `acosh(0.5)` $\rightarrow$ `Domain Error: acosh(x) requires x >= 1`
    - `atanh(1.5)` $\rightarrow$ `Domain Error: atanh(x) requires -1 < x < 1`
    - `acoth(0.5)` $\rightarrow$ `Domain Error: acoth(x) requires |x| > 1`
    - `asech(2)` $\rightarrow$ `Domain Error: asech(x) requires 0 < x <= 1`
    - `coth(0)` $\rightarrow$ `Domain Error: coth(x) requires x != 0`
    - `sec(90)` in Deg mode $\rightarrow$ `Domain Error: sec(x) undefined when cos(x) = 0`
    - `tan(90)` in Deg mode $\rightarrow$ `Domain Error: tan(x) undefined when cos(x) = 0`
    - `cot(0)` in Deg mode $\rightarrow$ `Domain Error: cot(x) undefined when sin(x) = 0`
    - `csc(0)` in Deg mode $\rightarrow$ `Domain Error: csc(x) undefined when sin(x) = 0`
- **Angle and Physical Unit Tagging**:
  - Output results for angular functions now display unit tags: `asin(1)` $\rightarrow$ `90 (deg)` (or `1.570796 (rad)`).
  - Variable assignments preserve unit tags: `x = conv(22, "mm", "cm")` $\rightarrow$ `x = 2.2 (cm)`.
- **Acceleration Conversions**:
  - Added support for acceleration units in GNU Units engine (`m/s^2`,  `cm/s^2`,  `g0`,  `ft/s^2`).
- **Long-Press History Clear**:
  - Long-pressing the clear button / G0 resets all calculation history (`e1`,  `e2`...),  REPL input history,  and user variables while preserving custom user formulas and constants.
- **Autocompletion Performance**:
  - Cached TAB completions for fast cycling without redundant re-evaluation.
