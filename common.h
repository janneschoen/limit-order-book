#ifndef COMMON_H
#define COMMON_H

#define MID_START 100       // Midpoint value at start of program
#define MAX_MID_DISTANCE 30 // Maximal distance of order price to midpoint
#define MAX_ORDER_QTY 10    // Maximal units one order can buy/sell

// Constants for readable indexing
#define BUY 0
#define SELL 1

struct Order{
    bool side;
    int quantity;
    unsigned price;
    int timestamp;
};

Order generate_order(int mid_price);

#endif