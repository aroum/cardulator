#!/usr/bin/env bash

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Default options
CLEAN=false
FLASH=false
TEST=false
DEBUG=false
MERGE=false
PLATFORM="cardputer"

# Help function
show_help() {
    echo "Usage: ./build_all.sh [options]"
    echo "Options:"
    echo "  -p, --platform <name> Target platform: 'cardputer', 'tdeck-pro', or 'all' (default: cardputer)"
    echo "  -m, --merge           Merge binaries into single flashable cardulator-<platform>.bin"
    echo "  -c, --clean           Clean build directories"
    echo "  -f, --flash           Build and flash to device"
    echo "  -t, --test            Run native tests"
    echo "  -d, --debug           Enable USB CDC Serial debugging"
    echo "  -h, --help            Show this help message"
}

# Parse options
while [[ "$#" -gt 0 ]]; do
    case $1 in
        -p|--platform)
            if [[ -n "$2" && "$2" != -* ]]; then
                PLATFORM="$2"
                shift
            else
                echo -e "${RED}Error: --platform requires an argument ('cardputer', 'tdeck-pro', 'all')${NC}"
                exit 1
            fi
            ;;
        -m|--merge) MERGE=true ;;
        -c|--clean) CLEAN=true ;;
        -f|--flash) FLASH=true ;;
        -t|--test) TEST=true ;;
        -d|--debug) DEBUG=true ;;
        -h|--help) show_help; exit 0 ;;
        *) echo "Unknown parameter passed: $1"; show_help; exit 1 ;;
    esac
    shift
done

# Normalize platform name
case "$PLATFORM" in
    cardputer|m5cardputer)
        ENVS=("cardputer")
        ;;
    tdeck-pro|tdeck|t-deck-pro|t-deck)
        ENVS=("tdeck-pro")
        ;;
    all)
        ENVS=("cardputer" "tdeck-pro")
        ;;
    *)
        echo -e "${RED}Error: Unknown platform '$PLATFORM'. Valid choices: 'cardputer', 'tdeck-pro', 'all'${NC}"
        exit 1
        ;;
esac

if [ "$DEBUG" = true ]; then
    export PLATFORMIO_BUILD_FLAGS="-DCDC_DEBUG"
    echo -e "${YELLOW}=== USB CDC Serial Debugging Enabled (-DCDC_DEBUG) ===${NC}"
fi

# Detect pio command
if command -v pio &> /dev/null; then
    PIO_CMD="pio"
elif [ -f "$HOME/.platformio/penv/bin/pio" ]; then
    PIO_CMD="$HOME/.platformio/penv/bin/pio"
else
    echo -e "${RED}Error: PlatformIO CLI (pio) not found!${NC}"
    exit 1
fi

# Clean if requested
if [ "$CLEAN" = true ]; then
    echo -e "${YELLOW}=== Cleaning PlatformIO build directories ===${NC}"
    $PIO_CMD run --target clean
fi

# Run tests if requested
if [ "$TEST" = true ]; then
    echo -e "${YELLOW}=== Running Native Tests ===${NC}"
    $PIO_CMD test -e native
    if [ $? -ne 0 ]; then
        echo -e "${RED}Tests failed!${NC}"
        exit 1
    fi
fi

# Build or Flash each target environment
for env in "${ENVS[@]}"; do
    if [ "$FLASH" = true ]; then
        echo -e "${YELLOW}=== Building and Flashing [$env] Firmware ===${NC}"
        $PIO_CMD run -e "$env" --target upload
    else
        echo -e "${YELLOW}=== Building [$env] Firmware ===${NC}"
        $PIO_CMD run -e "$env"
    fi

    if [ $? -ne 0 ]; then
        echo -e "${RED}=== Build Failed for [$env]! ===${NC}"
        exit 1
    fi

    if [ "$MERGE" = true ]; then
        echo -e "${YELLOW}=== Merging [$env] Binary ===${NC}"
        FW_PATH=$(find ".pio/build/${env}/" -name "*.bin" ! -name "bootloader.bin" ! -name "partitions.bin" | head -n 1)
        OUT_BIN="cardulator-${env}.bin"
        if command -v esptool.py &> /dev/null; then
            ESPTOOL_CMD="esptool.py"
        elif [ -f "$HOME/.platformio/penv/bin/esptool.py" ]; then
            ESPTOOL_CMD="$HOME/.platformio/penv/bin/esptool.py"
        elif [ -f "$HOME/.platformio/packages/tool-esptoolpy/esptool.py" ]; then
            ESPTOOL_CMD="python3 $HOME/.platformio/packages/tool-esptoolpy/esptool.py"
        else
            ESPTOOL_CMD=""
        fi

        if [ -n "$ESPTOOL_CMD" ] && [ -f ".pio/build/${env}/bootloader.bin" ] && [ -f ".pio/build/${env}/partitions.bin" ] && [ -f "$FW_PATH" ]; then
            $ESPTOOL_CMD --chip esp32s3 merge_bin -o "$OUT_BIN" 0x0 ".pio/build/${env}/bootloader.bin" 0x8000 ".pio/build/${env}/partitions.bin" 0x10000 "$FW_PATH"
            echo -e "${GREEN}Merged binary created: ${OUT_BIN}${NC}"
        else
            echo -e "${RED}Warning: Unable to merge binary. Ensure esptool.py is available and build outputs exist.${NC}"
        fi
    fi
done

echo -e "${GREEN}=== All Builds Completed Successfully! ===${NC}"