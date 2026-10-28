# Trigonometry and Mathematical Error Reference

## 1. Overview

Cardulator and Droidulator provide a comprehensive suite of trigonometric,  inverse trigonometric,  hyperbolic,  and inverse hyperbolic functions.  All functions support standard international notations as well as European and Russian engineering aliases.  Calculations seamlessly integrate with Degrees/Radians modes,  1D array vectorization,  and strict mathematical domain verification.

---

## 2. Trigonometric Functions

Direct trigonometric functions accept input angles according to the active mode (`Degrees` or `Radians`).  When a function reaches a mathematical singularity (division by zero),  a descriptive domain error is returned.

| Function | Aliases | Mode Dependency | Domain Restrictions | Error Message |
| :--- | :--- | :--- | :--- | :--- |
| $\sin(x)$ | `sin` | Deg / Rad | All $x \in \mathbb{R}$ | None |
| $\cos(x)$ | `cos` | Deg / Rad | All $x \in \mathbb{R}$ | None |
| $\tan(x)$ | `tan`,  `tg` | Deg / Rad | $\cos(x) \ne 0$ | `Domain Error: tan(x) undefined when cos(x) = 0` |
| $\cot(x)$ | `cot`,  `ctg`,  `ctan` | Deg / Rad | $\sin(x) \ne 0$ | `Domain Error: cot(x) undefined when sin(x) = 0` |
| $\sec(x)$ | `sec` | Deg / Rad | $\cos(x) \ne 0$ | `Domain Error: sec(x) undefined when cos(x) = 0` |
| $\csc(x)$ | `csc`,  `cosec` | Deg / Rad | $\sin(x) \ne 0$ | `Domain Error: csc(x) undefined when sin(x) = 0` |

---

## 3. Inverse Trigonometric Functions

Inverse trigonometric functions return output angles in the currently selected angle unit (`deg` or `rad`).  Results are tagged with unit indicators,  such as `45 (deg)` or `0.785398 (rad)`.

| Function | Aliases | Output Unit | Valid Domain | Error Message |
| :--- | :--- | :--- | :--- | :--- |
| $\arcsin(x)$ | `asin`,  `arcsin` | Deg / Rad | $-1 \le x \le 1$ | `Domain Error: asin(x) requires -1 <= x <= 1` |
| $\arccos(x)$ | `acos`,  `arccos` | Deg / Rad | $-1 \le x \le 1$ | `Domain Error: acos(x) requires -1 <= x <= 1` |
| $\arctan(x)$ | `atan`,  `arctan`,  `atg`,  `arctg` | Deg / Rad | All $x \in \mathbb{R}$ | None |
| $\text{atan2}(y, x)$ | `atan2` | Deg / Rad | All $(x, y) \in \mathbb{R}^2$ | None |
| $\text{arccot}(x)$ | `acot`,  `arccot`,  `actg`,  `arcctg`,  `actan`,  `arcctan` | Deg / Rad | All $x \in \mathbb{R}$ | None |
| $\text{arcsec}(x)$ | `asec`,  `arcsec` | Deg / Rad | $|x| \ge 1$ | `Domain Error: asec(x) requires \|x\| >= 1` |
| $\text{arccsc}(x)$ | `acsc`,  `arccsc`,  `acosec`,  `arccosec` | Deg / Rad | $|x| \ge 1$ | `Domain Error: acsc(x) requires \|x\| >= 1` |

---

## 4. Hyperbolic Functions

Hyperbolic functions operate on real numbers and are independent of the Degrees/Radians mode setting.

| Function | Aliases | Domain Restrictions | Error Message |
| :--- | :--- | :--- | :--- |
| $\sinh(x)$ | `sinh`,  `sh` | All $x \in \mathbb{R}$ | None |
| $\cosh(x)$ | `cosh`,  `ch` | All $x \in \mathbb{R}$ | None |
| $\tanh(x)$ | `tanh`,  `th` | All $x \in \mathbb{R}$ | None |
| $\coth(x)$ | `coth`,  `cth`,  `ctgh`,  `ctanh` | $x \ne 0$ | `Domain Error: coth(x) requires x != 0` |
| $\text{sech}(x)$ | `sech`,  `sch` | All $x \in \mathbb{R}$ | None |
| $\text{csch}(x)$ | `csch`,  `cosech` | $x \ne 0$ | `Domain Error: csch(x) requires x != 0` |

---

## 5. Inverse Hyperbolic Functions

Inverse hyperbolic functions calculate the area hyperbolic values for real arguments.

| Function | Aliases | Valid Domain | Error Message |
| :--- | :--- | :--- | :--- |
| $\text{arsinh}(x)$ | `asinh`,  `ahsin`,  `arcsinh`,  `arsinh`,  `arsh` | All $x \in \mathbb{R}$ | None |
| $\text{arcosh}(x)$ | `acosh`,  `ahcos`,  `arccosh`,  `arcosh`,  `arch` | $x \ge 1$ | `Domain Error: acosh(x) requires x >= 1` |
| $\text{artanh}(x)$ | `atanh`,  `ahtan`,  `arctanh`,  `artanh`,  `arth`,  `artg`,  `arctgh` | $-1 < x < 1$ | `Domain Error: atanh(x) requires -1 < x < 1` |
| $\text{arcoth}(x)$ | `acoth`,  `ahcot`,  `arccoth`,  `arcoth`,  `arcth`,  `ahcoth` | $|x| > 1$ | `Domain Error: acoth(x) requires \|x\| > 1` |
| $\text{arsech}(x)$ | `asech`,  `ahsec`,  `arcsech`,  `arsech`,  `ahsech` | $0 < x \le 1$ | `Domain Error: asech(x) requires 0 < x <= 1` |
| $\text{arcsch}(x)$ | `acsch`,  `ahcsc`,  `arccsch`,  `arcsch`,  `acosech`,  `arccosech`,  `ahcsch` | $x \ne 0$ | `Domain Error: acsch(x) requires x != 0` |

---

## 6. General Mathematical Domain & Overflow Errors

The engine validates arguments before computing logarithms, square roots, factorials, and combinatorics:

| Operation | Valid Domain | Error Message |
| :--- | :--- | :--- |
| `sqrt(x)` | $x \ge 0$ | `Domain Error: sqrt(x) requires x >= 0` |
| `ln(x)` | $x > 0$ | `Domain Error: ln(x) requires x > 0` |
| `log(x)` / `log10(x)` | $x > 0$ | `Domain Error: log(x) requires x > 0` |
| `log2(x)` | $x > 0$ | `Domain Error: log2(x) requires x > 0` |
| `logb(x, b)` | $x > 0,\ b > 0,\ b \ne 1$ | `Domain Error: logb(x, b) requires x > 0 and b > 0, b != 1` |
| `fact(n)` / `n!` | $0 \le n \le 170$ (integer) | `Domain Error: fact(n) requires n >= 0` / `Domain Error: fact(n) requires integer n >= 0` / `Overflow Error: fact(n) exceeds maximum limit (n <= 170)` |
| `fib(n)` | $0 \le n \le 1476$ (integer) | `Domain Error: fib(n) requires n >= 0` / `Domain Error: fib(n) requires integer n >= 0` / `Overflow Error: fib(n) exceeds maximum limit (n <= 1476)` |
| `C(n, k)` / `Cnk(n, k)` | $0 \le k \le n$ (integers) | `Domain Error: C(n, k) requires 0 <= k <= n` / `Domain Error: C(n, k) requires integer arguments` / `Overflow Error: C(n, k) exceeds maximum limit` |
| `P(n, k)` | $0 \le k \le n$ (integers) | `Domain Error: P(n, k) requires 0 <= k <= n` / `Domain Error: P(n, k) requires integer arguments` / `Overflow Error: P(n, k) exceeds maximum limit` |
| Division by zero | Denominator $\ne 0$ | `Division by Zero` |
| Floating-point overflow | Finite result | `Overflow Error` |

---

## 7. Vectorized Array Evaluation

All trigonometric and hyperbolic functions operate element-wise when passed an array or vector:

```text
> t = [0,  30,  45,  90]
> sin(t)
= [0,  0.5,  0.707107,  1]

> asec([1,  2,  -2])
= [0 (deg),  60 (deg),  120 (deg)]
```

If any element violates domain constraints (e.g.  `asec([0.5,  2])`),  evaluation immediately halts and returns the specific domain error message.
