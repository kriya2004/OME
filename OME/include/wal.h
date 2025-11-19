#pragma once

#include "order.h"
#include "types.h"
#include <string>
#include <fstream>
#include <vector>
#include <memory>

namespace ome {

// WAL event types
enum class WALEventType {
    NEW_ORDER,
    CANCEL_ORDER,
    REPLACE_ORDER,
    TRADE
};

// WAL entry structure
struct WALEntry {
    WALEventType event_type;
    Timestamp timestamp;
    
    // Event-specific data (stored as JSON)
    std::string data;
    
    WALEntry(WALEventType type, const std::string& event_data)
        : event_type(type)
        , timestamp(get_timestamp())
        , data(event_data)
    {}
};

// Write-Ahead Log for persistence and replay
class WAL {
public:
    explicit WAL(const std::string& filepath);
    ~WAL();

    // Logging functions
    void log_new_order(const Order& order);
    void log_cancel(OrderId order_id);
    void log_replace(OrderId order_id, Price new_price, Quantity new_qty);
    void log_trade(const Trade& trade);

    // Flush to disk
    void flush();

    // Replay support
    struct ReplayEvent {
        WALEventType type;
        Timestamp timestamp;
        std::string json_data;
    };
    
    static std::vector<ReplayEvent> read_log(const std::string& filepath);

private:
    std::string filepath_;
    std::ofstream log_file_;
    size_t entries_written_;
    
    void write_entry(const WALEntry& entry);
    std::string order_to_json(const Order& order) const;
    std::string trade_to_json(const Trade& trade) const;
};

} // namespace ome
