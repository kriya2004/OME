#pragma once

#include "order_book.h"
#include "wal.h"
#include <unordered_map>
#include <memory>
#include <atomic>

namespace ome {

// Main matching engine that coordinates order books and persistence
class MatchingEngine {
public:
    explicit MatchingEngine(const std::string& wal_path = "ome.wal");
    
    // Order operations
    struct OrderResult {
        bool success;
        OrderId order_id;
        std::vector<Trade> trades;
        std::string message;
    };
    
    OrderResult submit_order(ClientId client_id, const std::string& symbol,
                            Side side, Price price, Quantity quantity,
                            OrderType type = OrderType::LIMIT);
    
    OrderResult cancel_order(OrderId order_id);
    
    OrderResult replace_order(OrderId order_id, Price new_price, Quantity new_qty);
    
    // Query operations
    std::optional<Order> get_order(OrderId order_id) const;
    std::pair<std::optional<Price>, std::optional<Price>> 
        get_top_of_book(const std::string& symbol) const;
    
    OrderBook::BookSnapshot get_book_snapshot(const std::string& symbol, 
                                              size_t depth = 10) const;
    
    // Statistics
    struct Statistics {
        uint64_t total_orders;
        uint64_t total_trades;
        uint64_t total_volume;
        uint64_t active_orders;
    };
    
    Statistics get_statistics() const;
    
    // WAL access for replay
    WAL& get_wal() { return wal_; }
    
private:
    // Order ID generator
    std::atomic<OrderId> next_order_id_;
    
    // Order books per symbol
    std::unordered_map<std::string, std::unique_ptr<OrderBook>> order_books_;
    
    // Order tracking for cancel/replace
    struct OrderInfo {
        std::string symbol;
        OrderId order_id;
    };
    std::unordered_map<OrderId, OrderInfo> order_tracker_;
    
    // Write-ahead log
    WAL wal_;
    
    // Statistics
    std::atomic<uint64_t> total_orders_;
    std::atomic<uint64_t> total_trades_;
    std::atomic<uint64_t> total_volume_;
    
    // Get or create order book for symbol
    OrderBook* get_or_create_book(const std::string& symbol);
    
    // Generate unique order ID
    OrderId generate_order_id();
};

} // namespace ome
