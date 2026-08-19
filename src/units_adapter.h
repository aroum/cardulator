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

    // --- 3. Fallback to GNU Units bridge ---
    static bool s_init = false;
    if (!s_init) {
        const char *dat = units_dat.empty() ? nullptr : units_dat.c_str();
        bridge_init(dat);
        s_init = true;
        UNITS_LOGI("GNU Units bridge initialised");
    }

    double     out_value = 0.0;
    char       si_buf[128] = {};
    char       err_buf[256] = {};
    const char *want = unit_b.empty() ? nullptr : unit_b.c_str();

    int rc = bridge_convert(value,
                            unit_a.c_str(),
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
