#!/bin/bash
# Run benchmarks with various configurations

set -e

# Default parameters
ORDERS=100000
OUTPUT_DIR="bench_results"

mkdir -p $OUTPUT_DIR

echo "=== Running Benchmark Suite ==="
echo "Orders per test: $ORDERS"
echo "Output directory: $OUTPUT_DIR"
echo ""

# Build if needed
if [ ! -f "build/ome_bench" ]; then
    echo "Building benchmark binary..."
    ./tools/build.sh Release
fi

cd build

# Throughput benchmarks
echo "=== Throughput Benchmarks ==="

echo "1. Mixed workload..."
./ome_bench --mode throughput --orders $ORDERS --pattern mixed | tee ../$OUTPUT_DIR/throughput_mixed.txt

echo ""
echo "2. Aggressive workload..."
./ome_bench --mode throughput --orders $ORDERS --pattern aggressive | tee ../$OUTPUT_DIR/throughput_aggressive.txt

echo ""
echo "3. Passive workload..."
./ome_bench --mode throughput --orders $ORDERS --pattern passive | tee ../$OUTPUT_DIR/throughput_passive.txt

# Latency benchmarks
echo ""
echo "=== Latency Benchmark ==="
./ome_bench --mode latency --orders 10000 | tee ../$OUTPUT_DIR/latency_results.txt

# Move latency CSV
if [ -f "latencies.csv" ]; then
    mv latencies.csv ../$OUTPUT_DIR/
fi

# Clean up WAL
rm -f bench.wal

echo ""
echo "=== Benchmark Complete ==="
echo "Results saved to $OUTPUT_DIR/"
