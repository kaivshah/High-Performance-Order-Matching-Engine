#pragma once

#include "order.h"
#include <map>
#include <list>
#include <unordered_map>
#include <vector>
#include <functional>
#include <optional>

// Per price-level: orders stored in a std::list to guarantee iterator stability.
// This is critical — std::deque invalidates iterators on push/pop, which would
// corrupt our order_map lookup table.
using PriceLevel = std::list<Order>;

// Bids: sorted descending (best bid = highest price = begin())
using BidLevels = std::map<Price, PriceLevel, std::greater<Price>>;

// Asks: sorted ascending (best ask = lowest price = begin())
using AskLevels = std::map<Price, PriceLevel, std::less<Price>>;

// Stable iterator into a PriceLevel list — safe across insertions/deletions
// at other positions.
struct OrderLocation {
    Price             price;
    Side              side;
    PriceLevel::iterator iter;  // Points directly to Order in its list
};

class OrderBook {
public:
    explicit OrderBook(std::string symbol);

    // Returns list of trades generated (may be empty for resting limit orders).
    std::vector<Trade> addOrder(Order order);

    // Returns true if order was found and cancelled.
    bool cancelOrder(OrderId id);

    // Accessors for testing and display
    const BidLevels& bids() const { return bids_; }
    const AskLevels& asks() const { return asks_; }

    std::optional<Price> bestBid() const;
    std::optional<Price> bestAsk() const;

    // Returns total resting quantity at a given price
    Quantity quantityAtPrice(Side side, Price price) const;

    void printBook(int depth = 5) const;

    const std::string& symbol() const { return symbol_; }

private:
    std::string symbol_;

    BidLevels bids_;
    AskLevels asks_;

    // OrderId -> location in bids_/asks_ for O(1) cancel
    std::unordered_map<OrderId, OrderLocation> orderMap_;

    // Internal match logic: tries to fill `incoming` against resting orders.
    // Modifies resting book in place. Returns generated trades.
    std::vector<Trade> matchOrder(Order& incoming);

    // Remove an empty price level from the book
    void pruneLevel(Side side, Price price);
};