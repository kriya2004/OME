#pragma once

#include "order.h"
#include "types.h"
#include <map>
#include <deque>
#include <unordered_map>
#include <vector>
#include <memory>
#include <optional>
#include <algorithm>

namespace ome {

// Price level containing orders at the same price
struct PriceLevel {
    Price price;
    std::deque<Order> orders;  // FIFO queue for price-time priority
    Quantity total_quantity;

    PriceLevel() : price(0), total_quantity(0) {}
    PriceLevel(Price p) : price(p), total_quantity(0) {}

    void add_order(const Order& order) {
        total_quantity += order.quantity;
        orders.push_back(order);
    }

    void remove_order(OrderId order_id) {
        auto it = std::find_if(orders.begin(), orders.end(),
            [order_id](const Order& o) { return o.order_id == order_id; });
        
        if (it != orders.end()) {
            total_quantity -= it->quantity;
            orders.erase(it);
        }
    }

    bool is_empty() const {
        return orders.empty();
    }
};

// Single-symbol order book with matching engine
class OrderBook {
public:
    explicit OrderBook(const std::string& symbol);

    // Add a new order and attempt matching
    std::vector<Trade> add_order(const Order& order);

    // Cancel an existing order
    bool cancel_order(OrderId order_id);

    // Replace an order (cancel + new order)
    std::pair<bool, std::vector<Trade>> replace_order(
        OrderId order_id, Price new_price, Quantity new_qty);

    // Query operations
    std::optional<Order> get_order(OrderId order_id) const;
    std::pair<std::optional<Price>, std::optional<Price>> get_top_of_book() const;
    Quantity get_bid_depth() const;
    Quantity get_ask_depth() const;

    // Get book snapshot for display
    struct BookSnapshot {
        std::vector<std::pair<Price, Quantity>> bids;  // Descending price
        std::vector<std::pair<Price, Quantity>> asks;  // Ascending price
    };
    BookSnapshot get_snapshot(size_t depth = 10) const;

    const std::string& get_symbol() const { return symbol_; }

private:
    std::string symbol_;

    // Price levels: bids in descending order, asks in ascending order
    // std::map provides O(log P) operations where P = number of price levels
    std::map<Price, PriceLevel, std::greater<Price>> bid_book_;  // Descending
    std::map<Price, PriceLevel, std::less<Price>> ask_book_;     // Ascending

    // Fast order lookup: OrderId -> Order pointer
    std::unordered_map<OrderId, Order*> order_index_;

    // Matching functions
    std::vector<Trade> match_buy_order(Order& order);
    std::vector<Trade> match_sell_order(Order& order);

    // Helper to add order to book after matching
    void add_to_book(const Order& order);

    // Remove order from book
    void remove_from_book(OrderId order_id, Side side, Price price);
};

} // namespace ome
