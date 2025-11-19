#include "matching_engine.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <algorithm>

using namespace ome;

class CLI {
public:
    CLI() : engine_("ome.wal"), running_(true), client_id_(1) {}
    
    void run() {
        print_welcome();
        
        while (running_) {
            std::cout << "\n> ";
            std::string line;
            if (!std::getline(std::cin, line)) {
                break;
            }
            
            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t"));
            line.erase(line.find_last_not_of(" \t") + 1);
            
            if (line.empty()) continue;
            
            process_command(line);
        }
    }
    
private:
    MatchingEngine engine_;
    bool running_;
    ClientId client_id_;
    std::string current_symbol_ = "AAPL";
    
    void print_welcome() {
        std::cout << "=====================================\n";
        std::cout << "   Order Matching Engine (OME)\n";
        std::cout << "=====================================\n";
        std::cout << "Commands:\n";
        std::cout << "  NEW <side> <qty> @ <price>     - Submit limit order\n";
        std::cout << "  MARKET <side> <qty>            - Submit market order\n";
        std::cout << "  CANCEL <order_id>              - Cancel order\n";
        std::cout << "  REPLACE <order_id> <qty> @ <price> - Replace order\n";
        std::cout << "  TOB                            - Show top of book\n";
        std::cout << "  DUMP [depth]                   - Show book snapshot\n";
        std::cout << "  STATS                          - Show statistics\n";
        std::cout << "  SYMBOL <symbol>                - Set current symbol (default: AAPL)\n";
        std::cout << "  CLIENT <id>                    - Set client ID\n";
        std::cout << "  HELP                           - Show this help\n";
        std::cout << "  EXIT                           - Exit program\n";
        std::cout << "\nCurrent symbol: " << current_symbol_ << "\n";
    }
    
    void process_command(const std::string& line) {
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;
        
        // Convert to uppercase
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
        
        if (cmd == "NEW") {
            handle_new_order(iss, false);
        } else if (cmd == "MARKET") {
            handle_new_order(iss, true);
        } else if (cmd == "CANCEL") {
            handle_cancel(iss);
        } else if (cmd == "REPLACE") {
            handle_replace(iss);
        } else if (cmd == "TOB") {
            handle_tob();
        } else if (cmd == "DUMP") {
            handle_dump(iss);
        } else if (cmd == "STATS") {
            handle_stats();
        } else if (cmd == "SYMBOL") {
            handle_symbol(iss);
        } else if (cmd == "CLIENT") {
            handle_client(iss);
        } else if (cmd == "HELP") {
            print_welcome();
        } else if (cmd == "EXIT" || cmd == "QUIT") {
            running_ = false;
        } else {
            std::cout << "Unknown command: " << cmd << "\n";
        }
    }
    
    void handle_new_order(std::istringstream& iss, bool is_market) {
        std::string side_str;
        Quantity qty;
        Price price = 0;
        
        iss >> side_str >> qty;
        
        if (!is_market) {
            std::string at;
            iss >> at >> price;
            if (at != "@") {
                std::cout << "Invalid format. Use: NEW <side> <qty> @ <price>\n";
                return;
            }
        }
        
        // Parse side
        std::transform(side_str.begin(), side_str.end(), side_str.begin(), ::toupper);
        Side side;
        if (side_str == "BUY" || side_str == "B") {
            side = Side::BUY;
        } else if (side_str == "SELL" || side_str == "S") {
            side = Side::SELL;
        } else {
            std::cout << "Invalid side: " << side_str << ". Use BUY or SELL.\n";
            return;
        }
        
        // Submit order
        OrderType type = is_market ? OrderType::MARKET : OrderType::LIMIT;
        auto result = engine_.submit_order(client_id_, current_symbol_, 
                                          side, price, qty, type);
        
        if (result.success) {
            std::cout << "✓ Order " << result.order_id << " submitted\n";
            
            if (!result.trades.empty()) {
                std::cout << "  Trades executed:\n";
                for (const auto& trade : result.trades) {
                    std::cout << "    " << trade.quantity << " @ " 
                             << format_price(trade.price) << "\n";
                }
            }
        } else {
            std::cout << "✗ " << result.message << "\n";
        }
    }
    
    void handle_cancel(std::istringstream& iss) {
        OrderId order_id;
        iss >> order_id;
        
        auto result = engine_.cancel_order(order_id);
        
        if (result.success) {
            std::cout << "✓ Order " << order_id << " cancelled\n";
        } else {
            std::cout << "✗ " << result.message << "\n";
        }
    }
    
    void handle_replace(std::istringstream& iss) {
        OrderId order_id;
        Quantity qty;
        Price price;
        std::string at;
        
        iss >> order_id >> qty >> at >> price;
        
        if (at != "@") {
            std::cout << "Invalid format. Use: REPLACE <order_id> <qty> @ <price>\n";
            return;
        }
        
        auto result = engine_.replace_order(order_id, price, qty);
        
        if (result.success) {
            std::cout << "✓ Order " << order_id << " replaced\n";
            
            if (!result.trades.empty()) {
                std::cout << "  Trades executed:\n";
                for (const auto& trade : result.trades) {
                    std::cout << "    " << trade.quantity << " @ " 
                             << format_price(trade.price) << "\n";
                }
            }
        } else {
            std::cout << "✗ " << result.message << "\n";
        }
    }
    
    void handle_tob() {
        auto [bid, ask] = engine_.get_top_of_book(current_symbol_);
        
        std::cout << "Top of Book [" << current_symbol_ << "]:\n";
        if (bid.has_value()) {
            std::cout << "  Best Bid: " << format_price(bid.value()) << "\n";
        } else {
            std::cout << "  Best Bid: ---\n";
        }
        
        if (ask.has_value()) {
            std::cout << "  Best Ask: " << format_price(ask.value()) << "\n";
        } else {
            std::cout << "  Best Ask: ---\n";
        }
        
        if (bid.has_value() && ask.has_value()) {
            std::cout << "  Spread:   " << format_price(ask.value() - bid.value()) << "\n";
        }
    }
    
    void handle_dump(std::istringstream& iss) {
        size_t depth = 10;
        iss >> depth;
        
        auto snapshot = engine_.get_book_snapshot(current_symbol_, depth);
        
        std::cout << "\nOrder Book [" << current_symbol_ << "]:\n";
        std::cout << std::string(40, '=') << "\n";
        std::cout << std::setw(15) << "ASK PRICE" << " | " 
                 << std::setw(10) << "QUANTITY" << "\n";
        std::cout << std::string(40, '-') << "\n";
        
        // Print asks in reverse (highest first)
        for (auto it = snapshot.asks.rbegin(); it != snapshot.asks.rend(); ++it) {
            std::cout << std::setw(15) << format_price(it->first) << " | " 
                     << std::setw(10) << it->second << "\n";
        }
        
        std::cout << std::string(40, '=') << "\n";
        
        // Print bids
        for (const auto& [price, qty] : snapshot.bids) {
            std::cout << std::setw(15) << format_price(price) << " | " 
                     << std::setw(10) << qty << "\n";
        }
        std::cout << std::string(40, '-') << "\n";
        std::cout << std::setw(15) << "BID PRICE" << " | " 
                 << std::setw(10) << "QUANTITY" << "\n";
    }
    
    void handle_stats() {
        auto stats = engine_.get_statistics();
        
        std::cout << "\nEngine Statistics:\n";
        std::cout << "  Total Orders:  " << stats.total_orders << "\n";
        std::cout << "  Total Trades:  " << stats.total_trades << "\n";
        std::cout << "  Total Volume:  " << stats.total_volume << "\n";
        std::cout << "  Active Orders: " << stats.active_orders << "\n";
    }
    
    void handle_symbol(std::istringstream& iss) {
        std::string symbol;
        iss >> symbol;
        
        if (!symbol.empty()) {
            current_symbol_ = symbol;
            std::cout << "Current symbol set to: " << current_symbol_ << "\n";
        } else {
            std::cout << "Current symbol: " << current_symbol_ << "\n";
        }
    }
    
    void handle_client(std::istringstream& iss) {
        iss >> client_id_;
        std::cout << "Client ID set to: " << client_id_ << "\n";
    }
    
    std::string format_price(Price price) const {
        // Format integer price as decimal (assuming 2 decimal places)
        double display_price = price / 100.0;
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << display_price;
        return oss.str();
    }
};

int main(int argc, char* argv[]) {
    try {
        CLI cli;
        cli.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
