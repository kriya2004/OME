#!/bin/bash
# Profile the matching engine with perf

set -e

if [ ! -f "build/ome_bench" ]; then
    echo "Building with debug symbols..."
    ./tools/build.sh RelWithDebInfo
fi

cd build

echo "=== Profiling with perf ==="
echo "Recording performance data..."

# Check if perf is available
if ! command -v perf &> /dev/null; then
    echo "Error: perf is not installed"
    echo "Install with: sudo apt-get install linux-tools-common linux-tools-generic"
    exit 1
fi

# Record
sudo perf record --call-graph dwarf -F 999 ./ome_bench --orders 100000 --pattern mixed

echo ""
echo "Generating report..."
sudo perf report --stdio > perf_report.txt

echo ""
echo "Top 20 functions:"
sudo perf report --stdio --sort symbol --percent-limit 1 | head -40

# Generate flamegraph if available
if command -v stackcollapse-perf.pl &> /dev/null; then
    echo ""
    echo "Generating flamegraph..."
    sudo perf script | stackcollapse-perf.pl | flamegraph.pl > flamegraph.svg
    echo "Flamegraph saved to build/flamegraph.svg"
else
    echo ""
    echo "Note: Install FlameGraph for visualization:"
    echo "  git clone https://github.com/brendangregg/FlameGraph.git"
    echo "  export PATH=\$PATH:\$PWD/FlameGraph"
fi

echo ""
echo "=== Profiling Complete ==="
echo "Report saved to build/perf_report.txt"
