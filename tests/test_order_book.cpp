#include <gtest/gtest.h>
#include "../src/order_book.h"
#include "../src/engine.h"

// ─── Helpers ──────────────────────────────────────────────────────────────────

// Build an Order with a given timestamp for time-priority tests
static Order makeOrder(OrderId id, Side side, Price price, Quantity qty,
                       Timestamp ts = 0) {
    return Order(id, side, OrderType::LIMIT, price, qty, ts);
}

// ─── AddOrder Tests ───────────────────────────────────────────────────────────

TEST(OrderBookTest, AddBidAppearsInBook) {
    OrderBook book("TEST");
    auto trades = book.addOrder(makeOrder(1, Side::BUY, 10000, 100));

    EXPECT_TRUE(trades.empty());
    ASSERT_TRUE(book.bestBid().has_value());
    EXPECT_EQ(book.bestBid().value(), 10000);
    EXPECT_EQ(book.quantityAtPrice(Side::BUY, 10000), 100u);
}

TEST(OrderBookTest, AddAskAppearsInBook) {
    OrderBook book("TEST");
    book.addOrder(makeOrder(1, Side::SELL, 10100, 50));

    ASSERT_TRUE(book.bestAsk().has_value());
    EXPECT_EQ(book.bestAsk().value(), 10100);
    EXPECT_EQ(book.quantityAtPrice(Side::SELL, 10100), 50u);
}

TEST(OrderBookTest, BestBidIsHighestBid) {
    OrderBook book("TEST");
    book.addOrder(makeOrder(1, Side::BUY, 9900, 10));
    book.addOrder(makeOrder(2, Side::BUY, 10000, 10));
    book.addOrder(makeOrder(3, Side::BUY, 9950, 10));

    EXPECT_EQ(book.bestBid().value(), 10000);
}

TEST(OrderBookTest, BestAskIsLowestAsk) {
    OrderBook book("TEST");
    book.addOrder(makeOrder(1, Side::SELL, 10200, 10));
    book.addOrder(makeOrder(2, Side::SELL, 10100, 10));
    book.addOrder(makeOrder(3, Side::SELL, 10300, 10));

    EXPECT_EQ(book.bestAsk().value(), 10100);
}

// ─── Matching Tests ───────────────────────────────────────────────────────────

TEST(OrderBookTest, ExactMatch) {
    // Resting sell at 10000, incoming buy at 10000 → full fill
    OrderBook book("TEST");
    book.addOrder(makeOrder(1, Side::SELL, 10000, 100));
    auto trades = book.addOrder(makeOrder(2, Side::BUY, 10000, 100));

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].price, 10000);
    EXPECT_EQ(trades[0].quantity, 100u);
    EXPECT_EQ(trades[0].makerOrderId, 1u);
    EXPECT_EQ(trades[0].takerOrderId, 2u);

    // Book should be empty
    EXPECT_FALSE(book.bestBid().has_value());
    EXPECT_FALSE(book.bestAsk().has_value());
}

TEST(OrderBookTest, NoMatchWhenPricesDontCross) {
    // Buy at 9900, Sell at 10000 — no match
    OrderBook book("TEST");
    book.addOrder(makeOrder(1, Side::BUY,  9900, 100));
    auto trades = book.addOrder(makeOrder(2, Side::SELL, 10000, 100));

    EXPECT_TRUE(trades.empty());
    EXPECT_TRUE(book.bestBid().has_value());
    EXPECT_TRUE(book.bestAsk().has_value());
}

TEST(OrderBookTest, PartialFillTakerSmaller) {
    // Resting sell 100, incoming buy 60 → partial fill, 40 remain on ask side
    OrderBook book("TEST");
    book.addOrder(makeOrder(1, Side::SELL, 10000, 100));
    auto trades = book.addOrder(makeOrder(2, Side::BUY, 10000, 60));

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].quantity, 60u);
    EXPECT_EQ(book.quantityAtPrice(Side::SELL, 10000), 40u);
    EXPECT_FALSE(book.bestBid().has_value());  // Taker fully consumed
}

TEST(OrderBookTest, PartialFillMakerSmaller) {
    // Resting sell 50, incoming buy 100 → fills 50, remaining 50 rests as bid
    OrderBook book("TEST");
    book.addOrder(makeOrder(1, Side::SELL, 10000, 50));
    auto trades = book.addOrder(makeOrder(2, Side::BUY, 10000, 100));

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].quantity, 50u);
    EXPECT_FALSE(book.bestAsk().has_value());        // Ask fully consumed
    EXPECT_EQ(book.quantityAtPrice(Side::BUY, 10000), 50u); // Remainder rests
}

TEST(OrderBookTest, MultipleLevelsSweep) {
    // Incoming buy at 10300 sweeps through 3 ask price levels
    OrderBook book("TEST");
    book.addOrder(makeOrder(1, Side::SELL, 10100, 50));
    book.addOrder(makeOrder(2, Side::SELL, 10200, 50));
    book.addOrder(makeOrder(3, Side::SELL, 10300, 50));

    auto trades = book.addOrder(makeOrder(4, Side::BUY, 10300, 150));

    EXPECT_EQ(trades.size(), 3u);
    EXPECT_EQ(trades[0].price, 10100);
    EXPECT_EQ(trades[1].price, 10200);
    EXPECT_EQ(trades[2].price, 10300);
    EXPECT_FALSE(book.bestAsk().has_value());
}

// ─── Price-Time Priority Tests ────────────────────────────────────────────────

TEST(OrderBookTest, TimePriorityWithinPriceLevel) {
    // Two resting sells at same price — older one (lower ts) fills first
    OrderBook book("TEST");

    // Order 1 arrives earlier (ts=100), Order 2 later (ts=200)
    book.addOrder(makeOrder(1, Side::SELL, 10000, 30, /*ts=*/100));
    book.addOrder(makeOrder(2, Side::SELL, 10000, 30, /*ts=*/200));

    // Buy only enough to fill one order
    auto trades = book.addOrder(makeOrder(3, Side::BUY, 10000, 30));

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].makerOrderId, 1u);  // Order 1 (earlier) filled first
}

TEST(OrderBookTest, PricePriorityOverTimePriority) {
    // Order 1: sell at 10200, ts=100 (older)
    // Order 2: sell at 10100, ts=200 (newer but better price)
    // Incoming buy should match Order 2 first (price > time priority)
    OrderBook book("TEST");
    book.addOrder(makeOrder(1, Side::SELL, 10200, 50, 100));
    book.addOrder(makeOrder(2, Side::SELL, 10100, 50, 200));

    auto trades = book.addOrder(makeOrder(3, Side::BUY, 10200, 50));

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].makerOrderId, 2u);  // Lower price wins regardless of time
    EXPECT_EQ(trades[0].price, 10100);
}

// ─── Cancel Tests ─────────────────────────────────────────────────────────────

TEST(OrderBookTest, CancelRemovesOrder) {
    OrderBook book("TEST");
    book.addOrder(makeOrder(1, Side::BUY, 10000, 100));
    EXPECT_TRUE(book.cancelOrder(1));
    EXPECT_FALSE(book.bestBid().has_value());
}

TEST(OrderBookTest, CancelNonExistentReturnsFalse) {
    OrderBook book("TEST");
    EXPECT_FALSE(book.cancelOrder(999));
}

TEST(OrderBookTest, CancelOneOfManyAtSameLevel) {
    OrderBook book("TEST");
    book.addOrder(makeOrder(1, Side::BUY, 10000, 100));
    book.addOrder(makeOrder(2, Side::BUY, 10000, 50));
    EXPECT_TRUE(book.cancelOrder(1));
    EXPECT_EQ(book.quantityAtPrice(Side::BUY, 10000), 50u);
    EXPECT_EQ(book.bestBid().value(), 10000);
}

TEST(OrderBookTest, CancelFilledOrderFails) {
    // After a full fill, order should be gone from the map
    OrderBook book("TEST");
    book.addOrder(makeOrder(1, Side::SELL, 10000, 100));
    book.addOrder(makeOrder(2, Side::BUY,  10000, 100));
    EXPECT_FALSE(book.cancelOrder(1));  // Already filled
}

// ─── Market Order Tests ───────────────────────────────────────────────────────

TEST(OrderBookTest, MarketBuyFillsAtBestAsk) {
    OrderBook book("TEST");
    book.addOrder(makeOrder(1, Side::SELL, 10100, 100));
    book.addOrder(makeOrder(2, Side::SELL, 10200, 100));

    Order mkt(3, Side::BUY, OrderType::MARKET, 0, 100, 0);
    auto trades = book.addOrder(mkt);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].price, 10100);  // Filled at best ask
}

TEST(OrderBookTest, MarketOrderDoesNotRestInBook) {
    OrderBook book("TEST");
    // No asks — market buy has nothing to fill against
    Order mkt(1, Side::BUY, OrderType::MARKET, 0, 100, 0);
    auto trades = book.addOrder(mkt);

    EXPECT_TRUE(trades.empty());
    EXPECT_FALSE(book.bestBid().has_value());  // Did NOT rest
}

// ─── Engine Integration Tests ─────────────────────────────────────────────────

TEST(EngineTest, SubmitAndMatchViaEngine) {
    MatchingEngine engine;
    engine.addSymbol("AAPL");

    std::vector<Trade> recorded;
    engine.onTrade([&](const Trade& t) { recorded.push_back(t); });

    engine.submitLimitOrder("AAPL", Side::SELL, 15000, 100);
    engine.submitLimitOrder("AAPL", Side::BUY,  15000, 100);

    ASSERT_EQ(recorded.size(), 1u);
    EXPECT_EQ(recorded[0].price, 15000);
    EXPECT_EQ(recorded[0].quantity, 100u);
}

TEST(EngineTest, CancelViaEngine) {
    MatchingEngine engine;
    engine.addSymbol("AAPL");

    auto id = engine.submitLimitOrder("AAPL", Side::BUY, 15000, 100);
    EXPECT_TRUE(engine.cancelOrder("AAPL", id));
    EXPECT_FALSE(engine.cancelOrder("AAPL", id));  // Double cancel → false
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}