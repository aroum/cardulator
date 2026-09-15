#pragma once
#if defined(CARDULATOR_TDECK_PRO)

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_TCA8418.h>
#include <GxEPD2_BW.h>
#include <M5GFX.h>
#include "../lib/M5Cardputer/src/utility/Keyboard/Keyboard.h"

// I2C Pins for Keyboard
#define TDECK_I2C_SDA          13
#define TDECK_I2C_SCL          14
#define TDECK_KEYBOARD_ADDR    0x34
#define TDECK_KEYBOARD_INT     15
#define TDECK_KEYBOARD_LED     42

// SPI Pins for E-Ink
#define TDECK_EPD_CS           34
#define TDECK_EPD_DC           35
#define TDECK_EPD_RST          -1
#define TDECK_EPD_BUSY         37
#define TDECK_EPD_SCK          36
#define TDECK_EPD_MOSI         33

// Shared SPI chip selects to disable
#define TDECK_LORA_CS           3
#define TDECK_LORA_RST          4
#define TDECK_SD_CS            48

// Screen geometry: Portrait 240x320
#define TDECK_SCR_W            240
#define TDECK_SCR_H            320

// Key matrix
#define KEYPAD_ROWS 4
#define KEYPAD_COLS 10

inline GxEPD2_BW<GxEPD2_310_GDEQ031T10, GxEPD2_310_GDEQ031T10::HEIGHT>& getTDeckEpd() {
    static GxEPD2_BW<GxEPD2_310_GDEQ031T10, GxEPD2_310_GDEQ031T10::HEIGHT> epd(
        GxEPD2_310_GDEQ031T10(TDECK_EPD_CS, TDECK_EPD_DC, TDECK_EPD_RST, TDECK_EPD_BUSY)
    );
    return epd;
}

class TDeckHAL {
public:
    static void init() {
        Serial.begin(115200);
        delay(100);
        Serial.println("[T-Deck-Pro] Initializing Cardulator Hardware...");

        // Disable interfering devices on shared SPI bus
        pinMode(TDECK_LORA_CS, OUTPUT);  digitalWrite(TDECK_LORA_CS, HIGH);
        pinMode(TDECK_LORA_RST, OUTPUT); digitalWrite(TDECK_LORA_RST, HIGH);
        pinMode(TDECK_SD_CS, OUTPUT);    digitalWrite(TDECK_SD_CS, HIGH);
        pinMode(TDECK_EPD_CS, OUTPUT);   digitalWrite(TDECK_EPD_CS, HIGH);

        // Initialize SPI for E-Ink GDEQ031T10
        SPI.begin(TDECK_EPD_SCK, -1, TDECK_EPD_MOSI, TDECK_EPD_CS);

        auto& epd = getTDeckEpd();
        epd.init(115200, true, 2, false);
        epd.setRotation(0); // 240x320 portrait

        memset(s_epd_buf, 0, sizeof(s_epd_buf));
        memset(s_epd_prev_buf, 0xFF, sizeof(s_epd_prev_buf));
        s_epd_ready = true;

        // Initialize TCA8418 Keyboard
        Wire.begin(TDECK_I2C_SDA, TDECK_I2C_SCL, 400000);
        if (!_keypad.begin(TDECK_KEYBOARD_ADDR, &Wire)) {
            Serial.println("[T-Deck-Pro] ERROR: TCA8418 keyboard controller not found at 0x34!");
        } else {
            _keypad.matrix(KEYPAD_ROWS, KEYPAD_COLS);
            _keypad.flush();
            Serial.println("[T-Deck-Pro] TCA8418 keyboard initialized successfully.");
        }

        // Initialize Keypad / Screen Backlight (GPIO 42)
        pinMode(TDECK_KEYBOARD_LED, OUTPUT);
        ledcSetup(7, 5000, 8); // Channel 7, 5kHz, 8-bit resolution (0..255)
        ledcAttachPin(TDECK_KEYBOARD_LED, 7);
        setBrightness(128);
    }

    static void setBrightness(uint8_t brightness) {
        ledcWrite(7, brightness);
    }

    static bool pollKeyboard(Keyboard_Class::KeysState& s) {
        s.reset();
        bool has_event = false;
        uint32_t now = millis();

        int avail = _keypad.available();
        while (avail > 0) {
            int k = _keypad.getEvent();
            avail--;
            if (k == 0) continue;

            bool is_press = (k & 0x80) != 0;
            k &= 0x7F;
            k--; // 0-indexed key ID (0..39)
            if (k < 0) continue;

            // Physical row/col on T-Deck Pro keypad matrix:
            // k = row * 10 + raw_col
            // PCB columns are wired inverted (col 0 on hardware is physical col 9):
            int row = k / KEYPAD_COLS;
            int col = (KEYPAD_COLS - 1) - (k % KEYPAD_COLS);

            if (row < 0 || row >= KEYPAD_ROWS || col < 0 || col >= KEYPAD_COLS) continue;

            if (is_press) {
                _last_pressed_row = row;
                _last_pressed_col = col;
            }

            // Modifier Keys on physical layout:
            // Alt (Row 2, Col 0): acts as Fn on Cardputer
            if (row == 2 && col == 0) {
                _alt_held = is_press;
                if (is_press) _alt_sticky = !_alt_sticky;
                has_event = true;
                continue;
            }
            // Shift: Left Shift (Row 3, Col 5) and Right Shift (Row 3, Col 9)
            if (row == 3 && (col == 5 || col == 9)) {
                _shift_held = is_press;
                if (is_press) _shift_sticky = !_shift_sticky;
                has_event = true;
                continue;
            }
            // Sym (Row 3, Col 8): Symbol / Number layer
            if (row == 3 && col == 8) {
                _sym_held = is_press;
                if (is_press) _sym_sticky = !_sym_sticky;
                has_event = true;
                continue;
            }

            if (!is_press) {
                if (_active_row == row && _active_col == col) {
                    _active_row = -1;
                    _active_col = -1;
                }
                continue;
            }

            // Fresh key press
            _active_row = row;
            _active_col = col;
            _press_time = now;
            _repeat_time = now;

            has_event = true;
            generateKeyState(row, col, s);
            return true;
        }

        // Auto-repeat when key is held down
        if (_active_row >= 0 && _active_col >= 0) {
            if (now - _press_time > 350 && now - _repeat_time > 80) {
                _repeat_time = now;
                generateKeyState(_active_row, _active_col, s);
                return true;
            }
        }

        return has_event;
    }

    static void flushDisplay(M5Canvas& canvas, bool force_full = false) {
        if (!s_epd_ready) return;
        auto& epd = getTDeckEpd();

        const uint16_t* rgb_pixels = (const uint16_t*)canvas.getBuffer();
        if (!rgb_pixels) return;

        int first_changed_line = -1;
        int last_changed_line = -1;

        // Inverted theme: light background (white paper), dark ink (black text)
        // Canvas has black background (0x0000) with colored/white text.
        // We invert polarity so non-black pixels become BLACK ink on WHITE paper.
        // In GxEPD2, drawBitmap(..., GxEPD_BLACK): bit=1 draws black ink, bit=0 leaves background white.
        for (int y = 0; y < TDECK_SCR_H; ++y) {
            int line_offset = y * (TDECK_SCR_W / 8);

            for (int xb = 0; xb < (TDECK_SCR_W / 8); ++xb) {
                uint8_t b = 0;
                for (int bit = 0; bit < 8; ++bit) {
                    int x = xb * 8 + bit;
                    uint16_t c = rgb_pixels[y * TDECK_SCR_W + x];
                    // Pixel non-black in RGB565 -> black ink on E-Ink paper
                    if (c != 0) {
                        b |= (1 << (7 - bit));
                    }
                }
                if (s_epd_buf[line_offset + xb] != b) {
                    s_epd_buf[line_offset + xb] = b;
                    if (first_changed_line == -1) first_changed_line = y;
                    last_changed_line = y;
                }
            }
        }

        if (first_changed_line == -1 && !force_full && !_request_full_refresh) {
            return;
        }

        if (force_full || _request_full_refresh) {
            _request_full_refresh = false;
            epd.setFullWindow();
            epd.firstPage();
            do {
                epd.fillScreen(GxEPD_WHITE);
                epd.drawBitmap(0, 0, s_epd_buf, TDECK_SCR_W, TDECK_SCR_H, GxEPD_BLACK);
            } while (epd.nextPage());
        } else {
            // Partial window refresh
            int y_start = (first_changed_line / 8) * 8;
            int y_end = ((last_changed_line + 7) / 8) * 8;
            if (y_end > TDECK_SCR_H) y_end = TDECK_SCR_H;
            int h = y_end - y_start;
            if (h <= 0) h = 8;

            epd.setPartialWindow(0, y_start, TDECK_SCR_W, h);
            epd.firstPage();
            do {
                epd.fillScreen(GxEPD_WHITE);
                epd.drawBitmap(0, 0, s_epd_buf, TDECK_SCR_W, TDECK_SCR_H, GxEPD_BLACK);
            } while (epd.nextPage());
        }
    }

    static void triggerFullRefresh() {
        _request_full_refresh = true;
    }

    static bool isSymActive() { return _sym_held || _sym_sticky; }
    static bool isAltActive() { return _alt_held || _alt_sticky; }
    static bool isShiftActive() { return _shift_held || _shift_sticky; }
    static int getLastPressedRow() { return _last_pressed_row; }
    static int getLastPressedCol() { return _last_pressed_col; }

private:
    static void generateKeyState(int row, int col, Keyboard_Class::KeysState& s) {
        bool use_alt = _alt_held || _alt_sticky;
        bool use_sym = _sym_held || _sym_sticky;
        bool use_shift = _shift_held || _shift_sticky;

        s.alt = use_alt;
        s.shift = use_shift;
        s.fn = false;

        // 1. Navigation / Modes via Alt layer (Fn)
        if (use_alt) {
            char base_ch = getBaseKey(row, col);
            if (!_alt_held) _alt_sticky = false;
            if (!_shift_held) _shift_sticky = false;
            if (!_sym_held) _sym_sticky = false;

            // Sym + Alt combination for square brackets
            if (use_sym) {
                if (base_ch == 't') { s.alt = false; s.word.push_back('['); return; }
                if (base_ch == 'y') { s.alt = false; s.word.push_back(']'); return; }
            }

            if (base_ch == 'q') { s.fn = true; s.word.push_back('q'); } // Alt + Q = Return to REPL
            else if (base_ch == 'w' || base_ch == 'i') { s.up = true; }
            else if (base_ch == 's' || base_ch == 'k') { s.down = true; }
            else if (base_ch == 'a' || base_ch == 'j') { s.left = true; }
            else if (base_ch == 'd' || base_ch == 'l') { s.right = true; }
            else if (row == 1 && col == 9) {
                // Alt + Backspace acts as Ctrl + Backspace (delete previous word)
                s.backspace = true;
                s.ctrl = true;
            }
            else if (row == 3 && col == 7) { s.tab = true; } // Alt + Space = Tab
            else if (base_ch == 'g') { s.fn = true; s.word.push_back('g'); } // Plot
            else if (base_ch == 'f') { s.fn = true; s.word.push_back('f'); } // Formulas
            else if (base_ch == 'b') { s.fn = true; s.word.push_back('b'); } // Binds
            else if (base_ch == 'h') { s.fn = true; s.word.push_back('h'); } // Help
            else if (base_ch == 'c') { s.fn = true; s.word.push_back('c'); } // Clear
            else if (base_ch == 'v') { s.fn = true; s.word.push_back('v'); } // Vars
            else if (base_ch == 'p') { s.fn = true; s.word.push_back('p'); } // Params
            else if (base_ch == 'r') { triggerFullRefresh(); }
            else if (base_ch == 't') { s.fn = true; s.word.push_back('t'); } // Alt + T = tan(
            else if (base_ch == 'e') { s.alt = false; s.word.push_back('='); } // '=' symbol into expression
            return;
        }

        // 2. Sym Layer (Numbers & Math operators)
        if (use_sym) {
            // Shift + Sym special overrides
            if (use_shift) {
                char base_ch = getBaseKey(row, col);
                if (!_shift_held) _shift_sticky = false;
                if (!_sym_held) _sym_sticky = false;
                if (base_ch == 'z') { s.word.push_back('&'); return; }  // Shift + Sym + 7 (z) = '&'
                if (base_ch == 'g') { s.word.push_back('\\'); return; } // Shift + Sym + g = '\'
                if (base_ch == 'n') { s.word.push_back('<'); return; }  // Shift + Sym + n = '<'
                if (base_ch == 'm') { s.word.push_back('>'); return; }  // Shift + Sym + m = '>'
                if (base_ch == 'u') { s.word.push_back('&'); return; }  // Shift + Sym + u = '&'
                if (base_ch == 'p') { s.word.push_back('|'); return; }  // Shift + Sym + p = '|'
                if (base_ch == 'f') { s.word.push_back('^'); return; }  // Shift + 6 = '^'
                if (base_ch == 'd') { s.word.push_back('%'); return; }  // Shift + 5 = '%'
                if (base_ch == 'o') { s.word.push_back('='); return; }  // Shift + '+' = '='
                if (base_ch == 't') { s.word.push_back('{'); return; }  // Shift + '(' = '{'
                if (base_ch == 'y') { s.word.push_back('}'); return; }  // Shift + ')' = '}'
            }

            char sym_ch = getSymKey(row, col);
            if (sym_ch == 0x08) {
                s.backspace = true;
            } else if (sym_ch == 0x0D) {
                s.enter = true;
            } else if (sym_ch != 0) {
                s.word.push_back(sym_ch);
            }
            if (!_sym_held) _sym_sticky = false;
            if (!_shift_held) _shift_sticky = false;
            return;
        }

        // 3. Base Layer (Alphabet, space, enter, backspace, and Tab on r3c6)
        if (row == 1 && col == 9) {
            s.backspace = true;
        } else if (row == 2 && col == 9) {
            s.enter = true;
        } else if (row == 3 && col == 6) {
            // Key '0' pressed without Sym acts as Tab
            s.tab = true;
        } else {
            char ch = getBaseKey(row, col);
            if (ch != 0) {
                if (use_shift && ch >= 'a' && ch <= 'z') {
                    ch = ch - 'a' + 'A';
                }
                s.word.push_back(ch);
            }
        }
        // Always consume sticky modifiers after any base layer action
        if (!_shift_held) _shift_sticky = false;
        if (!_sym_held) _sym_sticky = false;
        if (!_alt_held) _alt_sticky = false;
    }

    static char getBaseKey(int row, int col) {
        static const char base_map[KEYPAD_ROWS][KEYPAD_COLS] = {
            {'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p'},
            {'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 0x08},
            {0,   'z', 'x', 'c', 'v', 'b', 'n', 'm', '=', 0x0D},
            {0,   0,   0,   0,   0,   0,   '0', ' ', 0,   0}
        };
        return base_map[row][col];
    }

    static char getSymKey(int row, int col) {
        static const char sym_map[KEYPAD_ROWS][KEYPAD_COLS] = {
            {'#', '1', '2', '3', '(', ')', '_', '-', '+', '|'},
            {'*', '4', '5', '6', '/', ':', ';', '\'', '"', 0x08},
            {0,   '7', '8', '9', '?', '!', ',', '.', '=', 0x0D},
            {0,   0,   0,   0,   0,   0,   '0', ' ', 0,   0}
        };
        return sym_map[row][col];
    }

    static inline Adafruit_TCA8418 _keypad;
    static inline bool _alt_held = false;
    static inline bool _alt_sticky = false;
    static inline bool _shift_held = false;
    static inline bool _shift_sticky = false;
    static inline bool _sym_held = false;
    static inline bool _sym_sticky = false;
    static inline bool _request_full_refresh = false;

    static inline int _active_row = -1;
    static inline int _active_col = -1;
    static inline int _last_pressed_row = -1;
    static inline int _last_pressed_col = -1;
    static inline uint32_t _press_time = 0;
    static inline uint32_t _repeat_time = 0;

    static inline uint8_t s_epd_buf[TDECK_SCR_W * TDECK_SCR_H / 8];
    static inline uint8_t s_epd_prev_buf[TDECK_SCR_W * TDECK_SCR_H / 8];
    static inline bool s_epd_ready = false;
};

#endif
