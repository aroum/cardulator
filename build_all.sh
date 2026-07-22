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

# Help function
show_help() {
    echo "Usage: ./build_all.sh [options]"
    echo "Options:"
    echo "  -c, --clean    Clean build directories"
    echo "  -f, --flash    Build and flash to M5Cardputer (Cardulator)"
    echo "  -t, --test     Run native tests"
    echo "  -d, --debug    Enable USB CDC Serial debugging"
    echo "  -h, --help     Show this help message"
}

# Parse options
while [[ "$#" -gt 0 ]]; do
    case $1 in
        -c|--clean) CLEAN=true ;;
        -f|--flash) FLASH=true ;;
        -t|--test) TEST=true ;;
        -d|--debug) DEBUG=true ;;
        -h|--help) show_help; exit 0 ;;
        *) echo "Unknown parameter passed: $1"; show_help; exit 1 ;;
    esac
    shift
done

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

# Build or Flash
if [ "$FLASH" = true ]; then
    echo -e "${YELLOW}=== Building and Flashing Cardulator Firmware ===${NC}"
    $PIO_CMD run -e cardputer --target upload
else
    echo -e "${YELLOW}=== Building Cardulator Firmware ===${NC}"
    $PIO_CMD run -e cardputer
fi

if [ $? -eq 0 ]; then
    echo -e "${GREEN}=== Build Successful! ===${NC}"
else
    echo -e "${RED}=== Build Failed! ===${NC}"
    exit 1
fi