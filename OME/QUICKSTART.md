# Quick Start Guide

## Building the Project

```bash
cd OME
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

Or use the build script:
```bash
./tools/build.sh Release
```

## Running Tests

```bash
cd build
ctest --output-on-failure
```

All tests should pass. The test suite covers:
- Simple matching scenarios
- Partial fills
- Price-time priority (FIFO)
- Market orders
- Order cancellation and replacement
- Edge cases

## Running the CLI

```bash
cd build
./ome_cli
```

Try these commands:
```
NEW BUY 100 @ 9850
NEW SELL 50 @ 9850
TOB
STATS
EXIT
```

Or pipe commands from a file:
```bash
./ome_cli < ../tools/sample_commands.txt
```

## Running Benchmarks

### Throughput Test
```bash
./ome_bench --mode throughput --orders 100000 --pattern mixed
```

Patterns available:
- `mixed`: Random buy/sell orders (~50% fill rate)
- `aggressive`: Orders that cross the spread (~90% fill rate)
- `passive`: Non-crossing orders (~10% fill rate)

### Latency Test
```bash
./ome_bench --mode latency --orders 10000
```

This generates `latencies.csv` with per-order latency data.

## Expected Performance

On a modern x86 CPU:
- **Throughput**: 150K-250K orders/sec
- **Latency P50**: 2-4 µs
- **Latency P99**: 6-10 µs

## Profiling

Build with debug symbols:
```bash
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
make -j
```

Profile with perf:
```bash
sudo perf record --call-graph dwarf ./ome_bench --orders 100000
sudo perf report
```

Or use the profile script:
```bash
./tools/profile.sh
```

## Project Structure

```
OME/
├── include/          # Header files
│   ├── types.h       # Type definitions
│   ├── order.h       # Order struct
│   ├── order_book.h  # Order book
│   ├── matching_engine.h
│   ├── wal.h         # Write-ahead log
│   └── allocator.h   # Custom allocators
├── src/              # Implementation
├── tests/            # Unit tests
├── bench/            # Benchmarks
├── docs/             # Documentation
└── tools/            # Build scripts
```

## Key Features

✅ Price-time priority matching  
✅ Limit and market orders  
✅ Partial fills  
✅ Order cancellation & replacement  
✅ Write-ahead log (persistence)  
✅ Custom arena/pool allocators  
✅ Comprehensive unit tests  
✅ Latency & throughput benchmarks  

## Next Steps

1. **Optimize**: Integrate arena allocator with order book
2. **Profile**: Use perf to find hotspots
3. **Extend**: Add multi-threading support
4. **Scale**: Implement symbol sharding

## Common Issues

**Issue**: Tests fail to build  
**Solution**: Ensure CMake 3.14+ and C++17 compiler

**Issue**: perf command not found  
**Solution**: `sudo apt-get install linux-tools-common linux-tools-generic`

**Issue**: Permission denied for perf  
**Solution**: `sudo sysctl -w kernel.perf_event_paranoid=-1`

## Documentation

- `README.md` - Overview and usage
- `docs/DESIGN.md` - Architecture and design decisions
- `docs/PROFILE.md` - Performance analysis and optimization

## Interview Talking Points

1. **Data structures**: Explain `std::map` for price levels, `std::deque` for FIFO
2. **Integer prices**: Avoid floating-point issues, enable deterministic matching
3. **O(1) order lookup**: Hash map for cancel/replace operations
4. **Custom allocators**: Arena allocator reduces allocation overhead
5. **Profiling**: Show understanding of perf, flamegraphs, optimization methodology

## License

MIT - See LICENSE file
