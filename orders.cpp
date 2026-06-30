#include "common.h"
#include <iostream>

// Generate a random limit order clustered around the given mid-price.
// Side is a fair coin flip; quantity and price are uniform within the
// ranges defined by MAX_ORDER_QTY and MAX_MID_DISTANCE.
Order generate_order(unsigned mid_price) {
    Order order;
    order.side     = rand() % 2;
    order.quantity = rand() % MAX_ORDER_QTY + 1;

    // Price uniformly distributed in [mid - MAX_MID_DISTANCE,
    //                                mid + MAX_MID_DISTANCE]
    order.price = mid_price - MAX_MID_DISTANCE
                + rand() % (MAX_MID_DISTANCE * 2 + 1);

    return order;
}
