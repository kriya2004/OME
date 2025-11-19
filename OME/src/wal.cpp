#include "wal.h"
#include <sstream>
#include <iomanip>
#include <iostream>

namespace ome {

WAL::WAL(const std::string& filepath) 
    : filepath_(filepath)
    , entries_written_(0) {
    
    log_file_.open(filepath_, std::ios::out | std::ios::app);
    if (!log_file_.is_open()) {
        throw std::runtime_error("Failed to open WAL file: " + filepath_);
    }
}

WAL::~WAL() {
    if (log_file_.is_open()) {
        flush();
        log_file_.close();
    }
}

std::string WAL::order_to_json(const Order& order) const {
    std::ostringstream oss;
    oss << "{"
        << "\"order_id\":" << order.order_id << ","
        << "\"client_id\":" << order.client_id << ","
        << "\"symbol\":\"" << order.symbol << "\","
        << "\"side\":\"" << to_string(order.side) << "\","
        << "\"price\":" << order.price << ","
        << "\"quantity\":" << order.quantity << ","
        << "\"original_qty\":" << order.original_qty << ","
        << "\"timestamp\":" << order.timestamp << ","
        << "\"type\":\"" << to_string(order.type) << "\","
        << "\"status\":\"" << to_string(order.status) << "\""
        << "}";
    return oss.str();
}

std::string WAL::trade_to_json(const Trade& trade) const {
    std::ostringstream oss;
    oss << "{"
        << "\"maker_order_id\":" << trade.maker_order_id << ","
        << "\"taker_order_id\":" << trade.taker_order_id << ","
        << "\"price\":" << trade.price << ","
        << "\"quantity\":" << trade.quantity << ","
        << "\"timestamp\":" << trade.timestamp << ","
        << "\"symbol\":\"" << trade.symbol << "\""
        << "}";
    return oss.str();
}

void WAL::write_entry(const WALEntry& entry) {
    const char* event_type_str = "";
    switch (entry.event_type) {
        case WALEventType::NEW_ORDER: event_type_str = "NEW_ORDER"; break;
        case WALEventType::CANCEL_ORDER: event_type_str = "CANCEL_ORDER"; break;
        case WALEventType::REPLACE_ORDER: event_type_str = "REPLACE_ORDER"; break;
        case WALEventType::TRADE: event_type_str = "TRADE"; break;
    }
    
    log_file_ << "{"
              << "\"event_type\":\"" << event_type_str << "\","
              << "\"timestamp\":" << entry.timestamp << ","
              << "\"data\":" << entry.data
              << "}\n";
    
    entries_written_++;
    
    // Flush every 100 entries for safety
    if (entries_written_ % 100 == 0) {
        flush();
    }
}

void WAL::log_new_order(const Order& order) {
    WALEntry entry(WALEventType::NEW_ORDER, order_to_json(order));
    write_entry(entry);
}

void WAL::log_cancel(OrderId order_id) {
    std::ostringstream oss;
    oss << "{\"order_id\":" << order_id << "}";
    WALEntry entry(WALEventType::CANCEL_ORDER, oss.str());
    write_entry(entry);
}

void WAL::log_replace(OrderId order_id, Price new_price, Quantity new_qty) {
    std::ostringstream oss;
    oss << "{"
        << "\"order_id\":" << order_id << ","
        << "\"new_price\":" << new_price << ","
        << "\"new_quantity\":" << new_qty
        << "}";
    WALEntry entry(WALEventType::REPLACE_ORDER, oss.str());
    write_entry(entry);
}

void WAL::log_trade(const Trade& trade) {
    WALEntry entry(WALEventType::TRADE, trade_to_json(trade));
    write_entry(entry);
}

void WAL::flush() {
    if (log_file_.is_open()) {
        log_file_.flush();
    }
}

std::vector<WAL::ReplayEvent> WAL::read_log(const std::string& filepath) {
    std::vector<ReplayEvent> events;
    std::ifstream file(filepath);
    
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open WAL file for reading: " + filepath);
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        // Simple JSON parsing (for production use a proper JSON library)
        // This is a minimal implementation for demonstration
        ReplayEvent event;
        event.json_data = line;
        
        // Extract event type
        size_t type_pos = line.find("\"event_type\":\"");
        if (type_pos != std::string::npos) {
            type_pos += 14;  // Length of "\"event_type\":\""
            size_t type_end = line.find("\"", type_pos);
            std::string type_str = line.substr(type_pos, type_end - type_pos);
            
            if (type_str == "NEW_ORDER") event.type = WALEventType::NEW_ORDER;
            else if (type_str == "CANCEL_ORDER") event.type = WALEventType::CANCEL_ORDER;
            else if (type_str == "REPLACE_ORDER") event.type = WALEventType::REPLACE_ORDER;
            else if (type_str == "TRADE") event.type = WALEventType::TRADE;
        }
        
        // Extract timestamp
        size_t ts_pos = line.find("\"timestamp\":");
        if (ts_pos != std::string::npos) {
            ts_pos += 12;  // Length of "\"timestamp\":"
            size_t ts_end = line.find_first_of(",}", ts_pos);
            event.timestamp = std::stoull(line.substr(ts_pos, ts_end - ts_pos));
        }
        
        events.push_back(event);
    }
    
    return events;
}

} // namespace ome
