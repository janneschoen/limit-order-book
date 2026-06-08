#include "common.h"
#include <iostream>

Order generate_order(int mid_price){
    Order order;
    order.side = rand() % 2;

    order.quantity = rand() % MAX_ORDER_QTY + 1;

    order.price = mid_price - MAX_MID_DISTANCE + rand() % (MAX_MID_DISTANCE * 2 + 1);

    return order;
}