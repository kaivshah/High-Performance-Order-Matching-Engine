#pragma once

#include <cstdint>
#include <string>

// Price is stored as fixed-point integer: 1 unit = 0.01 cents (i.e., $100.50 = 10050)
// This avoids floating-point precision issues common in financial systems.
using Price = int64_t;
using Quantity = uint32_t;
using OrderId = uint64_t;
using Timestamp = uint64_t;

enum class Side : uint8_t {
    BUY,
    SELL
};

enum class OrderType : uint8_t {
    LIMIT,
    MARKET
};

enum class OrderStatus : uint8_t {
    NEW,
    PARTIALLY_FILLED,
    FILLED,
    CANCELLED
};

struct Order {
    OrderId   id;
    Side      side;
    OrderType type;
    Price     price;      // Fixed-point: cents (e.g. $100.50 -> 10050)
    Quantity  quantity;   // Remaining quantity
    Quantity  originalQty;
    Timestamp timestamp;  // Nanoseconds since epoch (for time priority)
    OrderStatus status;

    Order(OrderId id_, Side side_, OrderType type_, Price price_,
          Quantity qty_, Timestamp ts_)
        : id(id_), side(side_), type(type_), price(price_),
          quantity(qty_), originalQty(qty_), timestamp(ts_),
          status(OrderStatus::NEW)
    {}
};

// Emitted every time a match occurs between a resting and aggressing order.
struct Trade {
    OrderId   makerOrderId;  // Resting order (was already in book)
    OrderId   takerOrderId;  // Aggressing order (just arrived)
    Price     price;         // Execution price (maker's price)
    Quantity  quantity;      // Filled quantity

    Trade(OrderId maker, OrderId taker, Price p, Quantity q)
        : makerOrderId(maker), takerOrderId(taker), price(p), quantity(q)
    {}
};

// Utility: convert fixed-point price to human-readable string
inline std::string priceToString(Price p) {
    // Price is in cents, display as dollars.cents
    bool negative = p < 0;
    if (negative) p = -p;
    std::string result = std::to_string(p / 100) + "." +
                         (p % 100 < 10 ? "0" : "") + std::to_string(p % 100);
    return (negative ? "-" : "") + result;
}