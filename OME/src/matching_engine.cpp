#include "matching_engine.h"

namespace ome {

MatchingEngine::MatchingEngine(const std::string& wal_path)
    : next_order_id_(1)
    , wal_(wal_path)
    , total_orders_(0)
    , total_trades_(0)
    , total_volume_(0)
{}

OrderId MatchingEngine::generate_order_id() {
    return next_order_id_.fetch_add(1, std::memory_order_relaxed);
}

OrderBook* MatchingEngine::get_or_create_book(const std::string& symbol) {
    auto it = order_books_.find(symbol);
    if (it != order_books_.end()) {
        return it->second.get();
    }
    
    // Create new order book for this symbol
    auto book = std::make_unique<OrderBook>(symbol);
    auto* book_ptr = book.get();
    order_books_[symbol] = std::move(book);
    return book_ptr;
}

MatchingEngine::OrderResult MatchingEngine::submit_order(
    ClientId client_id, const std::string& symbol,
    Side side, Price price, Quantity quantity, OrderType type) {
    
    OrderResult result;
    result.success = false;
    
    // Validate input
    if (quantity == 0) {
        result.message = "Invalid quantity: must be > 0";
        return result;
    }
    
    if (type == OrderType::LIMIT && price <= 0) {
        result.message = "Invalid price: must be > 0 for limit orders";
        return result;
    }
    
    // Generate order ID
    OrderId order_id = generate_order_id();
    
    // Create order
    Order order(order_id, client_id, symbol, side, price, quantity, type);
    
    // Log to WAL
    wal_.log_new_order(order);
    
    // Get or create order book
    OrderBook* book = get_or_create_book(symbol);
    
    // Add order and match
    auto trades = book->add_order(order);
    
    // Log trades
    for (const auto& trade : trades) {
        wal_.log_trade(trade);
        total_trades_.fetch_add(1, std::memory_order_relaxed);
        total_volume_.fetch_add(trade.quantity, std::memory_order_relaxed);
    }
    
    // Track order if it remains in book
    if (order.quantity > 0 && type == OrderType::LIMIT) {
        order_tracker_[order_id] = {symbol, order_id};
    }
    
    total_orders_.fetch_add(1, std::memory_order_relaxed);
    
    result.success = true;
    result.order_id = order_id;
    result.trades = std::move(trades);
    result.message = "Order submitted successfully";
    
    return result;
}

MatchingEngine::OrderResult MatchingEngine::cancel_order(OrderId order_id) {
    OrderResult result;
    result.success = false;
    result.order_id = order_id;
    
    // Find order
    auto it = order_tracker_.find(order_id);
    if (it == order_tracker_.end()) {
        result.message = "Order not found";
        return result;
    }
    
    const std::string& symbol = it->second.symbol;
    
    // Get order book
    auto book_it = order_books_.find(symbol);
    if (book_it == order_books_.end()) {
        result.message = "Order book not found";
        return result;
    }
    
    // Cancel in book
    bool cancelled = book_it->second->cancel_order(order_id);
    
    if (cancelled) {
        // Log to WAL
        wal_.log_cancel(order_id);
        
        // Remove from tracker
        order_tracker_.erase(it);
        
        result.success = true;
        result.message = "Order cancelled successfully";
    } else {
        result.message = "Failed to cancel order (may already be filled)";
    }
    
    return result;
}

MatchingEngine::OrderResult MatchingEngine::replace_order(
    OrderId order_id, Price new_price, Quantity new_qty) {
    
    OrderResult result;
    result.success = false;
    result.order_id = order_id;
    
    // Validate
    if (new_qty == 0) {
        result.message = "Invalid quantity: must be > 0";
        return result;
    }
    
    if (new_price <= 0) {
        result.message = "Invalid price: must be > 0";
        return result;
    }
    
    // Find order
    auto it = order_tracker_.find(order_id);
    if (it == order_tracker_.end()) {
        result.message = "Order not found";
        return result;
    }
    
    const std::string& symbol = it->second.symbol;
    
    // Get order book
    auto book_it = order_books_.find(symbol);
    if (book_it == order_books_.end()) {
        result.message = "Order book not found";
        return result;
    }
    
    // Replace in book
    auto [success, trades] = book_it->second->replace_order(order_id, new_price, new_qty);
    
    if (success) {
        // Log to WAL
        wal_.log_replace(order_id, new_price, new_qty);
        
        // Log any trades
        for (const auto& trade : trades) {
            wal_.log_trade(trade);
            total_trades_.fetch_add(1, std::memory_order_relaxed);
            total_volume_.fetch_add(trade.quantity, std::memory_order_relaxed);
        }
        
        result.success = true;
        result.trades = std::move(trades);
        result.message = "Order replaced successfully";
    } else {
        result.message = "Failed to replace order";
    }
    
    return result;
}

std::optional<Order> MatchingEngine::get_order(OrderId order_id) const {
    auto it = order_tracker_.find(order_id);
    if (it == order_tracker_.end()) {
        return std::nullopt;
    }
    
    const std::string& symbol = it->second.symbol;
    auto book_it = order_books_.find(symbol);
    if (book_it == order_books_.end()) {
        return std::nullopt;
    }
    
    return book_it->second->get_order(order_id);
}

std::pair<std::optional<Price>, std::optional<Price>> 
MatchingEngine::get_top_of_book(const std::string& symbol) const {
    auto it = order_books_.find(symbol);
    if (it == order_books_.end()) {
        return {std::nullopt, std::nullopt};
    }
    
    return it->second->get_top_of_book();
}

OrderBook::BookSnapshot MatchingEngine::get_book_snapshot(
    const std::string& symbol, size_t depth) const {
    
    auto it = order_books_.find(symbol);
    if (it == order_books_.end()) {
        return {};
    }
    
    return it->second->get_snapshot(depth);
}

MatchingEngine::Statistics MatchingEngine::get_statistics() const {
    Statistics stats;
    stats.total_orders = total_orders_.load(std::memory_order_relaxed);
    stats.total_trades = total_trades_.load(std::memory_order_relaxed);
    stats.total_volume = total_volume_.load(std::memory_order_relaxed);
    stats.active_orders = order_tracker_.size();
    return stats;
}

} // namespace ome
