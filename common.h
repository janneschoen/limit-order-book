#ifndef COMMON_H
#define COMMON_H

#define BUY 0
#define SELL 1

struct Order_t{
    bool side;
    int quantity;
    unsigned price;
    int timestamp;
};

#endif