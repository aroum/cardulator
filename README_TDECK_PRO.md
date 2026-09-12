# Cardulator for LilyGO T-Deck Pro

[Russian / Русский](README_TDECK_PRO_RU.md) | [Main README](README.md)

This document describes the port of **Cardulator** for the **LilyGO T-Deck Pro** (ESP32-S3 with 3.1" E-Ink display and physical 35-key matrix keyboard).

---

## 📱 Hardware Support & Version Differences

LilyGO has produced 3 distinct hardware revisions of the **T-Deck Pro** series. Cardulator is built and tested to run seamlessly on the E-Ink display and TCA8418 keypad across these revisions:

| Hardware Version | Haptic (DRV2605 @ 0x5A) | Expander (XL9555 @ 0x20) | Keypad Driver | Display Panel | Status |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **T-Deck Pro V1.0** | ❌ | ❌ | TCA8418 (I2C `0x34`) | GDEQ031T10 (320×240 E-Ink) | **Supported** |
| **T-Deck Pro V1.1** | ✅ | ❌ | TCA8418 (I2C `0x34`) | GDEQ031T10 (320×240 E-Ink) | **Supported** |
| **T-Deck Pro MAX** | ✅ | ✅ | TCA8418 (I2C `0x34`) | GDEQ031T10 (320×240 E-Ink) | **Supported** |

### Display & Rendering
- **Panel**: 3.1-inch Monochrome E-Paper display (320 × 240 pixels).
- **Theme**: Inverted high-contrast theme optimized for E-Ink paper: crisp black ink on clean white background.
- **Fast Partial Refresh**: Dynamic differential buffer caching updates only modified lines for low latency during text editing.
- **Full Clean Refresh**: Trigger a full E-Ink refresh cycle at any time using **`Alt + r`**.

---

## ⌨️ Keyboard Layout & Keybindings

The T-Deck Pro features a 4-row physical matrix keyboard driven by the **TCA8418** controller over I2C. Cardulator provides an intuitive multi-layer layout using **`Sym`**, **`Shift`**, and **`Alt`** modifiers.

### Modifier Behavior
- **`Alt` (Row 2, Col 0)**: Acts as the **`Fn`** modifier from M5Stack Cardputer. Used for menu mode switching, navigation, and editing shortcuts.
- **`Sym` (Row 3, Col 8)**: Toggles / latches the **Symbol & Numbers layer** (`0–9`, math symbols, punctuation).
- **`Shift` (Left `r3c5`, Right `r3c9`)**: Toggles uppercase / special symbol overrides.
- **WASD Navigation**: In all screens without text input fields (variables, constants, params, scripts, formulas), the **`w`**, **`a`**, **`s`**, **`d`** keys navigate Up/Left/Down/Right both with and without `Alt`.

---

### Physical Key Matrix & Symbol Map

#### Row 0 (Top Row)
| Key | Base | `Sym` | `Shift + Sym` | `Alt` (Fn) |
|:---:|:---:|:---:|:---:|:---:|
| **`q`** | `q` | `#` (Comment) | — | **Return to REPL** |
| **`w`** | `w` | **`1`** | — | **Up** |
| **`e`** | `e` | **`2`** | — | `=` |
| **`r`** | `r` | **`3`** | — | **Full E-Ink Refresh** |
| **`t`** | `t` | **`(`** | **`{`** | **`[`** |
| **`y`** | `y` | **`)`** | **`}`** | **`]`** |
| **`u`** | `u` | **`_`** | **`&`** (Logical AND) | — |
| **`i`** | `i` | **`-`** (Minus) | — | **Up** |
| **`o`** | `o` | **`+`** (Plus) | **`=`** | — |
| **`p`** | `p` | **`\|`** (Logical OR) | **`\|`** | Parameters (`STATE_PARAMS`) |

#### Row 1 (Middle Row)
| Key | Base | `Sym` | `Shift + Sym` | `Alt` (Fn) |
|:---:|:---:|:---:|:---:|:---:|
| **`a`** | `a` | **`*`** (Multiply) | — | **Left** |
| **`s`** | `s` | **`4`** | — | **Down** |
| **`d`** | `d` | **`5`** | **`%`** (Modulo) | **Right** |
| **`f`** | `f` | **`6`** | **`^`** (Power) | Formulas Manager (`STATE_FORMULAS`) |
| **`g`** | `g` | **`/`** (Divide) | **`\`** (Backslash) | 2D Plot Viewer (`STATE_PLOT`) |
| **`h`** | `h` | **`:`** | — | Help Popup |
| **`j`** | `j` | **`;`** (Semicolon) | — | **Left** |
| **`k`** | `k` | **`'`** (Single quote) | — | **Down** |
| **`l`** | `l` | **`"`** (Double quote) | — | **Right** |
| **`Backspace`** | Backspace | Backspace | — | **`Ctrl + Backspace`** (Delete Word) |

#### Row 2 (Lower Alphabet Row)
| Key | Base | `Sym` | `Shift + Sym` | `Alt` (Fn) |
|:---:|:---:|:---:|:---:|:---:|
| **`Alt`** | *Modifier* | *Modifier* | — | *Modifier* |
| **`z`** | `z` | **`7`** | — | — |
| **`x`** | `x` | **`8`** | — | — |
| **`c`** | `c` | **`9`** | — | Constants Manager (`STATE_CONSTS`) |
| **`v`** | `v` | **`?`** | — | Variables Manager (`STATE_VARS`) |
| **`b`** | `b` | **`!`** (Factorial / NOT) | — | Binds Manager (`STATE_BINDS`) |
| **`n`** | `n` | **`,`** (Comma) | **`<`** (Less than) | — |
| **`m`** | `m` | **`.`** (Decimal point) | **`>`** (Greater than) | — |
| **`=`** (`$`) | **`=`** | **`=`** | — | — |
| **`Enter`** | Evaluate / ↵ | Evaluate / ↵ | — | — |

#### Row 3 (Bottom Space & Control Row)
| Matrix Col | Physical Key | Base Action | `Sym` Action | `Alt` Action |
|:---:|:---:|:---:|:---:|:---:|
| `c5` | **Left Shift** | Shift latch | Shift latch | — |
| `c6` | **`0`** | **`Tab`** (Autocompletion) | **`0`** (Digit zero) | — |
| `c7` | **Space** | Space (` `) | Space (` `) | **`Tab`** |
| `c8` | **`Sym`** | Toggle Symbol Layer | Toggle Symbol Layer | — |
| `c9` | **Right Shift** | Shift latch | Shift latch | — |

---

## 🎯 Quick Reference: Brackets & Math Operators

| Symbol | How to Type |
|:---:|:---|
| **`(`** and **`)`** | `Sym + t` and `Sym + y` |
| **`[`** and **`]`** | `Alt + t` and `Alt + y` |
| **`{`** and **`}`** | `Shift + Sym + t` and `Shift + Sym + y` |
| **`<`** and **`>`** | `Shift + Sym + n` and `Shift + Sym + m` |
| **`=`** | Press the **`=`** key (`$`) directly, or `Shift + Sym + o`, or `Alt + e` |
| **`\`** | `Shift + Sym + g` |
| **`&`** | `Shift + Sym + u` |
| **`\|`** | `Sym + p` or `Shift + Sym + p` |
| **`^`** | `Shift + Sym + f` |
| **`%`** | `Shift + Sym + d` |
| **`Tab`** | Press **`0`** key without Sym, or press **`Alt + Space`** |
| **`Delete Word`** | **`Alt + Backspace`** |
| **Return to REPL** | **`Alt + q`** |

---

## 🛠️ Building and Flashing with PlatformIO

### 1. Build Environment
Cardulator uses the dedicated `tdeck-pro` environment configured in `platformio.ini`:

```bash
# Compile firmware
pio run -e tdeck-pro
```

### 2. Flash to Device
Connect the T-Deck Pro via USB-C (ensure the power switch is ON):

```bash
pio run -e tdeck-pro -t upload
```

### 3. Monitor Serial Output
Cardulator provides USB CDC diagnostic output and remote REPL control at 115200 baud:

```bash
pio device monitor -b 115200
```
