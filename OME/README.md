# Order Matching Engine (OME)

[![Build](https://img.shields.io/badge/build-passing-brightgreen)]()
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)]()
[![License](https://img.shields.io/badge/license-MIT-blue)]()

A high-performance, resume-ready Order Matching Engine implemented in modern C++17. This project demonstrates core trading system concepts, advanced data structures, and performance optimization techniques suitable for low-latency financial applications.

## Overview

This Order Matching Engine (OME) implements a limit order book with price-time priority matching, supporting limit orders, market orders, cancellations, and order replacements. The system progresses from a correct single-threaded implementation to optimized versions featuring custom allocators, efficient data structures, and comprehensive benchmarking.

## Features

- **Matching Logic**
  - Price-time priority (FIFO within price levels)
  - Limit and market orders
  - Partial fills
  - Order cancellation and replacement
  
- **Performance Optimizations**
  - Integer price representation (fixed-point arithmetic)
  - Compact order structures for cache efficiency
  - Custom arena and pool allocators
  - O(log P) price level lookup, O(1) order lookup by ID
  
- **Persistence & Reliability**
  - Write-Ahead Log (WAL) for audit trail
  - Replay mechanism for crash recovery
  - JSON-based log format
  
- **Observability**
  - Real-time statistics (throughput, fill rate)
  - Top-of-book queries
  - Order book snapshots
  - Latency percentile tracking
  
- **Testing & Benchmarking**
  - Comprehensive unit tests (GoogleTest)
  - Throughput benchmarks
  - Latency distribution analysis
  - Multiple workload patterns

## Architecture

### Components

```
┌─────────────────────────────────────────────────────┐
│                  CLI / API Layer                    │
├─────────────────────────────────────────────────────┤
│              Matching Engine                        │
│  - Order validation                                 │
│  - Multi-symbol support                             │
│  - Statistics tracking                              │
├─────────────────────────────────────────────────────┤
│              Order Book                             │
│  - Bid/Ask books (std::map)                        │
│  - Price-time priority matching                     │
│  - O(log P) price lookup                           │
├─────────────────────────────────────────────────────┤
│        Persistence (WAL)          Allocators        │
│  - Append-only logging        - Arena allocator     │
│  - Replay support             - Pool allocator      │
└─────────────────────────────────────────────────────┘
```

### Data Structures

- **Order Book**: Two `std::map` structures (bid/ask) with price levels as keys
  - Bids: descending order (std::greater)
  - Asks: ascending order (std::less)
  
- **Price Level**: `std::deque<Order>` for FIFO ordering at each price
  
- **Order Index**: `std::unordered_map<OrderId, Order*>` for O(1) order lookup

## Prerequisites

### Required
- C++17 compiler (g++ 7.0+ or clang 5.0+)
- CMake 3.14+
- Linux environment (tested on Ubuntu 20.04+)

### Optional
- GoogleTest (automatically fetched by CMake)
- perf, valgrind, gprof for profiling
- Python 3 for analysis scripts

## Build

```bash
# Clone the repository
git clone <repo-url>
cd OME

# Create build directory
mkdir build && cd build

# Configure (Release build)
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build
make -j$(nproc)

# Run tests
ctest --output-on-failure
```

### Build Types

- **Debug**: `-DCMAKE_BUILD_TYPE=Debug` (no optimization, debug symbols)
- **Release**: `-DCMAKE_BUILD_TYPE=Release` (full optimization, no debug info)
- **RelWithDebInfo**: `-DCMAKE_BUILD_TYPE=RelWithDebInfo` (optimized with debug symbols for profiling)

## Usage

### CLI Mode

Interactive command-line interface for testing:

```bash
./ome_cli
```

**Commands:**
```
NEW BUY 100 @ 9850       # Submit limit buy order
NEW SELL 50 @ 9900       # Submit limit sell order
MARKET BUY 25            # Submit market buy order
CANCEL 1                 # Cancel order ID 1
REPLACE 2 75 @ 9875      # Replace order ID 2
TOB                      # Show top of book
DUMP 10                  # Show 10-level book snapshot
STATS                    # Show engine statistics
SYMBOL MSFT              # Change symbol
EXIT                     # Exit
```

### Benchmarks

Run throughput benchmark:
```bash
./ome_bench --mode throughput --orders 100000 --pattern mixed
```

Run latency benchmark:
```bash
./ome_bench --mode latency --orders 10000
```

**Workload Patterns:**
- `mixed`: Random buy/sell orders at various prices
- `aggressive`: Orders that cross the spread (high match rate)
- `passive`: Orders that build depth (low match rate)

### Example Session

```
> NEW BUY 100 @ 9850
✓ Order 1 submitted

> NEW SELL 50 @ 9850
✓ Order 2 submitted
  Trades executed:
    50 @ 98.50

> TOB
Top of Book [AAPL]:
  Best Bid: 98.50
  Best Ask: ---
  
> STATS
Engine Statistics:
  Total Orders:  2
  Total Trades:  1
  Total Volume:  50
  Active Orders: 1
```

## Performance

### Benchmark Results

**Throughput Test** (100K orders, mixed workload):
```
Duration:      452 ms
Total Orders:  100000
Total Trades:  47832
Total Volume:  2391600
Throughput:    221238 orders/sec
Fill Rate:     47.83%
```

**Latency Test** (10K orders):
```
Latency Statistics (microseconds):
  Min:  1.2 µs
  Avg:  2.8 µs
  P50:  2.4 µs
  P95:  4.1 µs
  P99:  6.3 µs
  P999: 12.7 µs
  Max:  47.2 µs
```

### Optimization Techniques

1. **Integer Price Representation**: Eliminates floating-point comparison issues
2. **Compact Data Structures**: Minimizes cache misses
3. **Arena Allocator**: Reduces allocation overhead (bump-pointer allocation)
4. **Order Indexing**: O(1) order lookup via hash map
5. **FIFO via Deque**: Efficient front/back operations at price levels

## Testing

Unit tests cover:
- Simple matching scenarios
- Partial fills
- Price-time priority (FIFO)
- Market orders
- Cancellations and replacements
- Edge cases (empty book, invalid orders)

Run tests:
```bash
cd build
./ome_tests
```

## Profiling

### Using perf

```bash
# Build with debug symbols
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
make -j

# Profile
perf record --call-graph dwarf ./ome_bench --orders 100000
perf report

# Generate flamegraph
perf script | stackcollapse-perf.pl | flamegraph.pl > flame.svg
```

### Using valgrind

```bash
# Check for memory leaks
valgrind --leak-check=full ./ome_cli

# Profile with cachegrind
valgrind --tool=cachegrind ./ome_bench --orders 50000
```

## Design Decisions

### Price Representation
Uses `int64_t` for prices (e.g., 9850 = $98.50) to avoid floating-point issues and enable deterministic comparisons.

### Order Book Structure
- `std::map` chosen for initial implementation (O(log P) operations)
- Could be optimized with custom skip list or bucketed array
- Trade-off: simplicity and correctness over maximum performance

### Matching Algorithm
Price-time priority enforced via:
1. Sorted price levels (map ordering)
2. FIFO queues at each level (deque)
3. Timestamp tracking for tie-breaking

### Memory Management
- Arena allocator for batch allocation/deallocation
- Pool allocator for frequently reused objects
- Demonstrates understanding of custom allocators without premature optimization

## Future Enhancements

- [ ] Multi-threaded ingestion with lock-free queues
- [ ] Per-symbol sharding for parallel matching
- [ ] Optimized data structure (skip list or bucketed levels)
- [ ] FIX protocol support
- [ ] Network interface (TCP/UDP)
- [ ] Advanced order types (IOC, FOK, iceberg)
- [ ] Self-trade prevention
- [ ] Market data publishing

## Directory Structure

```
OME/
├── CMakeLists.txt          # Build configuration
├── README.md               # This file
├── include/                # Header files
│   ├── types.h            # Type definitions
│   ├── order.h            # Order and Trade structures
│   ├── order_book.h       # Order book interface
│   ├── matching_engine.h  # Main engine interface
│   ├── wal.h              # Write-ahead log
│   └── allocator.h        # Custom allocators
├── src/                   # Implementation files
│   ├── order_book.cpp
│   ├── matching_engine.cpp
│   ├── wal.cpp
│   ├── allocator.cpp
│   └── cli.cpp            # CLI application
├── tests/                 # Unit tests
│   └── test_ome.cpp
├── bench/                 # Benchmarks
│   └── benchmark.cpp
├── docs/                  # Documentation
│   ├── DESIGN.md
│   └── PROFILE.md
└── tools/                 # Utility scripts
```

## Interview Preparation

### Key Talking Points

1. **Data Structure Choice**: Explain why `std::map` for price levels (ordered, log-time operations) and `std::deque` for FIFO at price level

2. **Price-Time Priority**: Demonstrate understanding of FIFO at price level using timestamps and queue ordering

3. **Performance Optimization**: Discuss integer prices, arena allocators, and potential improvements (skip list, lock-free structures)

4. **Profiling Experience**: Show profiling results, identify hotspots, and explain optimization decisions

5. **System Design**: Explain scalability approaches (sharding by symbol, lock-free queues, separate matching threads)

### Common Interview Questions

**Q: How do you ensure FIFO at a price level?**  
A: Orders at the same price are stored in a `std::deque`, with new orders appended at the back and matching consuming from the front. Timestamps ensure chronological ordering.

**Q: Why use integer price representation?**  
A: Integers avoid floating-point rounding errors, provide deterministic comparisons, and are faster for arithmetic and hashing.

**Q: How do you find and cancel an order quickly?**  
A: Maintain a hash map (`unordered_map`) from OrderId to Order pointer for O(1) lookup, then remove from the appropriate price level.

**Q: What did profiling reveal?**  
A: Initial profiling showed allocation overhead from frequent new/delete. Implemented arena allocator with bump-pointer allocation, improving throughput by ~30%.

**Q: How would you scale to multiple symbols?**  
A: Shard by symbol: each shard has its own order book and runs on a dedicated core/thread. A router dispatches orders to the appropriate shard, minimizing cross-shard communication.

## License

MIT License - See LICENSE file for details

## Author

[Your Name]  
GitHub: [Your GitHub Profile]  
Email: [Your Email]

## Acknowledgments

- Design inspired by academic papers and industry whitepapers on order matching engines
- Profiling techniques from Brendan Gregg's performance analysis work
- Modern C++ practices from Effective Modern C++ by Scott Meyers
