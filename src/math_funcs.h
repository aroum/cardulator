#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <limits>
#include <random>
#include <algorithm>
#include "app_state.h"

// Custom function wrappers for Degrees/Radians mode
inline double custom_sin(double x) { return degreesMode ? std::sin(x * M_PI / 180.0) : std::sin(x); }
inline double custom_cos(double x) { return degreesMode ? std::cos(x * M_PI / 180.0) : std::cos(x); }
inline double custom_tan(double x) { return degreesMode ? std::tan(x * M_PI / 180.0) : std::tan(x); }
inline double custom_asin(double x) { return degreesMode ? std::asin(x) * 180.0 / M_PI : std::asin(x); }
inline double custom_acos(double x) { return degreesMode ? std::acos(x) * 180.0 / M_PI : std::acos(x); }
inline double custom_atan(double x) { return degreesMode ? std::atan(x) * 180.0 / M_PI : std::atan(x); }
inline double custom_ctan(double x) { return 1.0 / custom_tan(x); }

// Custom math functions
inline double custom_deg2rad(double deg) { return deg * M_PI / 180.0; }
inline double custom_rad2deg(double rad) { return rad * 180.0 / M_PI; }

inline double custom_log10(double x) { return std::log10(x); }
inline double custom_log2(double x) { return std::log2(x); }
inline double custom_logb(double b, double x) { return std::log(x) / std::log(b); }
inline double custom_mod(double x, double y) { return std::fmod(x, y); }
inline double custom_sgn(double x) { return (x > 0.0) - (x < 0.0); }
inline double custom_trunc(double x) { return std::trunc(x); }
inline double custom_abs(double x) { return std::abs(x); }

inline double custom_fact(double n) {
    if (n < 0) return std::numeric_limits<double>::quiet_NaN();
    double res = 1.0;
    for (int i = 2; i <= (int)n; ++i) res *= i;
    return res;
}
inline double custom_C(double n, double k) {
    if (k < 0 || k > n) return 0;
    return custom_fact(n) / (custom_fact(k) * custom_fact(n - k));
}
inline double custom_P(double n, double k) {
    if (k < 0 || k > n) return 0;
    return custom_fact(n) / custom_fact(n - k);
}
inline double custom_gcd(double a, double b) {
    int64_t ia = std::abs((int64_t)a);
    int64_t ib = std::abs((int64_t)b);
    while (ib) {
        int64_t temp = ib;
        ib = ia % ib;
        ia = temp;
    }
    return (double)ia;
}
inline double custom_lcm(double a, double b) {
    if (a == 0.0 && b == 0.0) return 0.0;
    return std::abs(a * b) / custom_gcd(a, b);
}
inline double custom_fib(double n) {
    int val = (int)n;
    if (val < 0) return std::numeric_limits<double>::quiet_NaN();
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
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(min_v, max_v);
    return dis(gen);
}
inline double custom_rNor(double mean, double stddev) {
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

inline double custom_dot(double a, double b) {
    return a * b;
}

// Formatting helpers
inline std::string addThousandsSep(const std::string& num_str) {
    size_t dot = num_str.find('.');
    std::string int_part = (dot == std::string::npos) ? num_str : num_str.substr(0, dot);
    std::string frac_part = (dot == std::string::npos) ? "" : num_str.substr(dot);
    
    std::string result = "";
    int count = 0;
    for (int i = (int)int_part.size() - 1; i >= 0; --i) {
        if (count > 0 && count % 3 == 0) result = " " + result;
        result = int_part[i] + result;
        count++;
    }
    return result + frac_part;
}

inline std::string fmtNum(double v) {
    if (std::isnan(v) || std::isinf(v)) return "Error";
    char buf[48];
    if (v == std::floor(v) && std::abs(v) < 1e15) {
        snprintf(buf, sizeof(buf), "%.0f", v);
    } else {
        snprintf(buf, sizeof(buf), "%.8g", v);
    }
    if (!use_thousands_sep) return buf;
    
    std::string s = buf;
    bool neg = !s.empty() && s[0] == '-';
    if (neg) s = s.substr(1);
    if (s.find('e') == std::string::npos && s.find('E') == std::string::npos) {
        s = addThousandsSep(s);
    }
    return neg ? "-" + s : s;
}
