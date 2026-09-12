#pragma once

#include <string>
#include <sstream>
#include <cmath>
#include <algorithm>

#ifdef __ANDROID__
#include <android/log.h>
#define UNITS_LOGI(...) __android_log_print(ANDROID_LOG_INFO, "UnitsAdapter", __VA_ARGS__)
#else
#define UNITS_LOGI(...)
#endif

/* Include the pure-C bridge — no gnu-units internals leak into C++. */
#include "units_bridge.h"

struct ConvResult {
    bool        success  = false;
    double      value    = 0.0;
    std::string unitStr;
    std::string errorMsg;
};

inline std::string cleanArgStr(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\"'");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\"'");
    return str.substr(first, last - first + 1);
}

inline ConvResult handleConv(double value,
                             const std::string& unit_a_raw,
                             const std::string& unit_b_raw = "",
                             const std::string& units_dat  = "")
{
    ConvResult res;
    std::string unit_a = cleanArgStr(unit_a_raw);
    std::string unit_b = cleanArgStr(unit_b_raw);

    if (unit_a.empty()) {
        res.errorMsg = "Empty source unit";
        return res;
    }

    auto toLower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s;
    };

    std::string la = toLower(unit_a);
    std::string lb = toLower(unit_b);

    // --- 0. Resistor Code Conversions (SMD3, SMD4, EIA-96) ---
    auto parseSMD3 = [](const std::string& code, double& ohms) -> bool {
        std::string s = code;
        if (s.empty()) return false;
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
        size_t rPos = s.find('R');
        if (rPos != std::string::npos) {
            std::string temp = s;
            if (rPos == 0) {
                // Notation like R2 -> 0.2, R22 -> 0.22, R47 -> 0.47
                temp = "0." + temp.substr(1);
            } else {
                // Notation like 2R2 -> 2.2, 4R7 -> 4.7, 0R1 -> 0.1
                temp[rPos] = '.';
            }
            try { ohms = std::stod(temp); return true; } catch(...) { return false; }
        }
        if (s.size() == 3 && std::isdigit(s[0]) && std::isdigit(s[1]) && std::isdigit(s[2])) {
            int base = (s[0] - '0') * 10 + (s[1] - '0');
            int exp = s[2] - '0';
            ohms = base * std::pow(10.0, exp);
            return true;
        }
        return false;
    };

    auto parseSMD4 = [](const std::string& code, double& ohms) -> bool {
        std::string s = code;
        if (s.empty()) return false;
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
        size_t rPos = s.find('R');
        if (rPos != std::string::npos) {
            std::string temp = s;
            if (rPos == 0) {
                temp = "0." + temp.substr(1);
            } else {
                temp[rPos] = '.';
            }
            try { ohms = std::stod(temp); return true; } catch(...) { return false; }
        }
        if (s.size() == 4 && std::isdigit(s[0]) && std::isdigit(s[1]) && std::isdigit(s[2]) && std::isdigit(s[3])) {
            int base = (s[0] - '0') * 100 + (s[1] - '0') * 10 + (s[2] - '0');
            int exp = s[3] - '0';
            ohms = base * std::pow(10.0, exp);
            return true;
        }
        return false;
    };

    auto parseEIA96 = [](const std::string& code, double& ohms) -> bool {
        std::string s = code;
        if (s.size() != 3) return false;
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
        if (!std::isdigit(s[0]) || !std::isdigit(s[1])) return false;
        int codeNum = (s[0] - '0') * 10 + (s[1] - '0');

        static const int eia96_table[97] = {
            0,
            100, 102, 105, 107, 110, 113, 115, 118, 121, 124,
            127, 130, 133, 137, 140, 143, 147, 150, 154, 158,
            162, 165, 169, 174, 178, 182, 187, 191, 196, 200,
            205, 210, 215, 221, 226, 232, 237, 243, 249, 255,
            261, 267, 274, 280, 287, 294, 301, 309, 316, 324,
            332, 340, 348, 357, 365, 374, 383, 392, 402, 412,
            422, 432, 442, 453, 464, 475, 487, 499, 511, 523,
            536, 549, 562, 576, 590, 604, 619, 634, 649, 665,
            681, 698, 715, 732, 750, 768, 787, 806, 825, 845,
            866, 887, 909, 931, 953, 976
        };
        if (codeNum < 1 || codeNum > 96) return false;
        int base = eia96_table[codeNum];

        double mult = 0.0;
        char multChar = s[2];
        switch (multChar) {
            case 'Z': mult = 0.001; break;
            case 'Y': case 'R': mult = 0.01; break;
            case 'X': case 'S': mult = 0.1; break;
            case 'A': mult = 1.0; break;
            case 'B': case 'H': mult = 10.0; break;
            case 'C': mult = 100.0; break;
            case 'D': mult = 1000.0; break;
            case 'E': mult = 10000.0; break;
            case 'F': mult = 100000.0; break;
            default: return false;
        }
        ohms = base * mult;
        return true;
    };

    auto ohmsToSMD3 = [](double ohms) -> std::string {
        if (ohms < 0) return "";
        if (ohms < 10) {
            char b[16]; snprintf(b, sizeof(b), "%.1f", ohms);
            std::string s(b); size_t p = s.find('.'); if (p != std::string::npos) s[p] = 'R';
            return s;
        }
        double temp = ohms; int exp = 0;
        while (temp >= 100.0 && exp < 9) { temp /= 10.0; exp++; }
        int val = static_cast<int>(std::round(temp));
        if (val >= 100) { val /= 10; exp++; }
        if (val < 10) return "";
        char b[16]; snprintf(b, sizeof(b), "%02d%d", val, exp);
        return std::string(b);
    };

    auto ohmsToSMD4 = [](double ohms) -> std::string {
        if (ohms < 0) return "";
        if (ohms < 100) {
            char b[16]; snprintf(b, sizeof(b), "%.2f", ohms);
            std::string s(b); size_t p = s.find('.'); if (p != std::string::npos) s[p] = 'R';
            if (s.size() > 4 && s[0] == '0') s = s.substr(1);
            return s;
        }
        double temp = ohms; int exp = 0;
        while (temp >= 1000.0 && exp < 9) { temp /= 10.0; exp++; }
        while (temp < 100.0 && exp > 0) { temp *= 10.0; exp--; }
        int val = static_cast<int>(std::round(temp));
        if (val >= 1000) { val /= 10; exp++; }
        if (val < 100) return "";
        char b[16]; snprintf(b, sizeof(b), "%03d%d", val, exp);
        return std::string(b);
    };

    auto ohmsToEIA96 = [](double ohms) -> std::string {
        if (ohms <= 0) return "";
        static const int eia96_table[97] = {
            0,
            100, 102, 105, 107, 110, 113, 115, 118, 121, 124,
            127, 130, 133, 137, 140, 143, 147, 150, 154, 158,
            162, 165, 169, 174, 178, 182, 187, 191, 196, 200,
            205, 210, 215, 221, 226, 232, 237, 243, 249, 255,
            261, 267, 274, 280, 287, 294, 301, 309, 316, 324,
            332, 340, 348, 357, 365, 374, 383, 392, 402, 412,
            422, 432, 442, 453, 464, 475, 487, 499, 511, 523,
            536, 549, 562, 576, 590, 604, 619, 634, 649, 665,
            681, 698, 715, 732, 750, 768, 787, 806, 825, 845,
            866, 887, 909, 931, 953, 976
        };
        static const struct { char c; double m; int exp; } mults[] = {
            {'Z', 0.001, -3}, {'Y', 0.01, -2}, {'X', 0.1, -1}, {'A', 1.0, 0},
            {'B', 10.0, 1}, {'C', 100.0, 2}, {'D', 1000.0, 3}, {'E', 10000.0, 4}, {'F', 100000.0, 5}
        };

        // Determine closest decade multiplier directly in O(1)
        int exp = static_cast<int>(std::floor(std::log10(ohms))) - 2;
        int m_idx = std::clamp(exp + 3, 0, 8);

        // Check best multiplier candidates around m_idx
        double bestDiff = 1e18;
        std::string bestCode = "";
        int startM = std::max(0, m_idx - 1);
        int endM = std::min(8, m_idx + 1);

        for (int mi = startM; mi <= endM; ++mi) {
            double m = mults[mi].m;
            char mc = mults[mi].c;
            double targetNorm = ohms / m;

            // Binary search in sorted eia96_table (indices 1 to 96)
            auto it = std::lower_bound(eia96_table + 1, eia96_table + 97, static_cast<int>(targetNorm));
            int idx = static_cast<int>(it - eia96_table);

            for (int candidate : {idx - 1, idx, idx + 1}) {
                if (candidate >= 1 && candidate <= 96) {
                    double calc = eia96_table[candidate] * m;
                    double diff = std::abs(calc - ohms);
                    if (diff < bestDiff) {
                        bestDiff = diff;
                        char buf[16]; snprintf(buf, sizeof(buf), "%02d%c", candidate, mc);
                        bestCode = buf;
                    }
                }
            }
        }
        return bestCode;
    };

    // Generalized Component Code Parsers (Resistor/Capacitor/Inductor)
    // - Resistors: R = decimal (e.g. 4R7 = 4.7 Ohm), digits = base * 10^exp Ohm
    // - Capacitors: pF base (e.g. 104 = 10 * 10^4 pF = 100 nF = 0.1 uF), R/P/N/U = decimal
    // - Inductors: uH base (e.g. 101 = 10 * 10^1 uH = 100 uH), R/N/U = decimal

    auto parseCapCode = [](const std::string& code, double& farads) -> bool {
        std::string s = code;
        if (s.empty()) return false;
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
        
        // Check for p, n, u RKM notation (e.g., 4p7, 10n, 1u5)
        for (char unitChar : {'P', 'N', 'U'}) {
            size_t pos = s.find(unitChar);
            if (pos != std::string::npos) {
                std::string temp = s; temp[pos] = '.';
                try {
                    double val = std::stod(temp);
                    double mult = (unitChar == 'P') ? 1e-12 : (unitChar == 'N') ? 1e-9 : 1e-6;
                    farads = val * mult;
                    return true;
                } catch(...) { return false; }
            }
        }
        if (s.size() == 3 && std::isdigit(s[0]) && std::isdigit(s[1]) && std::isdigit(s[2])) {
            int base = (s[0] - '0') * 10 + (s[1] - '0');
            int exp = s[2] - '0';
            double pF = base * std::pow(10.0, exp);
            farads = pF * 1e-12;
            return true;
        }
        return false;
    };

    auto parseIndCode = [](const std::string& code, double& henries) -> bool {
        std::string s = code;
        if (s.empty()) return false;
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
        
        // RKM notation for inductors in uH or nH (e.g. 4R7 = 4.7 uH, 10n = 10 nH)
        size_t rPos = s.find('R');
        if (rPos != std::string::npos) {
            std::string temp = s; temp[rPos] = '.';
            try { henries = std::stod(temp) * 1e-6; return true; } catch(...) { return false; }
        }
        size_t nPos = s.find('N');
        if (nPos != std::string::npos) {
            std::string temp = s; temp[nPos] = '.';
            try { henries = std::stod(temp) * 1e-9; return true; } catch(...) { return false; }
        }
        if (s.size() == 3 && std::isdigit(s[0]) && std::isdigit(s[1]) && std::isdigit(s[2])) {
            int base = (s[0] - '0') * 10 + (s[1] - '0');
            int exp = s[2] - '0';
            double uH = base * std::pow(10.0, exp);
            henries = uH * 1e-6;
            return true;
        }
        return false;
    };

    // Check if unit_a is purely numeric value string (only if value wasn't explicitly passed as numeric arg)
    double numericVal = value;
    bool isUnitANumeric = false;
    if (value != 0.0 && !unit_a.empty()) {
        size_t idx = 0;
        try {
            numericVal = std::stod(unit_a, &idx);
            if (idx == unit_a.size()) {
                isUnitANumeric = true;
            }
        } catch (...) {
            isUnitANumeric = false;
        }
    }

    // Target code requests when value is numeric or first arg is numeric string (e.g., conv(200, "smd3") or conv(200, "ohm", "smd3"))
    double targetVal = (isUnitANumeric ? numericVal : value);
    std::string targetCodeFormat = lb.empty() ? (isUnitANumeric ? la : "") : lb;
    if (targetVal > 0 && (targetCodeFormat == "smd3" || targetCodeFormat == "smd4" || targetCodeFormat == "eia96" || targetCodeFormat == "eia-96")) {
        if (targetCodeFormat == "smd3") {
            res.success = true; res.value = 0; res.unitStr = ohmsToSMD3(targetVal); return res;
        } else if (targetCodeFormat == "smd4") {
            res.success = true; res.value = 0; res.unitStr = ohmsToSMD4(targetVal); return res;
        } else if (targetCodeFormat == "eia96" || targetCodeFormat == "eia-96") {
            res.success = true; res.value = 0; res.unitStr = ohmsToEIA96(targetVal); return res;
        }
    }

    // String code parsing if unit_a is a code string (e.g., conv("01C", "ohm") or conv("104", "farad"))
    if (!isUnitANumeric) {
        double parsedCapFarads = -1.0;
        if ((lb == "f" || lb == "farad" || lb == "uf" || lb == "nf" || lb == "pf") && parseCapCode(unit_a, parsedCapFarads)) {
            res.success = true;
            if (lb == "uf") { res.value = parsedCapFarads * 1e6; res.unitStr = "uF"; }
            else if (lb == "nf") { res.value = parsedCapFarads * 1e9; res.unitStr = "nF"; }
            else if (lb == "pf") { res.value = parsedCapFarads * 1e12; res.unitStr = "pF"; }
            else { res.value = parsedCapFarads; res.unitStr = "F"; }
            return res;
        }

        double parsedIndHenries = -1.0;
        if ((lb == "h" || lb == "henry" || lb == "uh" || lb == "mh" || lb == "nh") && parseIndCode(unit_a, parsedIndHenries)) {
            res.success = true;
            if (lb == "uh") { res.value = parsedIndHenries * 1e6; res.unitStr = "uH"; }
            else if (lb == "mh") { res.value = parsedIndHenries * 1e3; res.unitStr = "mH"; }
            else if (lb == "nh") { res.value = parsedIndHenries * 1e9; res.unitStr = "nH"; }
            else { res.value = parsedIndHenries; res.unitStr = "H"; }
            return res;
        }

        double parsedResOhms = -1.0;
        auto parseInputCode = [&](const std::string& code, double& val) -> bool {
            if (code.size() == 3) {
                return parseSMD3(code, val) || parseEIA96(code, val);
            }
            return parseSMD4(code, val) || parseSMD3(code, val) || parseEIA96(code, val);
        };

        if (lb == "smd3" && parseInputCode(unit_a, parsedResOhms)) {
            res.success = true; res.value = 0; res.unitStr = ohmsToSMD3(parsedResOhms); return res;
        } else if (lb == "smd4" && parseInputCode(unit_a, parsedResOhms)) {
            res.success = true; res.value = 0; res.unitStr = ohmsToSMD4(parsedResOhms); return res;
        } else if ((lb == "eia96" || lb == "eia-96") && parseInputCode(unit_a, parsedResOhms)) {
            res.success = true; res.value = 0; res.unitStr = ohmsToEIA96(parsedResOhms); return res;
        } else if (lb == "ohm" || lb == "ohms" || lb.empty()) {
            if (parseInputCode(unit_a, parsedResOhms)) {
                res.success = true; res.value = parsedResOhms; res.unitStr = lb.empty() ? "ohm" : unit_b; return res;
            }
        }
    }

    // --- 1. AWG <-> mm^2 and Standard Metric/AWG (std_mm2, std_awg) ---
    // Standard metric cross-sections according to IEC 60228 / GOST 22483 (in mm^2)
    constexpr static const double STD_MM2_TABLE[] = {
        0.03, 0.05, 0.08, 0.12, 0.20, 0.35, 0.50, 0.75, 1.0, 1.5, 2.5, 4.0, 6.0, 10.0,
        16.0, 25.0, 35.0, 50.0, 70.0, 95.0, 120.0, 150.0, 185.0, 240.0, 300.0, 400.0,
        500.0, 625.0, 630.0, 800.0, 1000.0, 1200.0
    };
    constexpr static const size_t STD_MM2_COUNT = sizeof(STD_MM2_TABLE) / sizeof(STD_MM2_TABLE[0]);

    auto awgToMm2 = [](double awg) -> double {
        double d_mm = 0.127 * std::pow(92.0, (36.0 - awg) / 39.0);
        return (M_PI / 4.0) * d_mm * d_mm;
    };

    auto mm2ToAwg = [](double mm2) -> double {
        double d_mm = 2.0 * std::sqrt(mm2 / M_PI);
        return 36.0 - 39.0 * (std::log(d_mm / 0.127) / std::log(92.0));
    };

    // Rounding with copper safety margin (greater or equal cross-section / smaller AWG number)
    auto toStdMm2 = [&](double mm2) -> double {
        for (size_t i = 0; i < STD_MM2_COUNT; ++i) {
            if (STD_MM2_TABLE[i] >= mm2 - 1e-9) {
                return STD_MM2_TABLE[i];
            }
        }
        return STD_MM2_TABLE[STD_MM2_COUNT - 1];
    };

    auto toStdAwg = [&](double mm2) -> double {
        double exactAwg = mm2ToAwg(mm2);
        // Floor AWG gauge number to ensure standard thicker wire (more copper area)
        double stdAwg = std::floor(exactAwg + 1e-9);
        return stdAwg;
    };

    // Standard metric cross-section unit: std_mm2
    if ((la == "mm^2" || la == "mm2" || la == "awg" || la == "std_awg") && lb == "std_mm2") {
        res.success = true;
        double inputMm2 = (la == "awg" || la == "std_awg") ? awgToMm2(value) : value;
        res.value = toStdMm2(inputMm2);
        res.unitStr = "std_mm2";
        return res;
    }

    // Standard AWG integer gauge unit: std_awg
    if ((la == "mm^2" || la == "mm2" || la == "awg" || la == "std_mm2") && lb == "std_awg") {
        res.success = true;
        double inputMm2 = (la == "awg") ? awgToMm2(value) : value;
        res.value = toStdAwg(inputMm2);
        res.unitStr = "std_awg";
        return res;
    }

    // From std_mm2 / std_awg to continuous units
    if (la == "std_mm2" && (lb == "mm^2" || lb == "mm2" || lb.empty())) {
        res.success = true;
        res.value = value;
        res.unitStr = "mm^2";
        return res;
    }
    if (la == "std_mm2" && lb == "awg") {
        res.success = true;
        res.value = mm2ToAwg(value);
        res.unitStr = "AWG";
        return res;
    }
    if (la == "std_awg" && (lb == "mm^2" || lb == "mm2" || lb.empty())) {
        res.success = true;
        res.value = awgToMm2(value);
        res.unitStr = "mm^2";
        return res;
    }
    if (la == "std_awg" && lb == "awg") {
        res.success = true;
        res.value = value;
        res.unitStr = "AWG";
        return res;
    }

    if ((la == "awg" && (lb == "mm^2" || lb == "mm2" || lb.empty())) ||
        ((la == "mm^2" || la == "mm2") && lb == "awg")) {
        res.success = true;
        if (la == "awg") {
            res.value = awgToMm2(value);
            res.unitStr = "mm^2";
        } else {
            res.value = mm2ToAwg(value);
            res.unitStr = "AWG";
        }
        return res;
    }

    // --- 2. dB & Ratio / Reference conversions ---
    // Multipliers for power vs amplitude/voltage
    // dB_power     = 10 * log10(ratio) => ratio = 10^(dB / 10)
    // dB_amplitude = 20 * log10(ratio) => ratio = 10^(dB / 20)

    bool is_ratio_a = (la == "ratio" || la == "times" || la == "x" || la == "times_power" || la == "times_amp" || la == "times_v");
    bool is_ratio_b = (lb == "ratio" || lb == "times" || lb == "x" || lb == "times_power" || lb == "times_amp" || lb == "times_v");

    // a) dB <-> Ratio (power vs amplitude)
    if (la == "db" && (is_ratio_b || lb.empty())) {
        res.success = true;
        res.value = std::pow(10.0, value / 10.0); // Default to power ratio for pure dB
        res.unitStr = lb.empty() ? "times" : unit_b;
        return res;
    }
    if (la == "db_power" && is_ratio_b) {
        res.success = true;
        res.value = std::pow(10.0, value / 10.0);
        res.unitStr = unit_b;
        return res;
    }
    if ((la == "db_amp" || la == "db_v") && is_ratio_b) {
        res.success = true;
        res.value = std::pow(10.0, value / 20.0);
        res.unitStr = unit_b;
        return res;
    }
    if (is_ratio_a && lb == "db") {
        res.success = true;
        res.value = 10.0 * std::log10(value);
        res.unitStr = "dB";
        return res;
    }
    if (is_ratio_a && la == "times_amp" && lb == "db") {
        res.success = true;
        res.value = 20.0 * std::log10(value);
        res.unitStr = "dB";
        return res;
    }

    // b) Absolute dB units (dBm, dBW, dBV, dBu, dBmV, dBuV)
    struct DBRef { std::string name; std::string targetUnit; double refVal; bool isVoltage; };
    static const DBRef dbRefs[] = {
        {"dbm",  "mw", 1.0,         false}, // 1 mW
        {"dbw",  "w",  1.0,         false}, // 1 W
        {"dbv",  "v",  1.0,         true},  // 1 V
        {"dbmv", "mv", 1.0,         true},  // 1 mV
        {"dbuv", "uv", 1.0,         true},  // 1 uV
        {"dbu",  "v",  0.774596669, true}   // sqrt(0.6 mW in 600 ohm) ≈ 0.7746 V
    };

    for (const auto& ref : dbRefs) {
        double factor = ref.isVoltage ? 20.0 : 10.0;
        // From dBx to target unit (or SI)
        if (la == ref.name && (lb == ref.targetUnit || lb.empty())) {
            res.success = true;
            res.value = ref.refVal * std::pow(10.0, value / factor);
            res.unitStr = ref.targetUnit;
            return res;
        }
        // From target unit to dBx
        if (la == ref.targetUnit && lb == ref.name) {
            res.success = true;
            res.value = factor * std::log10(value / ref.refVal);
            res.unitStr = unit_b;
            return res;
        }
    }

    // --- 2.5 Built-in Standard Physical Unit Conversions (Offline / Flash fallback) ---
    {
        // Length (base: meter)
        auto getLengthFactor = [](const std::string& u, double& factor, std::string& canonical) -> bool {
            if (u == "m" || u == "meter" || u == "meters") { factor = 1.0; canonical = "m"; return true; }
            if (u == "mm") { factor = 1e-3; canonical = "mm"; return true; }
            if (u == "cm") { factor = 1e-2; canonical = "cm"; return true; }
            if (u == "km") { factor = 1e3; canonical = "km"; return true; }
            if (u == "in" || u == "inch" || u == "inches") { factor = 0.0254; canonical = "inch"; return true; }
            if (u == "ft" || u == "foot" || u == "feet") { factor = 0.3048; canonical = "ft"; return true; }
            if (u == "yd" || u == "yard" || u == "yards") { factor = 0.9144; canonical = "yd"; return true; }
            if (u == "mi" || u == "mile" || u == "miles") { factor = 1609.344; canonical = "mile"; return true; }
            if (u == "nauticalmile" || u == "nmi") { factor = 1852.0; canonical = "nmi"; return true; }
            if (u == "angstrom") { factor = 1e-10; canonical = "angstrom"; return true; }
            if (u == "au") { factor = 149597870700.0; canonical = "au"; return true; }
            return false;
        };

        // Mass (base: kg)
        auto getMassFactor = [](const std::string& u, double& factor, std::string& canonical) -> bool {
            if (u == "kg" || u == "kilogram" || u == "kilograms") { factor = 1.0; canonical = "kg"; return true; }
            if (u == "g" || u == "gram" || u == "grams") { factor = 1e-3; canonical = "g"; return true; }
            if (u == "mg") { factor = 1e-6; canonical = "mg"; return true; }
            if (u == "ug") { factor = 1e-9; canonical = "ug"; return true; }
            if (u == "lb" || u == "lbs" || u == "pound" || u == "pounds") { factor = 0.45359237; canonical = "lb"; return true; }
            if (u == "oz" || u == "ounce" || u == "ounces") { factor = 0.028349523125; canonical = "oz"; return true; }
            if (u == "ton" || u == "tonne" || u == "tonnes") { factor = 1000.0; canonical = "ton"; return true; }
            if (u == "stone") { factor = 6.35029318; canonical = "stone"; return true; }
            if (u == "carat") { factor = 0.0002; canonical = "carat"; return true; }
            return false;
        };

        // Time (base: second)
        auto getTimeFactor = [](const std::string& u, double& factor, std::string& canonical) -> bool {
            if (u == "s" || u == "sec" || u == "second" || u == "seconds") { factor = 1.0; canonical = "s"; return true; }
            if (u == "ms") { factor = 1e-3; canonical = "ms"; return true; }
            if (u == "us") { factor = 1e-6; canonical = "us"; return true; }
            if (u == "ns") { factor = 1e-9; canonical = "ns"; return true; }
            if (u == "min" || u == "minute" || u == "minutes") { factor = 60.0; canonical = "min"; return true; }
            if (u == "hr" || u == "hour" || u == "hours") { factor = 3600.0; canonical = "hr"; return true; }
            if (u == "day" || u == "days") { factor = 86400.0; canonical = "day"; return true; }
            if (u == "week" || u == "weeks") { factor = 604800.0; canonical = "week"; return true; }
            if (u == "year" || u == "years") { factor = 31557600.0; canonical = "year"; return true; }
            return false;
        };

        // Pressure (base: Pascal)
        auto getPressureFactor = [](const std::string& u, double& factor, std::string& canonical) -> bool {
            if (u == "pa" || u == "pascal") { factor = 1.0; canonical = "Pa"; return true; }
            if (u == "kpa") { factor = 1e3; canonical = "kPa"; return true; }
            if (u == "mpa") { factor = 1e6; canonical = "MPa"; return true; }
            if (u == "gpa") { factor = 1e9; canonical = "GPa"; return true; }
            if (u == "bar") { factor = 1e5; canonical = "bar"; return true; }
            if (u == "mbar") { factor = 100.0; canonical = "mbar"; return true; }
            if (u == "atm") { factor = 101325.0; canonical = "atm"; return true; }
            if (u == "torr" || u == "mmhg") { factor = 101325.0 / 760.0; canonical = (u == "mmhg" ? "mmHg" : "torr"); return true; }
            if (u == "psi") { factor = 6894.757293168; canonical = "psi"; return true; }
            if (u == "psf") { factor = 47.88025898; canonical = "psf"; return true; }
            if (u == "inhg") { factor = 3386.38866667; canonical = "inHg"; return true; }
            return false;
        };

        // Volume (base: liter)
        auto getVolumeFactor = [](const std::string& u, double& factor, std::string& canonical) -> bool {
            if (u == "l" || u == "liter" || u == "liters" || u == "litre" || u == "litres") { factor = 1.0; canonical = "l"; return true; }
            if (u == "ml") { factor = 1e-3; canonical = "ml"; return true; }
            if (u == "dl") { factor = 1e-1; canonical = "dl"; return true; }
            if (u == "cl") { factor = 1e-2; canonical = "cl"; return true; }
            if (u == "m^3" || u == "m3") { factor = 1000.0; canonical = "m^3"; return true; }
            if (u == "cm^3" || u == "cm3") { factor = 1e-3; canonical = "cm^3"; return true; }
            if (u == "gal" || u == "gallon" || u == "gallons") { factor = 3.785411784; canonical = "gal"; return true; }
            if (u == "quart" || u == "quarts") { factor = 0.946352946; canonical = "quart"; return true; }
            if (u == "pint" || u == "pints") { factor = 0.473176473; canonical = "pint"; return true; }
            if (u == "cup" || u == "cups") { factor = 0.2365882365; canonical = "cup"; return true; }
            if (u == "floz") { factor = 0.0295735295625; canonical = "floz"; return true; }
            if (u == "tbsp") { factor = 0.01478676478125; canonical = "tbsp"; return true; }
            if (u == "tsp") { factor = 0.00492892159375; canonical = "tsp"; return true; }
            if (u == "barrel") { factor = 158.987294928; canonical = "barrel"; return true; }
            return false;
        };

        // Temperature (special conversion)
        auto isTemp = [](const std::string& u) {
            return u == "degc" || u == "degf" || u == "k" || u == "kelvin";
        };

        if (isTemp(la) && (isTemp(lb) || lb.empty())) {
            double k = 0.0;
            if (la == "degc") k = value + 273.15;
            else if (la == "degf") k = (value - 32.0) * 5.0 / 9.0 + 273.15;
            else k = value;

            res.success = true;
            if (lb.empty() || lb == "k" || lb == "kelvin") {
                res.value = k;
                res.unitStr = "K";
            } else if (lb == "degc") {
                res.value = k - 273.15;
                res.unitStr = "degC";
            } else if (lb == "degf") {
                res.value = (k - 273.15) * 9.0 / 5.0 + 32.0;
                res.unitStr = "degF";
            }
            return res;
        }

        // Check standard dimension groups
        double f_a = 0.0, f_b = 0.0;
        std::string can_a, can_b;
        if (getLengthFactor(la, f_a, can_a)) {
            if (lb.empty()) {
                res.success = true;
                res.value = value * f_a;
                res.unitStr = "m";
                return res;
            } else if (getLengthFactor(lb, f_b, can_b)) {
                res.success = true;
                res.value = (value * f_a) / f_b;
                res.unitStr = unit_b.empty() ? can_b : unit_b;
                return res;
            }
        }
        if (getMassFactor(la, f_a, can_a)) {
            if (lb.empty()) {
                res.success = true;
                res.value = value * f_a;
                res.unitStr = "kg";
                return res;
            } else if (getMassFactor(lb, f_b, can_b)) {
                res.success = true;
                res.value = (value * f_a) / f_b;
                res.unitStr = unit_b.empty() ? can_b : unit_b;
                return res;
            }
        }
        if (getTimeFactor(la, f_a, can_a)) {
            if (lb.empty()) {
                res.success = true;
                res.value = value * f_a;
                res.unitStr = "s";
                return res;
            } else if (getTimeFactor(lb, f_b, can_b)) {
                res.success = true;
                res.value = (value * f_a) / f_b;
                res.unitStr = unit_b.empty() ? can_b : unit_b;
                return res;
            }
        }
        if (getPressureFactor(la, f_a, can_a)) {
            if (lb.empty()) {
                res.success = true;
                res.value = value * f_a;
                res.unitStr = "Pa";
                return res;
            } else if (getPressureFactor(lb, f_b, can_b)) {
                res.success = true;
                res.value = (value * f_a) / f_b;
                res.unitStr = unit_b.empty() ? can_b : unit_b;
                return res;
            }
        }
        if (getVolumeFactor(la, f_a, can_a)) {
            if (lb.empty()) {
                res.success = true;
                res.value = value * f_a;
                res.unitStr = "l";
                return res;
            } else if (getVolumeFactor(lb, f_b, can_b)) {
                res.success = true;
                res.value = (value * f_a) / f_b;
                res.unitStr = unit_b.empty() ? can_b : unit_b;
                return res;
            }
        }
    }

    // --- 3. Fallback to GNU Units bridge ---
    static bool s_init = false;
    if (!s_init) {
        const char *dat = units_dat.empty() ? nullptr : units_dat.c_str();
        bridge_init(dat);
        s_init = true;
        UNITS_LOGI("GNU Units bridge initialised");
    }

    // Normalize commonly typed case variations for GNU Units database
    auto normalizeGnuUnit = [&](const std::string& u) -> std::string {
        std::string low = toLower(u);
        if (low == "mmhg") return "mmHg";
        if (low == "inhg") return "inHg";
        if (low == "pa")   return "Pa";
        if (low == "kpa")  return "kPa";
        if (low == "mpa")  return "MPa";
        if (low == "gpa")  return "GPa";
        if (low == "mbar") return "mbar";
        if (low == "hz")   return "Hz";
        if (low == "khz")  return "kHz";
        if (low == "mhz")  return "MHz";
        if (low == "ghz")  return "GHz";
        return u;
    };

    std::string norm_unit_a = normalizeGnuUnit(unit_a);
    std::string norm_unit_b = normalizeGnuUnit(unit_b);
    const char *want = norm_unit_b.empty() ? nullptr : norm_unit_b.c_str();

    double     out_value = 0.0;
    char       si_buf[128] = {};
    char       err_buf[256] = {};

    int rc = bridge_convert(value,
                            norm_unit_a.c_str(),
                            want,
                            &out_value,
                            si_buf, sizeof(si_buf),
                            err_buf, sizeof(err_buf));

    if (rc != 0) {
        res.errorMsg = err_buf[0] ? err_buf : "Conversion failed";
        return res;
    }

    res.success = true;
    res.value   = out_value;
    res.unitStr = unit_b.empty() ? std::string(si_buf) : unit_b;
    return res;
}
