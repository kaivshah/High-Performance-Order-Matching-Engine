/*
 * main.cpp — Interactive CLI + Benchmark mode
 *
 * Usage:
 *   ./matching_engine              → interactive mode
 *   ./matching_engine --benchmark  → throughput benchmark
 *
 * Interactive commands:
 *   buy  <symbol> <price_cents> <qty>    — Limit buy order
 *   sell <symbol> <price_cents> <qty>    — Limit sell order
 *   mbuy <symbol> <qty>                  — Market buy order
 *   msell <symbol> <qty>                 — Market sell order
 *   cancel <symbol> <order_id>           — Cancel resting order
 *   book <symbol>                        — Print order book
 *   stats                                — Print engine statistics
 *   quit                                 — Exit
 */

#include "engine.h"
#include <iostream>
#include <sstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <random>

// ─── Benchmark ────────────────────────────────────────────────────────────────

void runBenchmark() {
    constexpr int NUM_ORDERS = 1'000'000;
    constexpr const char* SYM = "AAPL";

    MatchingEngine engine;
    engine.addSymbol(SYM);

    // Pre-seed book with resting orders on both sides
    for (int i = 0; i < 1000; ++i) {
        engine.submitLimitOrder(SYM, Side::BUY,  9900 - i, 100);  // bids below 99.00
        engine.submitLimitOrder(SYM, Side::SELL, 10100 + i, 100); // asks above 101.00
    }

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> priceDist(9850, 10150);
    std::uniform_int_distribution<int> qtyDist(1, 200);
    std::uniform_int_distribution<int> sideDist(0, 1);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_ORDERS; ++i) {
        Side  side  = (sideDist(rng) == 0) ? Side::BUY : Side::SELL;
        Price price = priceDist(rng);
        Quantity qty = qtyDist(rng);
        engine.submitLimitOrder(SYM, side, price, qty);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double secs = std::chrono::duration<double>(end - start).count();
    double throughput = NUM_ORDERS / secs;

    std::cout << "\n╔══════════════════════════════════════╗\n";
    std::cout << "║         BENCHMARK RESULTS            ║\n";
    std::cout << "╠══════════════════════════════════════╣\n";
    std::cout << "║  Orders     : " << std::setw(8) << NUM_ORDERS     << "              ║\n";
    std::cout << "║  Time       : " << std::setw(8) << std::fixed
              << std::setprecision(3) << secs          << "s             ║\n";
    std::cout << "║  Throughput : " << std::setw(8) << (uint64_t)throughput
              << " orders/sec   ║\n";
    std::cout << "╚══════════════════════════════════════╝\n\n";
    engine.printStats();
}

// ─── Interactive Mode ─────────────────────────────────────────────────────────

void runInteractive() {
    MatchingEngine engine;

    // Register a trade printer
    engine.onTrade([](const Trade& t) {
        std::cout << "[TRADE] Maker=" << t.makerOrderId
                  << " Taker=" << t.takerOrderId
                  << " Price=" << priceToString(t.price)
                  << " Qty=" << t.quantity << "\n";
    });

    // Default symbols
    engine.addSymbol("AAPL");
    engine.addSymbol("GOOG");
    engine.addSymbol("TSLA");

    std::cout << "═══════════════════════════════════════════════\n";
    std::cout << "   Order Matching Engine  —  Interactive Mode  \n";
    std::cout << "═══════════════════════════════════════════════\n";
    std::cout << "Symbols: AAPL, GOOG, TSLA\n";
    std::cout << "Commands: buy/sell/mbuy/msell/cancel/book/stats/quit\n";
    std::cout << "Price is in cents: $100.50 = 10050\n\n";

    std::string line;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;

        std::istringstream ss(line);
        std::string cmd;
        ss >> cmd;

        try {
            if (cmd == "quit" || cmd == "exit") {
                break;

            } else if (cmd == "buy" || cmd == "sell") {
                std::string sym;
                Price price; Quantity qty;
                ss >> sym >> price >> qty;
                Side side = (cmd == "buy") ? Side::BUY : Side::SELL;
                OrderId id = engine.submitLimitOrder(sym, side, price, qty);
                std::cout << "[OK] Limit " << cmd << " order submitted. ID=" << id << "\n";

            } else if (cmd == "mbuy" || cmd == "msell") {
                std::string sym;
                Quantity qty;
                ss >> sym >> qty;
                Side side = (cmd == "mbuy") ? Side::BUY : Side::SELL;
                OrderId id = engine.submitMarketOrder(sym, side, qty);
                std::cout << "[OK] Market order submitted. ID=" << id << "\n";

            } else if (cmd == "cancel") {
                std::string sym;
                OrderId id;
                ss >> sym >> id;
                bool ok = engine.cancelOrder(sym, id);
                std::cout << (ok ? "[OK] Order cancelled.\n" : "[FAIL] Order not found.\n");

            } else if (cmd == "book") {
                std::string sym;
                ss >> sym;
                auto* book = engine.getBook(sym);
                if (book) book->printBook();
                else std::cout << "[FAIL] Unknown symbol.\n";

            } else if (cmd == "stats") {
                engine.printStats();

            } else {
                std::cout << "[?] Unknown command: " << cmd << "\n";
            }
        } catch (const std::exception& e) {
            std::cout << "[ERROR] " << e.what() << "\n";
        }
    }

    engine.printStats();
    std::cout << "Goodbye.\n";
}

// ─── Entry Point ──────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "--benchmark") {
        runBenchmark();
    } else {
        runInteractive();
    }
    return 0;
}