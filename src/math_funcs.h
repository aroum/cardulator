#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <limits>
#include <random>
#include <algorithm>
#include "app_state.h"

// Custom function wrappers for Degrees/Radians mode and domain validation
inline double custom_sin(double x) { g_last_result_angle = AngleUnit::NONE; return degreesMode ? std::sin(x * M_PI / 180.0) : std::sin(x); }
inline double custom_cos(double x) { g_last_result_angle = AngleUnit::NONE; return degreesMode ? std::cos(x * M_PI / 180.0) : std::cos(x); }
inline double custom_tan(double x) {
    g_last_result_angle = AngleUnit::NONE;
    double c = custom_cos(x);
    if (std::abs(c) < 1e-15) {
        g_math_domain_err = "Domain Error: tan(x) undefined when cos(x) = 0";
        return std::numeric_limits<double>::quiet_NaN();
    }
    return degreesMode ? std::tan(x * M_PI / 180.0) : std::tan(x);
}

inline double custom_cot(double x) {
    g_last_result_angle = AngleUnit::NONE;
    double s = custom_sin(x);
    if (std::abs(s) < 1e-15) {
        g_math_domain_err = "Domain Error: cot(x) undefined when sin(x) = 0";
        return std::numeric_limits<double>::quiet_NaN();
    }
    return custom_cos(x) / s;
}
inline double custom_ctan(double x) { return custom_cot(x); }

inline double custom_sec(double x) {
    g_last_result_angle = AngleUnit::NONE;
    double c = custom_cos(x);
    if (std::abs(c) < 1e-15) {
        g_math_domain_err = "Domain Error: sec(x) undefined when cos(x) = 0";
        return std::numeric_limits<double>::quiet_NaN();
    }
    return 1.0 / c;
}

inline double custom_csc(double x) {
    g_last_result_angle = AngleUnit::NONE;
    double s = custom_sin(x);
    if (std::abs(s) < 1e-15) {
        g_math_domain_err = "Domain Error: csc(x) undefined when sin(x) = 0";
        return std::numeric_limits<double>::quiet_NaN();
    }
    return 1.0 / s;
}

// Inverse trigonometric functions (return angle in degrees or radians)
inline double custom_asin(double x) {
    if (x < -1.0 || x > 1.0) {
        g_math_domain_err = "Domain Error: asin(x) requires -1 <= x <= 1";
        return std::numeric_limits<double>::quiet_NaN();
    }
    g_last_result_angle = degreesMode ? AngleUnit::DEG : AngleUnit::RAD;
    return degreesMode ? std::asin(x) * 180.0 / M_PI : std::asin(x);
}

inline double custom_acos(double x) {
    if (x < -1.0 || x > 1.0) {
        g_math_domain_err = "Domain Error: acos(x) requires -1 <= x <= 1";
        return std::numeric_limits<double>::quiet_NaN();
    }
    g_last_result_angle = degreesMode ? AngleUnit::DEG : AngleUnit::RAD;
    return degreesMode ? std::acos(x) * 180.0 / M_PI : std::acos(x);
}

inline double custom_atan(double x) {
    g_last_result_angle = degreesMode ? AngleUnit::DEG : AngleUnit::RAD;
    return degreesMode ? std::atan(x) * 180.0 / M_PI : std::atan(x);
}

inline double custom_atan2(double y, double x) {
    g_last_result_angle = degreesMode ? AngleUnit::DEG : AngleUnit::RAD;
    return degreesMode ? std::atan2(y, x) * 180.0 / M_PI : std::atan2(y, x);
}

inline double custom_acot(double x) {
    g_last_result_angle = degreesMode ? AngleUnit::DEG : AngleUnit::RAD;
    double rad = (M_PI / 2.0) - std::atan(x);
    return degreesMode ? rad * 180.0 / M_PI : rad;
}

inline double custom_asec(double x) {
    if (std::abs(x) < 1.0) {
        g_math_domain_err = "Domain Error: asec(x) requires |x| >= 1";
        return std::numeric_limits<double>::quiet_NaN();
    }
    g_last_result_angle = degreesMode ? AngleUnit::DEG : AngleUnit::RAD;
    double rad = std::acos(1.0 / x);
    return degreesMode ? rad * 180.0 / M_PI : rad;
}

inline double custom_acsc(double x) {
    if (std::abs(x) < 1.0) {
        g_math_domain_err = "Domain Error: acsc(x) requires |x| >= 1";
        return std::numeric_limits<double>::quiet_NaN();
    }
    g_last_result_angle = degreesMode ? AngleUnit::DEG : AngleUnit::RAD;
    double rad = std::asin(1.0 / x);
    return degreesMode ? rad * 180.0 / M_PI : rad;
}

// Hyperbolic functions (independent of Degrees/Radians mode)
inline double custom_sinh(double x) { g_last_result_angle = AngleUnit::NONE; return std::sinh(x); }
inline double custom_cosh(double x) { g_last_result_angle = AngleUnit::NONE; return std::cosh(x); }
inline double custom_tanh(double x) { g_last_result_angle = AngleUnit::NONE; return std::tanh(x); }

inline double custom_coth(double x) {
    g_last_result_angle = AngleUnit::NONE;
    if (x == 0.0) {
        g_math_domain_err = "Domain Error: coth(x) requires x != 0";
        return std::numeric_limits<double>::quiet_NaN();
    }
    return 1.0 / std::tanh(x);
}

inline double custom_sech(double x) {
    g_last_result_angle = AngleUnit::NONE;
    return 1.0 / std::cosh(x);
}

inline double custom_csch(double x) {
    g_last_result_angle = AngleUnit::NONE;
    if (x == 0.0) {
        g_math_domain_err = "Domain Error: csch(x) requires x != 0";
        return std::numeric_limits<double>::quiet_NaN();
    }
    return 1.0 / std::sinh(x);
}

inline double custom_asinh(double x) { g_last_result_angle = AngleUnit::NONE; return std::asinh(x); }

inline double custom_acosh(double x) {
    if (x < 1.0) {
        g_math_domain_err = "Domain Error: acosh(x) requires x >= 1";
        return std::numeric_limits<double>::quiet_NaN();
    }
    g_last_result_angle = AngleUnit::NONE;
    return std::acosh(x);
}

inline double custom_atanh(double x) {
    if (x <= -1.0 || x >= 1.0) {
        g_math_domain_err = "Domain Error: atanh(x) requires -1 < x < 1";
        return std::numeric_limits<double>::quiet_NaN();
    }
    g_last_result_angle = AngleUnit::NONE;
    return std::atanh(x);
}

inline double custom_acoth(double x) {
    if (std::abs(x) <= 1.0) {
        g_math_domain_err = "Domain Error: acoth(x) requires |x| > 1";
        return std::numeric_limits<double>::quiet_NaN();
    }
    g_last_result_angle = AngleUnit::NONE;
    return std::atanh(1.0 / x);
}

inline double custom_asech(double x) {
    if (x <= 0.0 || x > 1.0) {
        g_math_domain_err = "Domain Error: asech(x) requires 0 < x <= 1";
        return std::numeric_limits<double>::quiet_NaN();
    }
    g_last_result_angle = AngleUnit::NONE;
    return std::acosh(1.0 / x);
}

inline double custom_acsch(double x) {
    if (x == 0.0) {
        g_math_domain_err = "Domain Error: acsch(x) requires x != 0";
        return std::numeric_limits<double>::quiet_NaN();
    }
    g_last_result_angle = AngleUnit::NONE;
    return std::asinh(1.0 / x);
}

// Custom math functions with domain validation
inline double custom_sqrt(double x) {
    if (x < 0.0) {
        g_math_domain_err = "Domain Error: sqrt(x) requires x >= 0";
        return std::numeric_limits<double>::quiet_NaN();
    }
    g_last_result_angle = AngleUnit::NONE;
    return std::sqrt(x);
}

inline double custom_ln(double x) {
    if (x <= 0.0) {
        g_math_domain_err = "Domain Error: ln(x) requires x > 0";
        return std::numeric_limits<double>::quiet_NaN();
    }
    g_last_result_angle = AngleUnit::NONE;
    return std::log(x);
}

inline double custom_deg2rad(double deg) {
    g_last_result_angle = AngleUnit::RAD;
    return deg * M_PI / 180.0;
}
inline double custom_rad2deg(double rad) {
    g_last_result_angle = AngleUnit::DEG;
    return rad * 180.0 / M_PI;
}

inline double custom_log10(double x) {
    if (x <= 0.0) {
        g_math_domain_err = "Domain Error: log(x) requires x > 0";
        return std::numeric_limits<double>::quiet_NaN();
    }
    g_last_result_angle = AngleUnit::NONE;
    return std::log10(x);
}

inline double custom_log2(double x) {
    if (x <= 0.0) {
        g_math_domain_err = "Domain Error: log2(x) requires x > 0";
        return std::numeric_limits<double>::quiet_NaN();
    }
    g_last_result_angle = AngleUnit::NONE;
    return std::log2(x);
}

inline double custom_logb(double x, double b) {
    if (x <= 0.0 || b <= 0.0 || b == 1.0) {
        g_math_domain_err = "Domain Error: logb(x, base) requires x > 0, base > 0, base != 1";
        return std::numeric_limits<double>::quiet_NaN();
    }
    g_last_result_angle = AngleUnit::NONE;
    return std::log(x) / std::log(b);
}

inline double custom_mod(double x, double y) { g_last_result_angle = AngleUnit::NONE; return std::fmod(x, y); }
inline double custom_sgn(double x) { g_last_result_angle = AngleUnit::NONE; return (x > 0.0) - (x < 0.0); }
inline double custom_trunc(double x) { g_last_result_angle = AngleUnit::NONE; return std::trunc(x); }
inline double custom_abs(double x) { g_last_result_angle = AngleUnit::NONE; return std::abs(x); }

inline double custom_fact(double n) {
    if (n < 0.0) {
        g_math_domain_err = "Domain Error: fact(n) requires n >= 0";
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (std::floor(n) != n) {
        g_math_domain_err = "Domain Error: fact(n) requires integer n >= 0";
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (n > 170.0) {
        g_math_domain_err = "Overflow Error: fact(n) exceeds maximum limit (n <= 170)";
        return std::numeric_limits<double>::quiet_NaN();
    }
    g_last_result_angle = AngleUnit::NONE;
    double res = 1.0;
    for (int i = 2; i <= static_cast<int>(n); ++i) res *= i;
    return res;
}

inline double custom_C(double n, double k) {
    if (k < 0.0 || k > n || n < 0.0) {
        g_math_domain_err = "Domain Error: C(n, k) requires 0 <= k <= n";
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (std::floor(n) != n || std::floor(k) != k) {
        g_math_domain_err = "Domain Error: C(n, k) requires integer arguments";
        return std::numeric_limits<double>::quiet_NaN();
    }
    g_last_result_angle = AngleUnit::NONE;
    double k_eff = std::min(k, n - k);
    if (k_eff > 100000.0) {
        g_math_domain_err = "Overflow Error: C(n, k) exceeds maximum limit";
        return std::numeric_limits<double>::quiet_NaN();
    }
    double res = 1.0;
    int ki = static_cast<int>(k_eff);
    for (int i = 1; i <= ki; ++i) {
        res = res * (n - ki + i) / i;
        if (std::isinf(res)) {
            g_math_domain_err = "Overflow Error: C(n, k) exceeds maximum limit";
            return std::numeric_limits<double>::quiet_NaN();
        }
    }
    return std::round(res);
}

inline double custom_P(double n, double k) {
    if (k < 0.0 || k > n || n < 0.0) {
        g_math_domain_err = "Domain Error: P(n, k) requires 0 <= k <= n";
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (std::floor(n) != n || std::floor(k) != k) {
        g_math_domain_err = "Domain Error: P(n, k) requires integer arguments";
        return std::numeric_limits<double>::quiet_NaN();
    }
    g_last_result_angle = AngleUnit::NONE;
    if (k > 171.0) {
        g_math_domain_err = "Overflow Error: P(n, k) exceeds maximum limit";
        return std::numeric_limits<double>::quiet_NaN();
    }
    double res = 1.0;
    int ki = static_cast<int>(k);
    for (int i = 0; i < ki; ++i) {
        res *= (n - i);
        if (std::isinf(res)) {
            g_math_domain_err = "Overflow Error: P(n, k) exceeds maximum limit";
            return std::numeric_limits<double>::quiet_NaN();
        }
    }
    return res;
}
#include <numeric>

inline double custom_gcd(double a, double b) {
    g_last_result_angle = AngleUnit::NONE;
    return (double)std::gcd((int64_t)std::abs((int64_t)a), (int64_t)std::abs((int64_t)b));
}
inline double custom_lcm(double a, double b) {
    g_last_result_angle = AngleUnit::NONE;
    return (double)std::lcm((int64_t)std::abs((int64_t)a), (int64_t)std::abs((int64_t)b));
}
inline double custom_fib(double n) {
    if (n < 0.0) {
        g_math_domain_err = "Domain Error: fib(n) requires n >= 0";
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (std::floor(n) != n) {
        g_math_domain_err = "Domain Error: fib(n) requires integer n >= 0";
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (n > 1476.0) {
        g_math_domain_err = "Overflow Error: fib(n) exceeds maximum limit (n <= 1476)";
        return std::numeric_limits<double>::quiet_NaN();
    }
    int val = static_cast<int>(n);
    g_last_result_angle = AngleUnit::NONE;
    if (val == 0) return 0;
    if (val == 1) return 1;
    double a = 0, b = 1;
    for (int i = 2; i <= val; ++i) {
        double temp = a + b;
        a = b;
        b = temp;
    }
    return b;
}

inline double custom_rUni(double min_v, double max_v) {
    g_last_result_angle = AngleUnit::NONE;
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(min_v, max_v);
    return dis(gen);
}
inline double custom_rNor(double mean, double stddev) {
    g_last_result_angle = AngleUnit::NONE;
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::normal_distribution<> dis(mean, stddev);
    return dis(gen);
}

// Variadic statistics functions
inline double variadic_mean(
    double a1, double a2, double a3, double a4, double a5, double a6, double a7, double a8,
    double a9, double a10, double a11, double a12, double a13, double a14, double a15, double a16,
    double a17, double a18, double a19, double a20, double a21, double a22, double a23, double a24) {
    double sum = 0.0;
    int count = 0;
    double arr[24] = {a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24};
    for (int i = 0; i < 24; ++i) {
        if (!std::isnan(arr[i])) {
            sum += arr[i];
            count++;
        }
    }
    return count > 0 ? sum / count : std::numeric_limits<double>::quiet_NaN();
}
inline double variadic_median(
    double a1, double a2, double a3, double a4, double a5, double a6, double a7, double a8,
    double a9, double a10, double a11, double a12, double a13, double a14, double a15, double a16,
    double a17, double a18, double a19, double a20, double a21, double a22, double a23, double a24) {
    std::vector<double> vals;
    double arr[24] = {a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24};
    for (int i = 0; i < 24; ++i) {
        if (!std::isnan(arr[i])) vals.push_back(arr[i]);
    }
    if (vals.empty()) return std::numeric_limits<double>::quiet_NaN();
    std::sort(vals.begin(), vals.end());
    size_t size = vals.size();
    if (size % 2 == 0) {
        return (vals[size / 2 - 1] + vals[size / 2]) / 2.0;
    } else {
        return vals[size / 2];
    }
}
inline double variadic_var(
    double a1, double a2, double a3, double a4, double a5, double a6, double a7, double a8,
    double a9, double a10, double a11, double a12, double a13, double a14, double a15, double a16,
    double a17, double a18, double a19, double a20, double a21, double a22, double a23, double a24) {
    double m = variadic_mean(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24);
    if (std::isnan(m)) return m;
    double sum = 0.0;
    int count = 0;
    double arr[24] = {a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24};
    for (int i = 0; i < 24; ++i) {
        if (!std::isnan(arr[i])) {
            sum += (arr[i] - m) * (arr[i] - m);
            count++;
        }
    }
    return count > 1 ? sum / (count - 1) : 0.0;
}
inline double variadic_std(
    double a1, double a2, double a3, double a4, double a5, double a6, double a7, double a8,
    double a9, double a10, double a11, double a12, double a13, double a14, double a15, double a16,
    double a17, double a18, double a19, double a20, double a21, double a22, double a23, double a24) {
    double variance = variadic_var(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24);
    return std::sqrt(variance);
}

inline double variadic_mode(
    double a1, double a2, double a3, double a4, double a5, double a6, double a7, double a8,
    double a9, double a10, double a11, double a12, double a13, double a14, double a15, double a16,
    double a17, double a18, double a19, double a20, double a21, double a22, double a23, double a24) {
    std::vector<double> vals;
    double arr[24] = {a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24};
    for (int i = 0; i < 24; ++i) {
        if (!std::isnan(arr[i])) vals.push_back(arr[i]);
    }
    if (vals.empty()) return std::numeric_limits<double>::quiet_NaN();
    std::map<double, int> counts;
    for (double v : vals) counts[v]++;
    double best_val = vals[0];
    int max_c = 0;
    for (const auto& kv : counts) {
        if (kv.second > max_c) {
            max_c = kv.second;
            best_val = kv.first;
        }
    }
    return best_val;
}

inline double variadic_sum(
    double a1, double a2, double a3, double a4, double a5, double a6, double a7, double a8,
    double a9, double a10, double a11, double a12, double a13, double a14, double a15, double a16,
    double a17, double a18, double a19, double a20, double a21, double a22, double a23, double a24) {
    double sum = 0.0;
    int count = 0;
    double arr[24] = {a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24};
    for (int i = 0; i < 24; ++i) {
        if (!std::isnan(arr[i])) {
            sum += arr[i];
            count++;
        }
    }
    return count > 0 ? sum : std::numeric_limits<double>::quiet_NaN();
}

inline double variadic_prod(
    double a1, double a2, double a3, double a4, double a5, double a6, double a7, double a8,
    double a9, double a10, double a11, double a12, double a13, double a14, double a15, double a16,
    double a17, double a18, double a19, double a20, double a21, double a22, double a23, double a24) {
    double prod = 1.0;
    int count = 0;
    double arr[24] = {a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24};
    for (int i = 0; i < 24; ++i) {
        if (!std::isnan(arr[i])) {
            prod *= arr[i];
            count++;
        }
    }
    return count > 0 ? prod : std::numeric_limits<double>::quiet_NaN();
}

inline double variadic_min(
    double a1, double a2, double a3, double a4, double a5, double a6, double a7, double a8,
    double a9, double a10, double a11, double a12, double a13, double a14, double a15, double a16,
    double a17, double a18, double a19, double a20, double a21, double a22, double a23, double a24) {
    double m = std::numeric_limits<double>::infinity();
    int count = 0;
    double arr[24] = {a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24};
    for (int i = 0; i < 24; ++i) {
        if (!std::isnan(arr[i])) {
            if (arr[i] < m) m = arr[i];
            count++;
        }
    }
    return count > 0 ? m : std::numeric_limits<double>::quiet_NaN();
}

inline double variadic_max(
    double a1, double a2, double a3, double a4, double a5, double a6, double a7, double a8,
    double a9, double a10, double a11, double a12, double a13, double a14, double a15, double a16,
    double a17, double a18, double a19, double a20, double a21, double a22, double a23, double a24) {
    double m = -std::numeric_limits<double>::infinity();
    int count = 0;
    double arr[24] = {a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24};
    for (int i = 0; i < 24; ++i) {
        if (!std::isnan(arr[i])) {
            if (arr[i] > m) m = arr[i];
            count++;
        }
    }
    return count > 0 ? m : std::numeric_limits<double>::quiet_NaN();
}

inline double custom_dot(double a, double b) {
    g_last_result_angle = AngleUnit::NONE;
    return a * b;
}

inline double variadic_gcd(double a1, double a2, double a3, double a4, double a5, double a6, double a7, double a8,
    double a9, double a10, double a11, double a12, double a13, double a14, double a15, double a16,
    double a17, double a18, double a19, double a20, double a21, double a22, double a23, double a24) {
    g_last_result_angle = AngleUnit::NONE;
    double arr[24] = {a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24};
    int64_t res = 0;
    int count = 0;
    for (int i = 0; i < 24; ++i) {
        if (!std::isnan(arr[i])) {
            int64_t v = std::abs(static_cast<int64_t>(arr[i]));
            res = (count == 0) ? v : std::gcd(res, v);
            count++;
        }
    }
    return count > 0 ? static_cast<double>(res) : std::numeric_limits<double>::quiet_NaN();
}

inline double variadic_lcm(double a1, double a2, double a3, double a4, double a5, double a6, double a7, double a8,
    double a9, double a10, double a11, double a12, double a13, double a14, double a15, double a16,
    double a17, double a18, double a19, double a20, double a21, double a22, double a23, double a24) {
    g_last_result_angle = AngleUnit::NONE;
    double arr[24] = {a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24};
    int64_t res = 1;
    int count = 0;
    for (int i = 0; i < 24; ++i) {
        if (!std::isnan(arr[i])) {
            int64_t v = std::abs(static_cast<int64_t>(arr[i]));
            res = (count == 0) ? v : std::lcm(res, v);
            count++;
        }
    }
    return count > 0 ? static_cast<double>(res) : std::numeric_limits<double>::quiet_NaN();
}

// Formatting settings:
// fmt_mode: 0 = NORM, 1 = FLT, 2 = FIX, 3 = SCI, 4 = ENG
// fmt_precision: 0..12 (default 6)
// use_thousands_sep: true/false
inline int fmt_mode = 0;
inline int fmt_precision = 6;

inline std::string addDigitGrouping(const std::string& num_str) {
    size_t dot = num_str.find('.');
    std::string int_part = (dot == std::string::npos) ? num_str : num_str.substr(0, dot);
    std::string frac_part = (dot == std::string::npos) ? "" : num_str.substr(dot + 1);

    // Group integer part (right to left)
    std::string grouped_int = "";
    int count = 0;
    for (int i = (int)int_part.size() - 1; i >= 0; --i) {
        if (count > 0 && count % 3 == 0) grouped_int = " " + grouped_int;
        grouped_int = int_part[i] + grouped_int;
        count++;
    }

    if (dot == std::string::npos) return grouped_int;

    // Group fractional part (left to right): e.g. 000 001 234
    std::string grouped_frac = "";
    for (size_t i = 0; i < frac_part.size(); ++i) {
        if (i > 0 && i % 3 == 0) grouped_frac += " ";
        grouped_frac += frac_part[i];
    }

    return grouped_int + "." + grouped_frac;
}

inline std::string fmtNum(double v) {
    if (std::isnan(v) || std::isinf(v)) return "Error";
    if (v == 0.0) v = 0.0; // avoid -0

    char buf[64];
    int prec = std::clamp(fmt_precision, 0, 12);

    if (fmt_mode == 1) {
        // FLT (Pure Float, no exponent, strips trailing zeros)
        snprintf(buf, sizeof(buf), "%.*f", prec, v);
        std::string s(buf);
        if (s.find('.') != std::string::npos) {
            while (!s.empty() && s.back() == '0') s.pop_back();
            if (!s.empty() && s.back() == '.') s.pop_back();
        }
        snprintf(buf, sizeof(buf), "%s", s.c_str());
    } else if (fmt_mode == 2) {
        // FIX (Fixed point with exact trailing zeros)
        snprintf(buf, sizeof(buf), "%.*f", prec, v);
    } else if (fmt_mode == 3) {
        // SCI (Scientific e.g. 1.2345e-06)
        snprintf(buf, sizeof(buf), "%.*e", prec, v);
    } else if (fmt_mode == 4) {
        // ENG (Engineering: exponent multiple of 3)
        if (v == 0.0) {
            snprintf(buf, sizeof(buf), "%.*fe+00", prec, 0.0);
        } else {
            double abs_v = std::abs(v);
            int exp_val = static_cast<int>(std::floor(std::log10(abs_v)));
            int eng_exp = std::floor(exp_val / 3.0) * 3;
            double mantissa = v / std::pow(10.0, eng_exp);
            char sign_c = (eng_exp >= 0) ? '+' : '-';
            snprintf(buf, sizeof(buf), "%.*fe%c%02d", prec, mantissa, sign_c, std::abs(eng_exp));
        }
    } else {
        // NORM / AUTO (honor user precision, min 1)
        if (v == std::floor(v) && std::abs(v) < 1e15) {
            snprintf(buf, sizeof(buf), "%.0f", v);
        } else if (std::abs(v) >= 1e-4 && std::abs(v) < 1e7) {
            snprintf(buf, sizeof(buf), "%.*g", std::max(1, prec), v);
        } else {
            snprintf(buf, sizeof(buf), "%.*e", prec, v);
        }
    }

    std::string s = buf;
    if (!use_thousands_sep) return s;

    // Apply digit grouping to mantissa / main number part
    bool neg = !s.empty() && s[0] == '-';
    if (neg) s = s.substr(1);

    size_t e_pos = s.find_first_of("eE");
    std::string main_num = (e_pos == std::string::npos) ? s : s.substr(0, e_pos);
    std::string exp_suffix = (e_pos == std::string::npos) ? "" : s.substr(e_pos);

    std::string grouped_main = addDigitGrouping(main_num);
    std::string result = (neg ? "-" : "") + grouped_main + exp_suffix;
    return result;
}
