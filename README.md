# Order Matching Engine

A high-performance, single-threaded equity Limit Order Book (LOB) matching engine
written in modern C++17.
---

## Features

| Feature | Detail |
|---|---|
| **Price-Time Priority** | Orders at same price are filled oldest-first (FIFO) |
| **Fixed-Point Pricing** | Prices stored as `int64_t` cents — no float precision bugs |
| **O(log N) Add/Match** | `std::map` price levels; best bid/ask always at `begin()` |
| **O(1) Cancel** | `std::unordered_map<OrderId, iterator>` into stable `std::list` |
| **Maker/Taker Model** | Trade reports identify resting (maker) vs aggressing (taker) order |
| **Market Orders** | Fully aggressive, sweep book, never rest |
| **Multi-Symbol** | Engine manages one `OrderBook` per symbol |
| **Throughput Benchmark** | `--benchmark` mode: ~1M orders, reports orders/sec |
| **21 Unit Tests** | GTest suite covering all matching rules and edge cases |

---

## Architecture

```
MatchingEngine         ← Assigns IDs, timestamps, routes to symbol books
    └── OrderBook      ← Per-symbol Limit Order Book
            ├── BidLevels  (std::map<Price, std::list<Order>, std::greater>)
            ├── AskLevels  (std::map<Price, std::list<Order>, std::less>)
            └── orderMap_  (std::unordered_map<OrderId, OrderLocation>)
```



---

## Build

### Requirements
- CMake 3.14+
- C++17 compiler (GCC 9+, Clang 10+, MSVC 2019+)
- Internet access (FetchContent downloads GoogleTest on first build)

### Steps

```bash
# Clone and enter project
cd order_matching_engine

# Configure (Release build)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build everything
cmake --build build -j$(nproc)

# Run unit tests
cd build && ctest --output-on-failure

# Run interactive mode
./matching_engine

# Run throughput benchmark
./matching_engine --benchmark
```

---

## Interactive Mode

```
> buy AAPL 15050 100        # Limit BUY 100 shares of AAPL at $150.50
> sell AAPL 15050 100       # Limit SELL → matches above order
[TRADE] Maker=1 Taker=2 Price=150.50 Qty=100

> buy AAPL 14900 500        # Resting bid at $149.00
> cancel AAPL 3             # Cancel order 3
> book AAPL                 # Print order book
> stats                     # Engine throughput stats
> quit
```

---

## Test Coverage

| Test | Validates |
|---|---|
| `AddBidAppearsInBook` | Resting order is stored correctly |
| `BestBidIsHighestBid` | Map sort order (descending) |
| `BestAskIsLowestAsk` | Map sort order (ascending) |
| `ExactMatch` | Full fill, both sides cleared |
| `NoMatchWhenPricesDontCross` | Limit price respected |
| `PartialFillTakerSmaller` | Remainder stays on resting side |
| `PartialFillMakerSmaller` | Taker remainder rests in book |
| `MultipleLevelsSweep` | Aggressor sweeps multiple price levels |
| `TimePriorityWithinPriceLevel` | FIFO within same price |
| `PricePriorityOverTimePriority` | Better price beats older time |
| `CancelRemovesOrder` | Clean removal from book |
| `CancelNonExistentReturnsFalse` | Safe cancel of unknown ID |
| `CancelOneOfManyAtSameLevel` | Targeted cancel, level survives |
| `CancelFilledOrderFails` | Filled orders no longer in map |
| `MarketBuyFillsAtBestAsk` | Market order price agnosticism |
| `MarketOrderDoesNotRestInBook` | Unmatched market qty discarded |
| `SubmitAndMatchViaEngine` | End-to-end engine integration |
| `CancelViaEngine` | Cancel via engine API |
