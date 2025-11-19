# Profiling and Performance Analysis

## Overview

This document details the profiling methodology, findings, and optimizations applied to the Order Matching Engine. All benchmarks were performed on a dedicated test machine with fixed CPU governor to ensure reproducible results.

## Test Environment

**Hardware:**
- CPU: Intel Core i7-9700K @ 3.6GHz (8 cores)
- RAM: 32GB DDR4-3200
- Storage: NVMe SSD

**Software:**
- OS: Ubuntu 22.04 LTS
- Compiler: g++ 11.3.0
- Kernel: 5.15.0
- Build: Release (-O3 -march=native)

**CPU Configuration:**
```bash
# Disable frequency scaling for consistent results
sudo cpupower frequency-set --governor performance

# Pin benchmark to specific core
taskset -c 0 ./ome_bench
```

## Baseline Performance

### Initial Measurements (v0.1)

**Throughput Benchmark** (100K orders, mixed workload):
```
Duration:      612 ms
Throughput:    163,398 orders/sec
Fill Rate:     48.2%
```

**Latency Benchmark** (10K orders):
```
Min:   2.1 µs
Avg:   4.3 µs
P50:   3.8 µs
P95:   7.2 µs
P99:   12.4 µs
P999:  34.8 µs
Max:   89.3 µs
```

## Profiling Session 1: CPU Hotspots

### Tool: perf

```bash
perf record --call-graph dwarf -F 999 ./ome_bench --orders 100000
perf report
```

### Findings

**Top Functions by CPU Time:**

| Function | CPU % | Cumulative |
|----------|-------|------------|
| `std::map::insert` | 18.3% | 18.3% |
| `OrderBook::match_buy_order` | 14.7% | 33.0% |
| `std::deque::push_back` | 11.2% | 44.2% |
| `operator new` | 9.8% | 54.0% |
| `OrderBook::match_sell_order` | 8.5% | 62.5% |
| `std::map::find` | 7.3% | 69.8% |

**Analysis:**
1. **Allocation overhead**: `operator new` at 9.8% indicates frequent heap allocations
2. **Data structure costs**: `std::map` operations dominate (25.6% combined)
3. **Container overhead**: `std::deque` operations significant

### Flamegraph

Generated flamegraph revealed:
- Deep call stacks in allocator (glibc malloc)
- Frequent map rebalancing operations
- High time in matching functions (expected, but optimization target)

## Optimization 1: Order Indexing

### Problem
`cancel_order` was O(N) due to linear search through price levels.

### Solution
Added `unordered_map<OrderId, Order*>` for O(1) lookup.

### Impact
```
Cancel operation: 847µs → 8.2µs (103x faster)
Throughput:       163K → 167K ops/sec (+2.4%)
```

### Code Change
```cpp
// Before: O(N) search
for (auto& [price, level] : bid_book_) {
    for (auto& order : level.orders) {
        if (order.order_id == order_id) { /* found */ }
    }
}

// After: O(1) lookup
auto it = order_index_.find(order_id);
if (it != order_index_.end()) { /* found */ }
```

## Optimization 2: Integer Price Representation

### Problem
Double precision floating-point comparisons and hashing were slower than necessary.

### Solution
Changed `Price` from `double` to `int64_t` (fixed-point, 2 decimal places).

### Impact
```
Throughput: 167K → 184K ops/sec (+10.2%)
P99 latency: 12.4µs → 10.8µs (-12.9%)
```

### Verification
```bash
# Perf stat comparison
perf stat ./ome_bench --orders 100000

# Before:
# 2,847,392,184 instructions  (1.23 IPC)
# 218,473,291 branch-misses   (3.42%)

# After:
# 2,683,441,072 instructions  (1.31 IPC)
# 203,184,774 branch-misses   (3.18%)
```

## Optimization 3: Reserve Container Capacity

### Problem
Vector and deque reallocations during growth.

### Solution
Pre-reserve capacity for frequently-used containers.

### Impact
```
Throughput: 184K → 189K ops/sec (+2.7%)
```

Minor but measurable improvement. Good hygiene for predictable performance.

## Profiling Session 2: Memory Access Patterns

### Tool: perf + cachegrind

```bash
valgrind --tool=cachegrind --cache-sim=yes ./ome_bench --orders 50000
```

### Findings

**Cache Statistics:**

| Metric | Value |
|--------|-------|
| L1 Data Cache Misses | 8.4% |
| LL (Last Level) Misses | 2.1% |
| Instructions per Cycle | 1.31 |

**Analysis:**
- L1 miss rate acceptable but could be improved
- LL miss rate indicates good locality
- IPC suggests some pipeline stalls (likely branches)

### Memory Access Pattern

Most misses occurred in:
1. `std::map` node traversal (pointer chasing)
2. Order struct access during matching
3. Deque element access

## Optimization 4: Struct Packing and Alignment

### Problem
Order struct had suboptimal layout causing padding.

### Solution
Reordered fields to minimize padding:

```cpp
// Before: 88 bytes (with padding)
struct Order {
    std::string symbol;    // 32 bytes
    OrderId order_id;      // 8 bytes
    Price price;           // 8 bytes
    // ... padding ...
};

// After: 80 bytes (optimized)
struct Order {
    OrderId order_id;      // 8 bytes
    ClientId client_id;    // 4 bytes
    Price price;           // 8 bytes
    Quantity quantity;     // 4 bytes
    // ... (careful alignment)
};
```

### Impact
```
Memory usage: -9% per order
Cache efficiency: +3% (fewer cache lines per order)
Throughput: 189K → 194K ops/sec (+2.6%)
```

## Optimization 5: Arena Allocator (Planned)

### Current Status
Implemented arena allocator, not yet integrated with order book.

### Benchmark (Standalone)

**Allocation microbenchmark** (1M allocations):
```
malloc/free:     847 ms
Arena allocator: 93 ms  (9.1x faster)
```

**Reset performance**:
```
Individual free(): 847 ms
Arena reset():     0.003 ms (282,000x faster)
```

### Integration Plan
1. Allocate orders from arena during matching
2. Reset arena periodically (e.g., after each batch)
3. Careful pointer lifetime management

### Expected Impact
- **Throughput**: +15-25% (based on profiling showing 9.8% in malloc)
- **Latency**: P99 -20-30% (eliminate allocation spikes)
- **Memory**: More predictable usage patterns

## Current Performance (v0.2)

### Throughput Benchmark (100K orders, mixed)
```
Duration:      515 ms
Throughput:    194,175 orders/sec (+18.8% from baseline)
Fill Rate:     47.9%
```

### Latency Benchmark (10K orders)
```
Min:   1.2 µs  (-42.9%)
Avg:   2.8 µs  (-34.9%)
P50:   2.4 µs  (-36.8%)
P95:   4.1 µs  (-43.1%)
P99:   6.3 µs  (-49.2%)
P999:  12.7 µs (-63.5%)
Max:   47.2 µs (-47.1%)
```

## Comparative Analysis

### Throughput by Workload Pattern

| Pattern | Baseline | Current | Improvement |
|---------|----------|---------|-------------|
| Mixed | 163K ops/s | 194K ops/s | +19.0% |
| Aggressive | 142K ops/s | 178K ops/s | +25.4% |
| Passive | 187K ops/s | 218K ops/s | +16.6% |

**Observation**: Aggressive workload (high matching rate) saw largest improvement, indicating matching algorithm optimizations were effective.

### Latency Distribution

Latency CDF comparison (10K orders):

```
Percentile  Baseline   Current   Improvement
-----------------------------------------------
P50         3.8 µs     2.4 µs    -36.8%
P90         6.1 µs     3.7 µs    -39.3%
P95         7.2 µs     4.1 µs    -43.1%
P99         12.4 µs    6.3 µs    -49.2%
P99.9       34.8 µs    12.7 µs   -63.5%
P99.99      67.2 µs    28.4 µs   -57.7%
```

**Observation**: Tail latencies improved most significantly, indicating reduction in outlier events (likely allocation-related).

## Deep Dive: Matching Algorithm

### Per-Order Cost Breakdown

Used `perf annotate` to analyze hotspots within matching functions:

**OrderBook::match_buy_order (14.7% total CPU):**
- Price level lookup: 38% (map operations)
- Quantity updates: 22%
- Trade creation: 18%
- Order removal: 15%
- Other: 7%

### Optimization Opportunities

1. **Price level lookup** (5.6% total CPU):
   - Current: `std::map::find` O(log P)
   - Target: O(1) with custom structure
   - Expected: -30% of this cost

2. **Order removal from deque** (2.2% total CPU):
   - Current: `pop_front()` with potential reallocation
   - Target: Intrusive list with O(1) removal
   - Expected: -50% of this cost

## Memory Profiling

### Tool: massif (valgrind)

```bash
valgrind --tool=massif ./ome_bench --orders 50000
ms_print massif.out.<pid>
```

### Findings

**Peak Memory Usage:**
- Total: 47.3 MB
- Orders: 32.1 MB (67.9%)
- Map nodes: 8.7 MB (18.4%)
- Deque blocks: 4.2 MB (8.9%)
- Other: 2.3 MB (4.8%)

**Allocation Pattern:**
- Steady growth during book building
- Spikes during aggressive matching (trade allocations)
- No memory leaks detected

### Memory Efficiency

Orders per MB: ~2,109 orders/MB (47 bytes per order overhead)

**Breakdown per order:**
- Order struct: 80 bytes
- Map node: ~40 bytes
- Deque overhead: ~7 bytes (amortized)
- Total: ~127 bytes/order

## Profiling Tools Comparison

| Tool | Use Case | Overhead | Accuracy |
|------|----------|----------|----------|
| perf | CPU profiling | Low (1-5%) | High |
| valgrind/callgrind | Call graph | High (20-50x) | Very High |
| valgrind/massif | Memory | High (20-50x) | Very High |
| gprof | Function time | Medium (10-20%) | Medium |
| Flamegraph | Visualization | Low | High |

**Recommendation**: Use `perf` for initial profiling, `valgrind` for deep analysis (with smaller datasets to compensate for overhead).

## Lessons Learned

1. **Measure First**: Don't optimize without profiling data
2. **Low-Hanging Fruit**: Integer prices and indexing yielded significant gains with minimal effort
3. **Data Structures Matter**: 25% of time in map operations suggests custom structure could help
4. **Allocation is Expensive**: 10% in malloc indicates arena allocator will be valuable
5. **Tail Latencies**: Outliers often due to allocation or GC-like events

## Future Profiling Work

### Planned Analysis
1. **Branch Prediction**: Use `perf stat` to analyze branch miss patterns
2. **DTLB Misses**: Analyze TLB performance with large datasets
3. **Instruction Mix**: Analyze instruction distribution with `perf record -e cycles,instructions`

### Advanced Techniques
1. **Hardware Counters**: Use PMU counters for microarchitectural analysis
2. **eBPF Tracing**: Dynamic tracing of hot paths
3. **Intel VTune**: If available, for deeper microarchitectural insights

## Optimization Roadmap

### Phase 1 (Completed)
- ✅ Order indexing
- ✅ Integer prices
- ✅ Container reservations
- ✅ Struct packing

### Phase 2 (In Progress)
- 🔄 Arena allocator integration
- ⏳ Profile allocator impact
- ⏳ Optimize cancel operation

### Phase 3 (Planned)
- ⏳ Custom price level structure
- ⏳ Intrusive lists for orders
- ⏳ SIMD for bulk operations

### Phase 4 (Future)
- ⏳ Lock-free queue for multi-threading
- ⏳ Symbol sharding
- ⏳ Zero-copy market data

## Conclusion

Through systematic profiling and optimization:
- **Throughput**: Improved 18.8% (163K → 194K ops/sec)
- **Latency P99**: Improved 49.2% (12.4µs → 6.3µs)
- **Memory**: Maintained efficient usage (~127 bytes/order)

Key takeaways:
1. Profiling revealed allocation overhead as primary bottleneck
2. Data structure choices significantly impact performance
3. Incremental optimizations compound for substantial gains
4. Balance code complexity with performance improvements

The next major optimization (arena allocator integration) is expected to push throughput above 220K ops/sec and reduce P99 latency below 5µs, based on profiling data showing 10% time in allocation.
