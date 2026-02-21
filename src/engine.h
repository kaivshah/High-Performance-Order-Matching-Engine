#pragma once

#include "order_book.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <atomic>
#include <chrono>
#include <functional>

// MatchingEngine: the top-level orchestrator.
//
// Responsibilities:
//   - Assign unique, monotonically-increasing order IDs
//   - Assign nanosecond timestamps for time-priority ordering
//   - Route orders to the correct OrderBook by symbol
//   - Expose callbacks for trade and order event reporting
//   - Single-threaded: deterministic, no locks needed in matching path

class MatchingEngine {
public:
    // Called on every fill. Use for logging, risk checks, PnL tracking, etc.
    using TradeCallback = std::function<void(const Trade&)>;

    MatchingEngine();

    // Register a new trading symbol. Must be called before submitting orders.
    void addSymbol(const std::string& symbol);

    // Submit a new limit order. Returns the assigned OrderId.
    // Generates trades immediately if it crosses the book.
    OrderId submitLimitOrder(const std::string& symbol, Side side,
                             Price price, Quantity qty);

    // Submit a new market order. Fills immediately at best available price.
    // Returns assigned OrderId. Unmatched quantity is discarded.
    OrderId submitMarketOrder(const std::string& symbol, Side side,
                              Quantity qty);

    // Cancel a resting order. Returns true if found and removed.
    bool cancelOrder(const std::string& symbol, OrderId id);

    // Register a callback to be invoked on every trade execution.
    void onTrade(TradeCallback cb) { tradeCallback_ = std::move(cb); }

    // Access the raw order book (e.g. for testing or display)
    OrderBook* getBook(const std::string& symbol);

    // Print statistics: total orders, total trades, throughput
    void printStats() const;

private:
    std::unordered_map<std::string, OrderBook> books_;

    std::atomic<OrderId>   nextOrderId_{1};
    TradeCallback          tradeCallback_;

    // Stats
    uint64_t totalOrders_{0};
    uint64_t totalTrades_{0};
    std::chrono::steady_clock::time_point startTime_;

    OrderId  nextId()  { return nextOrderId_.fetch_add(1, std::memory_order_relaxed); }
    Timestamp nowNs();

    void dispatchTrades(const std::vector<Trade>& trades);
};