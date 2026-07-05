#include "common.h"
#include <algorithm>
#include <iostream>

// Generate a random limit order clustered around the given mid-price.
// Side is a fair coin flip; quantity is uniform within [1, MAX_ORDER_QTY].
//
// Price uses a NON-UNIFORM distribution to model realistic market behavior:
// most limit orders are placed near the mid (tightening the spread), while
// fewer land at the extremes.
//
// We take the minimum of 3 independent uniform randoms in [0, MAX_MID_DISTANCE).
// This concentrates probability mass near zero:
//   P(offset ≤ D*1/3) ≈ 58%       P(offset ≤ D*2/3) ≈ 88%
//   P(offset ≤ D*1/2) ≈ 75%       P(offset ≤ D)     = 100%
//
// Without this, a uniform distribution causes the outer tail to constantly
// sweep away inner liquidity, producing a permanently wide spread.
Order generate_order(unsigned mid_price) {
    Order order;
    order.side     = rand() % 2;
    order.quantity = rand() % MAX_ORDER_QTY + 1;

    // Min-of-3 uniforms: offset heavily concentrated near zero.
    int range  = MAX_MID_DISTANCE;
    int offset = std::min({rand() % range, rand() % range, rand() % range});
    if (rand() % 2)
        order.price = mid_price + offset;
    else
        order.price = mid_price - offset;

    return order;
}
