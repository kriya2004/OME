#!/bin/bash
# Build script for Order Matching Engine

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Building Order Matching Engine ===${NC}"

# Parse arguments
BUILD_TYPE=${1:-Release}
CLEAN_BUILD=${2:-false}

echo "Build type: $BUILD_TYPE"

# Create build directory
if [ "$CLEAN_BUILD" = "clean" ]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf build
fi

mkdir -p build
cd build

# Configure
echo -e "${GREEN}Configuring CMake...${NC}"
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE ..

# Build
echo -e "${GREEN}Building...${NC}"
make -j$(nproc)

# Run tests
echo -e "${GREEN}Running tests...${NC}"
if ctest --output-on-failure; then
    echo -e "${GREEN}✓ All tests passed!${NC}"
else
    echo -e "${RED}✗ Tests failed!${NC}"
    exit 1
fi

echo -e "${GREEN}=== Build complete ===${NC}"
echo ""
echo "Executables:"
echo "  - build/ome_cli   (CLI interface)"
echo "  - build/ome_bench (Benchmarks)"
echo "  - build/ome_tests (Unit tests)"
