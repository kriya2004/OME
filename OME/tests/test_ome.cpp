#include <gtest/gtest.h>
#include "order_book.h"
#include "matching_engine.h"

using namespace ome;

class OrderBookTest : public ::testing::Test {
protected:
    void SetUp() override {
        book = std::make_unique<OrderBook>("TEST");
    }
    
    std::unique_ptr<OrderBook> book;
};

TEST_F(OrderBookTest, SimpleMatch) {
    // Add sell order
    Order sell_order(1, 1, "TEST", Side::SELL, 10000, 100);
    auto trades = book->add_order(sell_order);
    EXPECT_EQ(trades.size(), 0);  // No match yet
    
    // Add buy order that matches
    Order buy_order(2, 1, "TEST", Side::BUY, 10000, 50);
    trades = book->add_order(buy_order);
    
    EXPECT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 50);
    EXPECT_EQ(trades[0].price, 10000);
}

TEST_F(OrderBookTest, PartialFill) {
    // Add large sell order
    Order sell_order(1, 1, "TEST", Side::SELL, 10000, 100);
    book->add_order(sell_order);
    
    // Add smaller buy order
    Order buy_order(2, 1, "TEST", Side::BUY, 10000, 30);
    auto trades = book->add_order(buy_order);
    
    EXPECT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 30);
    
    // Check remaining quantity in book
    auto [best_bid, best_ask] = book->get_top_of_book();
    EXPECT_TRUE(best_ask.has_value());
    EXPECT_EQ(best_ask.value(), 10000);
}

TEST_F(OrderBookTest, PriceTimePriority) {
    // Add two sell orders at same price
    Order sell1(1, 1, "TEST", Side::SELL, 10000, 50);
    Order sell2(2, 1, "TEST", Side::SELL, 10000, 50);
    
    book->add_order(sell1);
    book->add_order(sell2);
    
    // Buy order should match first order first
    Order buy(3, 1, "TEST", Side::BUY, 10000, 75);
    auto trades = book->add_order(buy);
    
    EXPECT_EQ(trades.size(), 2);
    EXPECT_EQ(trades[0].maker_order_id, 1);  // First order matched first
    EXPECT_EQ(trades[0].quantity, 50);
    EXPECT_EQ(trades[1].maker_order_id, 2);  // Second order matched
    EXPECT_EQ(trades[1].quantity, 25);
}

TEST_F(OrderBookTest, MarketOrder) {
    // Add sell orders at different prices
    book->add_order(Order(1, 1, "TEST", Side::SELL, 10000, 50));
    book->add_order(Order(2, 1, "TEST", Side::SELL, 10100, 50));
    book->add_order(Order(3, 1, "TEST", Side::SELL, 10200, 50));
    
    // Market buy order should match best prices
    Order market_buy(4, 1, "TEST", Side::BUY, 0, 120, OrderType::MARKET);
    auto trades = book->add_order(market_buy);
    
    EXPECT_EQ(trades.size(), 3);
    EXPECT_EQ(trades[0].price, 10000);  // Best price first
    EXPECT_EQ(trades[1].price, 10100);
    EXPECT_EQ(trades[2].price, 10200);
}

TEST_F(OrderBookTest, CancelOrder) {
    Order order(1, 1, "TEST", Side::BUY, 10000, 100);
    book->add_order(order);
    
    EXPECT_TRUE(book->cancel_order(1));
    EXPECT_FALSE(book->cancel_order(1));  // Already cancelled
    
    auto [best_bid, best_ask] = book->get_top_of_book();
    EXPECT_FALSE(best_bid.has_value());  // Book should be empty
}

TEST_F(OrderBookTest, ReplaceOrder) {
    Order order(1, 1, "TEST", Side::BUY, 10000, 100);
    book->add_order(order);
    
    auto [success, trades] = book->replace_order(1, 10100, 150);
    EXPECT_TRUE(success);
    
    auto [best_bid, best_ask] = book->get_top_of_book();
    EXPECT_TRUE(best_bid.has_value());
    EXPECT_EQ(best_bid.value(), 10100);
}

TEST_F(OrderBookTest, EmptyBookTopOfBook) {
    auto [best_bid, best_ask] = book->get_top_of_book();
    EXPECT_FALSE(best_bid.has_value());
    EXPECT_FALSE(best_ask.has_value());
}

TEST_F(OrderBookTest, MultipleMatches) {
    // Build sell side
    book->add_order(Order(1, 1, "TEST", Side::SELL, 10000, 30));
    book->add_order(Order(2, 1, "TEST", Side::SELL, 10000, 20));
    book->add_order(Order(3, 1, "TEST", Side::SELL, 10100, 50));
    
    // Large buy order crosses multiple levels
    Order buy(4, 1, "TEST", Side::BUY, 10100, 90);
    auto trades = book->add_order(buy);
    
    EXPECT_EQ(trades.size(), 3);
    EXPECT_EQ(trades[0].quantity, 30);
    EXPECT_EQ(trades[1].quantity, 20);
    EXPECT_EQ(trades[2].quantity, 40);  // Partial fill of third order
}

class MatchingEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = std::make_unique<MatchingEngine>("test.wal");
    }
    
    void TearDown() override {
        // Clean up WAL file
        std::remove("test.wal");
    }
    
    std::unique_ptr<MatchingEngine> engine;
};

TEST_F(MatchingEngineTest, SubmitAndMatch) {
    auto result1 = engine->submit_order(1, "TEST", Side::SELL, 10000, 100);
    EXPECT_TRUE(result1.success);
    EXPECT_EQ(result1.trades.size(), 0);
    
    auto result2 = engine->submit_order(2, "TEST", Side::BUY, 10000, 50);
    EXPECT_TRUE(result2.success);
    EXPECT_EQ(result2.trades.size(), 1);
}

TEST_F(MatchingEngineTest, Statistics) {
    engine->submit_order(1, "TEST", Side::SELL, 10000, 100);
    engine->submit_order(2, "TEST", Side::BUY, 10000, 50);
    
    auto stats = engine->get_statistics();
    EXPECT_EQ(stats.total_orders, 2);
    EXPECT_EQ(stats.total_trades, 1);
    EXPECT_EQ(stats.total_volume, 50);
}

TEST_F(MatchingEngineTest, CancelOrder) {
    auto result = engine->submit_order(1, "TEST", Side::BUY, 10000, 100);
    OrderId order_id = result.order_id;
    
    auto cancel_result = engine->cancel_order(order_id);
    EXPECT_TRUE(cancel_result.success);
    
    // Try to cancel again
    auto cancel_result2 = engine->cancel_order(order_id);
    EXPECT_FALSE(cancel_result2.success);
}

TEST_F(MatchingEngineTest, InvalidOrders) {
    // Zero quantity
    auto result1 = engine->submit_order(1, "TEST", Side::BUY, 10000, 0);
    EXPECT_FALSE(result1.success);
    
    // Zero price for limit order
    auto result2 = engine->submit_order(1, "TEST", Side::BUY, 0, 100, OrderType::LIMIT);
    EXPECT_FALSE(result2.success);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
