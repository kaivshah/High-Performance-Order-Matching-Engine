#include "order_book.h"
#include <iostream>
#include <iomanip>
#include <stdexcept>

OrderBook::OrderBook(std::string symbol) : symbol_(std::move(symbol)) {}

// ─── Public: Add Order ────────────────────────────────────────────────────────

std::vector<Trade> OrderBook::addOrder(Order order) {
    std::vector<Trade> trades;

    if (order.type == OrderType::MARKET) {
        // Market orders are purely aggressive — match immediately, no resting.
        trades = matchOrder(order);
        if (order.quantity > 0) {
            // Unexecuted market order quantity is cancelled (no resting allowed).
            // In real exchanges this would emit a cancel report.
        }
        return trades;
    }

    // LIMIT order: first try to match aggressively, then rest any remainder.
    trades = matchOrder(order);

    if (order.quantity > 0) {
        // Rest the remaining quantity in the book.
        order.status = (order.quantity < order.originalQty)
                           ? OrderStatus::PARTIALLY_FILLED
                           : OrderStatus::NEW;

        if (order.side == Side::BUY) {
            auto& level = bids_[order.price];
            level.push_back(order);
            auto it = std::prev(level.end());
            orderMap_[order.id] = {order.price, Side::BUY, it};
        } else {
            auto& level = asks_[order.price];
            level.push_back(order);
            auto it = std::prev(level.end());
            orderMap_[order.id] = {order.price, Side::SELL, it};
        }
    }

    return trades;
}

// ─── Public: Cancel Order ─────────────────────────────────────────────────────

bool OrderBook::cancelOrder(OrderId id) {
    auto mapIt = orderMap_.find(id);
    if (mapIt == orderMap_.end()) return false;

    OrderLocation& loc = mapIt->second;

    // Erase from the price level list — O(1) because we have the iterator.
    if (loc.side == Side::BUY) {
        bids_[loc.price].erase(loc.iter);
        pruneLevel(Side::BUY, loc.price);
    } else {
        asks_[loc.price].erase(loc.iter);
        pruneLevel(Side::SELL, loc.price);
    }

    orderMap_.erase(mapIt);
    return true;
}

// ─── Public: Accessors ────────────────────────────────────────────────────────

std::optional<Price> OrderBook::bestBid() const {
    if (bids_.empty()) return std::nullopt;
    return bids_.begin()->first;
}

std::optional<Price> OrderBook::bestAsk() const {
    if (asks_.empty()) return std::nullopt;
    return asks_.begin()->first;
}

Quantity OrderBook::quantityAtPrice(Side side, Price price) const {
    Quantity total = 0;
    if (side == Side::BUY) {
        auto it = bids_.find(price);
        if (it == bids_.end()) return 0;
        for (const auto& o : it->second) total += o.quantity;
    } else {
        auto it = asks_.find(price);
        if (it == asks_.end()) return 0;
        for (const auto& o : it->second) total += o.quantity;
    }
    return total;
}

// ─── Public: Display ─────────────────────────────────────────────────────────

void OrderBook::printBook(int depth) const {
    std::cout << "\n═══ Order Book: " << symbol_ << " ═══\n";
    std::cout << std::setw(12) << "Price" << std::setw(12) << "Qty"
              << "   Side\n";
    std::cout << std::string(36, '-') << "\n";

    // Print asks in reverse (so highest ask is at top, best ask near spread)
    std::vector<std::pair<Price, Quantity>> askLevels;
    int count = 0;
    for (const auto& [price, level] : asks_) {
        if (count++ >= depth) break;
        Quantity qty = 0;
        for (const auto& o : level) qty += o.quantity;
        askLevels.push_back({price, qty});
    }
    for (int i = (int)askLevels.size() - 1; i >= 0; --i) {
        std::cout << std::setw(12) << priceToString(askLevels[i].first)
                  << std::setw(12) << askLevels[i].second
                  << "   SELL\n";
    }

    // Spread line
    std::cout << std::string(36, '-') << "  <-- SPREAD\n";

    // Print bids
    count = 0;
    for (const auto& [price, level] : bids_) {
        if (count++ >= depth) break;
        Quantity qty = 0;
        for (const auto& o : level) qty += o.quantity;
        std::cout << std::setw(12) << priceToString(price)
                  << std::setw(12) << qty
                  << "   BUY\n";
    }
    std::cout << std::string(36, '-') << "\n\n";
}

// ─── Private: Matching Logic ─────────────────────────────────────────────────

std::vector<Trade> OrderBook::matchOrder(Order& incoming) {
    std::vector<Trade> trades;

    // Depending on side, we match against the opposite side.
    // BUY incoming matches against SELL resting orders (asks), lowest first.
    // SELL incoming matches against BUY resting orders (bids), highest first.

    auto tryMatch = [&](auto& restingBook, auto priceCheck) {
        while (incoming.quantity > 0 && !restingBook.empty()) {
            auto levelIt = restingBook.begin();

            // Price check: can we trade at this level?
            // For MARKET orders, priceCheck always returns true.
            if (!priceCheck(levelIt->first)) break;

            PriceLevel& level = levelIt->second;

            while (incoming.quantity > 0 && !level.empty()) {
                Order& resting = level.front();

                Quantity fillQty = std::min(incoming.quantity, resting.quantity);
                Price fillPrice = resting.price;  // Maker price governs

                trades.emplace_back(resting.id, incoming.id, fillPrice, fillQty);

                incoming.quantity -= fillQty;
                resting.quantity  -= fillQty;

                if (resting.quantity == 0) {
                    resting.status = OrderStatus::FILLED;
                    // Remove from order map before erasing from list
                    orderMap_.erase(resting.id);
                    level.pop_front();
                } else {
                    resting.status = OrderStatus::PARTIALLY_FILLED;
                }
            }

            // Clean up empty price level
            if (level.empty()) {
                restingBook.erase(levelIt);
            }
        }
    };

    if (incoming.side == Side::BUY) {
        if (incoming.type == OrderType::MARKET) {
            tryMatch(asks_, [](Price) { return true; });
        } else {
            // Limit BUY: only match if ask <= our limit price
            tryMatch(asks_, [&](Price askPrice) {
                return askPrice <= incoming.price;
            });
        }
    } else {
        // SELL
        if (incoming.type == OrderType::MARKET) {
            tryMatch(bids_, [](Price) { return true; });
        } else {
            // Limit SELL: only match if bid >= our limit price
            tryMatch(bids_, [&](Price bidPrice) {
                return bidPrice >= incoming.price;
            });
        }
    }

    if (incoming.quantity < incoming.originalQty && incoming.quantity > 0)
        incoming.status = OrderStatus::PARTIALLY_FILLED;
    else if (incoming.quantity == 0)
        incoming.status = OrderStatus::FILLED;

    return trades;
}

// ─── Private: Helpers ─────────────────────────────────────────────────────────

void OrderBook::pruneLevel(Side side, Price price) {
    if (side == Side::BUY) {
        auto it = bids_.find(price);
        if (it != bids_.end() && it->second.empty()) bids_.erase(it);
    } else {
        auto it = asks_.find(price);
        if (it != asks_.end() && it->second.empty()) asks_.erase(it);
    }
}