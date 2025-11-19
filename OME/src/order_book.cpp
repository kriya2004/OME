#include "order_book.h"
#include <algorithm>
#include <iostream>

namespace ome {

OrderBook::OrderBook(const std::string& symbol) : symbol_(symbol) {}

std::vector<Trade> OrderBook::add_order(const Order& order) {
    std::vector<Trade> trades;
    
    // Make a mutable copy for matching
    Order working_order = order;
    
    // Match based on side
    if (order.side == Side::BUY) {
        trades = match_buy_order(working_order);
    } else {
        trades = match_sell_order(working_order);
    }
    
    // If there's remaining quantity, add to book
    if (working_order.quantity > 0 && working_order.type == OrderType::LIMIT) {
        add_to_book(working_order);
    }
    
    return trades;
}

std::vector<Trade> OrderBook::match_buy_order(Order& order) {
    std::vector<Trade> trades;
    
    // Match against ask book (lowest price first)
    for (auto& [price, level] : ask_book_) {
        // Stop if no more quantity to fill or price doesn't match
        if (order.quantity == 0) break;
        
        // For limit orders, check price
        if (order.type == OrderType::LIMIT && order.price < price) break;
        
        // Match against orders at this price level (FIFO)
        while (!level.orders.empty() && order.quantity > 0) {
            Order& maker_order = level.orders.front();
            
            // Calculate fill quantity
            Quantity fill_qty = std::min(order.quantity, maker_order.quantity);
            
            // Create trade
            trades.emplace_back(maker_order.order_id, order.order_id, 
                              price, fill_qty, symbol_);
            
            // Update quantities
            order.fill(fill_qty);
            maker_order.fill(fill_qty);
            level.total_quantity -= fill_qty;
            
            // Remove filled maker order
            if (maker_order.quantity == 0) {
                order_index_.erase(maker_order.order_id);
                level.orders.pop_front();
            } else {
                // Update pointer in index (order was modified)
                order_index_[maker_order.order_id] = &level.orders.front();
            }
        }
    }
    
    // Clean up empty price levels
    for (auto it = ask_book_.begin(); it != ask_book_.end();) {
        if (it->second.is_empty()) {
            it = ask_book_.erase(it);
        } else {
            ++it;
        }
    }
    
    return trades;
}

std::vector<Trade> OrderBook::match_sell_order(Order& order) {
    std::vector<Trade> trades;
    
    // Match against bid book (highest price first)
    for (auto& [price, level] : bid_book_) {
        // Stop if no more quantity to fill or price doesn't match
        if (order.quantity == 0) break;
        
        // For limit orders, check price
        if (order.type == OrderType::LIMIT && order.price > price) break;
        
        // Match against orders at this price level (FIFO)
        while (!level.orders.empty() && order.quantity > 0) {
            Order& maker_order = level.orders.front();
            
            // Calculate fill quantity
            Quantity fill_qty = std::min(order.quantity, maker_order.quantity);
            
            // Create trade
            trades.emplace_back(maker_order.order_id, order.order_id, 
                              price, fill_qty, symbol_);
            
            // Update quantities
            order.fill(fill_qty);
            maker_order.fill(fill_qty);
            level.total_quantity -= fill_qty;
            
            // Remove filled maker order
            if (maker_order.quantity == 0) {
                order_index_.erase(maker_order.order_id);
                level.orders.pop_front();
            } else {
                // Update pointer in index
                order_index_[maker_order.order_id] = &level.orders.front();
            }
        }
    }
    
    // Clean up empty price levels
    for (auto it = bid_book_.begin(); it != bid_book_.end();) {
        if (it->second.is_empty()) {
            it = bid_book_.erase(it);
        } else {
            ++it;
        }
    }
    
    return trades;
}

void OrderBook::add_to_book(const Order& order) {
    if (order.side == Side::BUY) {
        auto& level = bid_book_[order.price];
        if (level.orders.empty()) {
            level.price = order.price;
        }
        level.add_order(order);
        order_index_[order.order_id] = &level.orders.back();
    } else {
        auto& level = ask_book_[order.price];
        if (level.orders.empty()) {
            level.price = order.price;
        }
        level.add_order(order);
        order_index_[order.order_id] = &level.orders.back();
    }
}

bool OrderBook::cancel_order(OrderId order_id) {
    auto it = order_index_.find(order_id);
    if (it == order_index_.end()) {
        return false;  // Order not found
    }
    
    Order* order = it->second;
    Side side = order->side;
    Price price = order->price;
    
    // Mark as cancelled
    order->cancel();
    
    // Remove from book
    remove_from_book(order_id, side, price);
    
    return true;
}

void OrderBook::remove_from_book(OrderId order_id, Side side, Price price) {
    order_index_.erase(order_id);
    
    if (side == Side::BUY) {
        auto it = bid_book_.find(price);
        if (it != bid_book_.end()) {
            it->second.remove_order(order_id);
            if (it->second.is_empty()) {
                bid_book_.erase(it);
            }
        }
    } else {
        auto it = ask_book_.find(price);
        if (it != ask_book_.end()) {
            it->second.remove_order(order_id);
            if (it->second.is_empty()) {
                ask_book_.erase(it);
            }
        }
    }
}

std::pair<bool, std::vector<Trade>> OrderBook::replace_order(
    OrderId order_id, Price new_price, Quantity new_qty) {
    
    auto it = order_index_.find(order_id);
    if (it == order_index_.end()) {
        return {false, {}};
    }
    
    // Save order details before canceling
    Order old_order = *it->second;
    
    // Cancel existing order
    if (!cancel_order(order_id)) {
        return {false, {}};
    }
    
    // Create new order with same ID and client
    Order new_order(order_id, old_order.client_id, old_order.symbol,
                   old_order.side, new_price, new_qty, old_order.type);
    
    // Add new order (may generate trades)
    auto trades = add_order(new_order);
    
    return {true, trades};
}

std::optional<Order> OrderBook::get_order(OrderId order_id) const {
    auto it = order_index_.find(order_id);
    if (it != order_index_.end()) {
        return *it->second;
    }
    return std::nullopt;
}

std::pair<std::optional<Price>, std::optional<Price>> 
OrderBook::get_top_of_book() const {
    std::optional<Price> best_bid;
    std::optional<Price> best_ask;
    
    if (!bid_book_.empty()) {
        best_bid = bid_book_.begin()->first;
    }
    
    if (!ask_book_.empty()) {
        best_ask = ask_book_.begin()->first;
    }
    
    return {best_bid, best_ask};
}

Quantity OrderBook::get_bid_depth() const {
    Quantity total = 0;
    for (const auto& [price, level] : bid_book_) {
        total += level.total_quantity;
    }
    return total;
}

Quantity OrderBook::get_ask_depth() const {
    Quantity total = 0;
    for (const auto& [price, level] : ask_book_) {
        total += level.total_quantity;
    }
    return total;
}

OrderBook::BookSnapshot OrderBook::get_snapshot(size_t depth) const {
    BookSnapshot snapshot;
    
    // Get top N bid levels
    size_t count = 0;
    for (const auto& [price, level] : bid_book_) {
        if (count++ >= depth) break;
        snapshot.bids.emplace_back(price, level.total_quantity);
    }
    
    // Get top N ask levels
    count = 0;
    for (const auto& [price, level] : ask_book_) {
        if (count++ >= depth) break;
        snapshot.asks.emplace_back(price, level.total_quantity);
    }
    
    return snapshot;
}

} // namespace ome
