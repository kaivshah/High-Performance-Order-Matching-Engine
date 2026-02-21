#include "engine.h"
#include <iostream>
#include <iomanip>
#include <stdexcept>

MatchingEngine::MatchingEngine()
    : startTime_(std::chrono::steady_clock::now()) {}

void MatchingEngine::addSymbol(const std::string& symbol) {
    if (books_.count(symbol)) return;
    books_.emplace(symbol, OrderBook(symbol));
}

OrderId MatchingEngine::submitLimitOrder(const std::string& symbol,
                                         Side side, Price price, Quantity qty) {
    auto* book = getBook(symbol);
    if (!book) throw std::runtime_error("Unknown symbol: " + symbol);

    OrderId id  = nextId();
    Timestamp ts = nowNs();

    Order order(id, side, OrderType::LIMIT, price, qty, ts);
    auto trades = book->addOrder(std::move(order));

    ++totalOrders_;
    dispatchTrades(trades);
    return id;
}

OrderId MatchingEngine::submitMarketOrder(const std::string& symbol,
                                           Side side, Quantity qty) {
    auto* book = getBook(symbol);
    if (!book) throw std::runtime_error("Unknown symbol: " + symbol);

    OrderId id  = nextId();
    Timestamp ts = nowNs();

    Order order(id, side, OrderType::MARKET, 0 /*price unused*/, qty, ts);
    auto trades = book->addOrder(std::move(order));

    ++totalOrders_;
    dispatchTrades(trades);
    return id;
}

bool MatchingEngine::cancelOrder(const std::string& symbol, OrderId id) {
    auto* book = getBook(symbol);
    if (!book) return false;
    return book->cancelOrder(id);
}

OrderBook* MatchingEngine::getBook(const std::string& symbol) {
    auto it = books_.find(symbol);
    if (it == books_.end()) return nullptr;
    return &it->second;
}

void MatchingEngine::printStats() const {
    auto now = std::chrono::steady_clock::now();
    double seconds = std::chrono::duration<double>(now - startTime_).count();
    double throughput = (seconds > 0) ? totalOrders_ / seconds : 0.0;

    std::cout << "\n─── Engine Statistics ───────────────────────\n";
    std::cout << "  Total Orders Processed : " << totalOrders_  << "\n";
    std::cout << "  Total Trades Generated : " << totalTrades_  << "\n";
    std::cout << "  Elapsed Time           : " << std::fixed
              << std::setprecision(3) << seconds << "s\n";
    std::cout << "  Throughput             : " << (uint64_t)throughput
              << " orders/sec\n";
    std::cout << "─────────────────────────────────────────────\n\n";
}

// ─── Private ──────────────────────────────────────────────────────────────────

Timestamp MatchingEngine::nowNs() {
    return static_cast<Timestamp>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

void MatchingEngine::dispatchTrades(const std::vector<Trade>& trades) {
    totalTrades_ += trades.size();
    if (tradeCallback_) {
        for (const auto& t : trades) tradeCallback_(t);
    }
}