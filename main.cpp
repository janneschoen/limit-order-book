// main.cpp — limit order book simulation
//
// Each tick:
//   1. Update the mid-price via a constrained random walk.
//   2. Generate a random limit order and insert it into the book.
//   3. Run the matching engine: while best_bid ≥ best_ask, fill at the
//      resting price with price-time priority (FIFO per price level).
//   4. Print best bid and best ask, then sleep until the next tick.

#include "common.h"
#include <iostream>
#include <map>
#include <deque>
#include <chrono>
#include <thread>

void sleep(float num_seconds) {
    unsigned num_milliseconds = num_seconds * 1000;
    std::this_thread::sleep_for(std::chrono::milliseconds(num_milliseconds));
}

int main() {
    // The two halves of the order book: bids and asks.
    // std::map sorts keys ascending → for bids the best price is the
    // highest key (rbegin), for asks it's the lowest key (begin).
    // std::deque per price level enforces FIFO (price-time priority).
    std::map<unsigned, std::deque<Order>> bids;
    std::map<unsigned, std::deque<Order>> asks;

    unsigned mid_price = MID_START;

    for (;;) {

        // --- 1. Mid-price random walk ---
        // Step by -1, 0, or +1, but never let the mid drop so low
        // that generated prices could go below 1.
        mid_price += (mid_price - MAX_MID_DISTANCE - 1 > 0)
                        ? rand() % 3 - 1
                        : 1;

        // --- 2. Order arrival ---
        Order new_order = generate_order(mid_price);
        if (new_order.side == BUY)
            bids[new_order.price].push_back(new_order);
        else
            asks[new_order.price].push_back(new_order);

        // --- 3. Matching engine ---
        // Continuously cross the book while best_bid ≥ best_ask.
        // Price-time priority: within a price level, the oldest order
        // (front of deque) is filled first. Partial fills leave the
        // remainder in place. Fully filled orders and empty price
        // levels are garbage-collected inline.
        unsigned best_bid = get_best_price(BUY,  bids);
        unsigned best_ask = get_best_price(SELL, asks);

        while (best_bid && best_ask && best_bid >= best_ask) {
            unsigned bid_vol  = bids[best_bid][0].quantity;
            unsigned ask_vol  = asks[best_ask][0].quantity;
            unsigned fill_vol = std::min(bid_vol, ask_vol);

            bids[best_bid][0].quantity -= fill_vol;
            asks[best_ask][0].quantity -= fill_vol;

            // Remove fully filled orders; prune empty price levels
            if (bids[best_bid][0].quantity == 0) {
                bids[best_bid].pop_front();
                if (bids[best_bid].empty())
                    bids.erase(best_bid);
            }
            if (asks[best_ask][0].quantity == 0) {
                asks[best_ask].pop_front();
                if (asks[best_ask].empty())
                    asks.erase(best_ask);
            }

            best_bid = get_best_price(BUY,  bids);
            best_ask = get_best_price(SELL, asks);
        }

        // --- 4. Output ---
        std::cout << best_bid << " | " << best_ask << "\n";

        sleep(SLEEP_TIME);
    }

    return 0;
}
