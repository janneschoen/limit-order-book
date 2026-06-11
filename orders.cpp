#include "common.h"
#include <iostream>
#include <map>
#include <deque>

Order generate_order(unsigned mid_price){
    Order order;
    order.side = rand() % 2;

    order.quantity = rand() % MAX_ORDER_QTY + 1;

    // Random price in range: midprice +- maximal mid distance
    order.price = mid_price - MAX_MID_DISTANCE + rand() % (MAX_MID_DISTANCE * 2 + 1);

    return order;
}

unsigned get_best_price(bool side, std::map<unsigned, std::deque<Order>> &orders){
    unsigned num_orders = orders.size();
    unsigned best_price = 0;
    if(num_orders){
        if(side == BUY){
            best_price = orders.rbegin()->second[0].price;
        }
        if(side == SELL){
            best_price = orders.begin()->second[0].price;
        }
    }
    return best_price;
}