#pragma once

#include <cstdint>
#include <string>
#include <chrono>

namespace ome {

// Type aliases for clarity and easy refactoring
using OrderId = uint64_t;
using ClientId = uint32_t;
using Price = int64_t;      // Integer price representation (e.g., paise/cents)
using Quantity = uint32_t;
using Timestamp = uint64_t;

// Order side: Buy or Sell
enum class Side : uint8_t {
    BUY,
    SELL
};

// Order type: Limit or Market
enum class OrderType : uint8_t {
    LIMIT,
    MARKET
};

// Order status
enum class OrderStatus : uint8_t {
    ACTIVE,     // Order is active in the book
    PARTIAL,    // Order is partially filled
    FILLED,     // Order is completely filled
    CANCELLED   // Order has been cancelled
};

// Convert enums to strings for logging/display
inline const char* to_string(Side side) {
    return side == Side::BUY ? "BUY" : "SELL";
}

inline const char* to_string(OrderType type) {
    return type == OrderType::LIMIT ? "LIMIT" : "MARKET";
}

inline const char* to_string(OrderStatus status) {
    switch (status) {
        case OrderStatus::ACTIVE: return "ACTIVE";
        case OrderStatus::PARTIAL: return "PARTIAL";
        case OrderStatus::FILLED: return "FILLED";
        case OrderStatus::CANCELLED: return "CANCELLED";
        default: return "UNKNOWN";
    }
}

// Get current monotonic timestamp in nanoseconds
inline Timestamp get_timestamp() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

} // namespace ome
