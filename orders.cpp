#include "common.h"
#include <iostream>
#include <map>
#include <deque>

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

// Return the best price on the given side of the book.
// Bids are sorted ascending by std::map → best bid is the last key
// (highest price willing to buy). Asks are also sorted ascending →
// best ask is the first key (lowest price willing to sell).
// Returns 0 when the side is empty (valid since all prices are ≥ 1).
unsigned get_best_price(bool side, std::map<unsigned, std::deque<Order>> &orders) {
    if (orders.empty()) return 0;

    if (side == BUY)
        return orders.rbegin()->second[0].price;  // highest price
    else
        return orders.begin()->second[0].price;   // lowest price
}
