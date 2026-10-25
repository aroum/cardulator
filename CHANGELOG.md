# Changelog

All notable changes to this project will be documented in this file.  The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),  and this project adheres to [Calendar Versioning](https://calver.org/) (CalVer) with the format `YY.MM.DD`.

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
