// common.h — shared definitions for the limit order book simulation
//
// This is the single header for the project. It defines the Order struct,
// simulation constants, and the interface for order generation.
// Both orders.cpp and main.cpp include it.

#ifndef COMMON_H
#define COMMON_H

#include <deque>

// Simulation parameters (tunable)
//
// SLEEP_TIME       — seconds between ticks (console mode)
// MID_START        — initial mid-price
// MID_STEP_EVERY   — how many ticks between mid-price random-walk steps.
//                    Higher = slower mid drift, letting resting liquidity
//                    build up visibly near the spread.
// MAX_MID_DISTANCE — max offset of an order from mid.
//                    With min-of-3 distribution in orders.cpp,
//                    ~58% of orders land within D/3 of mid, producing a
//                    tight visible spread.
// MAX_ORDER_QTY    — max quantity per order (uniform in [1, MAX_ORDER_QTY])
#define SLEEP_TIME       0.1   // seconds between ticks
#define MID_START        100   // initial mid-price
#define MID_STEP_EVERY   50     // update mid every N ticks
#define MAX_MID_DISTANCE 30    // max offset of an order's price from the mid
#define MAX_ORDER_QTY    10    // max quantity per order

// Sliding window: flat array indexed by (price - base) for O(1) access.
// Buffer is 4× the active range so the window recenters infrequently
// (every ~64 ticks of mid-price drift) with no orders ever lost.
#define WINDOW_SIZE 256

// Order side constants — used as both Order.side and internal helpers
#define BUY  0
#define SELL 1

struct Order {
    bool     side;      // BUY (0) or SELL (1)
    unsigned quantity;  // number of units
    unsigned price;     // limit price
};

// Generate a random order with price in [mid - MAX_MID_DISTANCE,
// mid + MAX_MID_DISTANCE], quantity in [1, MAX_ORDER_QTY], and
// side chosen uniformly.
Order generate_order(unsigned mid_price);

#endif
