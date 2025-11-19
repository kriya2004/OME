#pragma once

#include "types.h"
#include <string>

namespace ome {

// Core Order structure - kept compact for cache efficiency
// No virtual functions to maintain POD-like characteristics
struct Order {
    OrderId order_id;
    ClientId client_id;
    std::string symbol;
    Side side;
    Price price;            // Integer price (e.g., price in cents)
    Quantity quantity;      // Current quantity (reduces on partial fills)
    Quantity original_qty;  // Original quantity for tracking
    Timestamp timestamp;
    OrderType type;
    OrderStatus status;

    Order() = default;

    Order(OrderId id, ClientId cid, const std::string& sym, Side s, 
          Price p, Quantity qty, OrderType t = OrderType::LIMIT)
        : order_id(id)
        , client_id(cid)
        , symbol(sym)
        , side(s)
        , price(p)
        , quantity(qty)
        , original_qty(qty)
        , timestamp(get_timestamp())
        , type(t)
        , status(OrderStatus::ACTIVE)
    {}

    // Check if order is active (not filled or cancelled)
    bool is_active() const {
        return status == OrderStatus::ACTIVE || status == OrderStatus::PARTIAL;
    }

    // Check if order can match (active and has quantity)
    bool can_match() const {
        return is_active() && quantity > 0;
    }

    // Fill the order by a given quantity
    void fill(Quantity fill_qty) {
        if (fill_qty >= quantity) {
            quantity = 0;
            status = OrderStatus::FILLED;
        } else {
            quantity -= fill_qty;
            status = OrderStatus::PARTIAL;
        }
    }

    // Cancel the order
    void cancel() {
        status = OrderStatus::CANCELLED;
        quantity = 0;
    }
};

// Trade result from matching
struct Trade {
    OrderId maker_order_id;
    OrderId taker_order_id;
    Price price;
    Quantity quantity;
    Timestamp timestamp;
    std::string symbol;

    Trade(OrderId maker, OrderId taker, Price p, Quantity qty, const std::string& sym)
        : maker_order_id(maker)
        , taker_order_id(taker)
        , price(p)
        , quantity(qty)
        , timestamp(get_timestamp())
        , symbol(sym)
    {}
};

} // namespace ome
