#include "matching_engine.h"
#include <iostream>
#include <chrono>
#include <random>
#include <vector>
#include <algorithm>
#include <fstream>
#include <iomanip>

using namespace ome;
using namespace std::chrono;

struct LatencyStats {
    std::vector<uint64_t> latencies;  // in nanoseconds
    
    void add(uint64_t latency_ns) {
        latencies.push_back(latency_ns);
    }
    
    void compute_and_print() {
        if (latencies.empty()) {
            std::cout << "No latency data\n";
            return;
        }
        
        std::sort(latencies.begin(), latencies.end());
        
        size_t n = latencies.size();
        uint64_t min = latencies[0];
        uint64_t max = latencies[n - 1];
        uint64_t p50 = latencies[n / 2];
        uint64_t p95 = latencies[(n * 95) / 100];
        uint64_t p99 = latencies[(n * 99) / 100];
        uint64_t p999 = latencies[(n * 999) / 1000];
        
        uint64_t sum = 0;
        for (uint64_t lat : latencies) sum += lat;
        double avg = static_cast<double>(sum) / n;
        
        std::cout << "\nLatency Statistics (microseconds):\n";
        std::cout << "  Min:  " << min / 1000.0 << " µs\n";
        std::cout << "  Avg:  " << avg / 1000.0 << " µs\n";
        std::cout << "  P50:  " << p50 / 1000.0 << " µs\n";
        std::cout << "  P95:  " << p95 / 1000.0 << " µs\n";
        std::cout << "  P99:  " << p99 / 1000.0 << " µs\n";
        std::cout << "  P999: " << p999 / 1000.0 << " µs\n";
        std::cout << "  Max:  " << max / 1000.0 << " µs\n";
    }
    
    void save_to_csv(const std::string& filename) {
        std::ofstream file(filename);
        file << "latency_ns,latency_us\n";
        for (uint64_t lat : latencies) {
            file << lat << "," << (lat / 1000.0) << "\n";
        }
    }
};

class Benchmark {
public:
    Benchmark() : engine_("bench.wal"), gen_(std::random_device{}()) {}
    
    void run_throughput_test(size_t num_orders, const std::string& pattern) {
        std::cout << "\n=== Throughput Benchmark ===\n";
        std::cout << "Orders: " << num_orders << "\n";
        std::cout << "Pattern: " << pattern << "\n\n";
        
        auto start = high_resolution_clock::now();
        
        if (pattern == "mixed") {
            run_mixed_workload(num_orders);
        } else if (pattern == "aggressive") {
            run_aggressive_matching(num_orders);
        } else if (pattern == "passive") {
            run_passive_accumulation(num_orders);
        }
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(end - start);
        
        auto stats = engine_.get_statistics();
        
        double seconds = duration.count() / 1000.0;
        double throughput = stats.total_orders / seconds;
        
        std::cout << "\nResults:\n";
        std::cout << "  Duration:      " << duration.count() << " ms\n";
        std::cout << "  Total Orders:  " << stats.total_orders << "\n";
        std::cout << "  Total Trades:  " << stats.total_trades << "\n";
        std::cout << "  Total Volume:  " << stats.total_volume << "\n";
        std::cout << "  Throughput:    " << std::fixed << std::setprecision(0) 
                  << throughput << " orders/sec\n";
        std::cout << "  Fill Rate:     " << std::fixed << std::setprecision(2)
                  << (100.0 * stats.total_trades / stats.total_orders) << "%\n";
    }
    
    void run_latency_test(size_t num_orders) {
        std::cout << "\n=== Latency Benchmark ===\n";
        std::cout << "Orders: " << num_orders << "\n\n";
        
        LatencyStats stats;
        
        for (size_t i = 0; i < num_orders; ++i) {
            auto start = high_resolution_clock::now();
            
            // Submit random order
            Side side = dist_bool_(gen_) ? Side::BUY : Side::SELL;
            Price price = dist_price_(gen_);
            Quantity qty = dist_qty_(gen_);
            
            engine_.submit_order(1, "BENCH", side, price, qty);
            
            auto end = high_resolution_clock::now();
            auto latency = duration_cast<nanoseconds>(end - start).count();
            stats.add(latency);
        }
        
        stats.compute_and_print();
        stats.save_to_csv("latencies.csv");
        std::cout << "\nLatency data saved to latencies.csv\n";
    }
    
private:
    MatchingEngine engine_;
    std::mt19937 gen_;
    std::uniform_int_distribution<Price> dist_price_{9000, 11000};
    std::uniform_int_distribution<Quantity> dist_qty_{1, 100};
    std::bernoulli_distribution dist_bool_{0.5};
    
    void run_mixed_workload(size_t num_orders) {
        for (size_t i = 0; i < num_orders; ++i) {
            Side side = dist_bool_(gen_) ? Side::BUY : Side::SELL;
            Price price = dist_price_(gen_);
            Quantity qty = dist_qty_(gen_);
            
            engine_.submit_order(1, "BENCH", side, price, qty);
        }
    }
    
    void run_aggressive_matching(size_t num_orders) {
        // Build book with limit orders
        for (size_t i = 0; i < num_orders / 2; ++i) {
            Side side = dist_bool_(gen_) ? Side::BUY : Side::SELL;
            Price price = side == Side::BUY ? 9500 : 10500;
            Quantity qty = dist_qty_(gen_);
            
            engine_.submit_order(1, "BENCH", side, price, qty);
        }
        
        // Now send aggressive orders that cross the spread
        for (size_t i = num_orders / 2; i < num_orders; ++i) {
            Side side = dist_bool_(gen_) ? Side::BUY : Side::SELL;
            Price price = side == Side::BUY ? 10500 : 9500;  // Cross spread
            Quantity qty = dist_qty_(gen_);
            
            engine_.submit_order(1, "BENCH", side, price, qty);
        }
    }
    
    void run_passive_accumulation(size_t num_orders) {
        // All orders at non-crossing prices
        for (size_t i = 0; i < num_orders; ++i) {
            Side side = dist_bool_(gen_) ? Side::BUY : Side::SELL;
            Price price = side == Side::BUY ? 
                std::uniform_int_distribution<Price>{9000, 9500}(gen_) :
                std::uniform_int_distribution<Price>{10500, 11000}(gen_);
            Quantity qty = dist_qty_(gen_);
            
            engine_.submit_order(1, "BENCH", side, price, qty);
        }
    }
};

int main(int argc, char* argv[]) {
    size_t num_orders = 100000;
    std::string mode = "throughput";
    std::string pattern = "mixed";
    
    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--orders" && i + 1 < argc) {
            num_orders = std::stoull(argv[++i]);
        } else if (arg == "--mode" && i + 1 < argc) {
            mode = argv[++i];
        } else if (arg == "--pattern" && i + 1 < argc) {
            pattern = argv[++i];
        }
    }
    
    Benchmark bench;
    
    if (mode == "throughput") {
        bench.run_throughput_test(num_orders, pattern);
    } else if (mode == "latency") {
        bench.run_latency_test(num_orders);
    } else {
        std::cerr << "Unknown mode: " << mode << "\n";
        std::cerr << "Usage: " << argv[0] 
                  << " [--orders N] [--mode throughput|latency] [--pattern mixed|aggressive|passive]\n";
        return 1;
    }
    
    return 0;
}
