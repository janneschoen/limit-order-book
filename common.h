#include <map>
#include <deque>
#ifndef COMMON_H
#define COMMON_H

#define SLEEP_TIME 0.1     // Delay between loop iterations [s]
#define MID_START 100       // Midpoint value at start of program
#define MAX_MID_DISTANCE 30 // Maximal distance of order price to midpoint
#define MAX_ORDER_QTY 10    // Maximal units one order can buy/sell

// Constants for readable indexing
#define BUY 0
#define SELL 1

struct Order{
    bool side;
    unsigned quantity;
    unsigned price;
};

// Generate a random order around mid price in range defined by MAX_MID_DISTANCE
Order generate_order(unsigned mid_price);

// Get best bid / ask from list of orders
unsigned get_best_price(bool side, std::map<unsigned, std::deque<Order>> &orders);

#endif