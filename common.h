// common.h — shared definitions for the limit order book simulation
//
// This is the single header for the project. It defines the Order struct,
// simulation constants, and the interface for order generation.
// Both orders.cpp and main.cpp include it.

#ifndef COMMON_H
#define COMMON_H

#include <deque>

// Simulation parameters
#define SLEEP_TIME       0.1   // seconds between ticks
#define MID_START        100   // initial mid-price
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
