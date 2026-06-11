// common.h — shared definitions for the limit order book simulation
//
// This is the single header for the project. It defines the Order struct,
// simulation constants, and the interface for order generation and price
// lookup. Both orders.cpp and main.cpp include it.

#ifndef COMMON_H
#define COMMON_H

#include <map>
#include <deque>

// Simulation parameters
#define SLEEP_TIME       0.1   // seconds between ticks
#define MID_START        100   // initial mid-price
#define MAX_MID_DISTANCE 30    // max offset of an order's price from the mid
#define MAX_ORDER_QTY    10    // max quantity per order

// Order side constants — used as both Order.side and get_best_price() argument
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

// Return the best price on the given side: highest bid for BUY,
// lowest ask for SELL. Returns 0 if the side is empty.
unsigned get_best_price(bool side, std::map<unsigned, std::deque<Order>> &orders);

#endif
