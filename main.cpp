// main.cpp — limit order book simulation with sliding-window price index
//
// Each tick:
//   1. Update the mid-price via a constrained random walk.
//   2. Recenter the sliding window if the mid has drifted too far.
//   3. Generate a random limit order and insert it into the book.
//   4. Run the matching engine: while best_bid ≥ best_ask, fill at the
//      resting price with price-time priority (FIFO per price level).
//   5. Print best bid and best ask, then sleep until the next tick.
//
// The order book uses a flat array of deques indexed by (price - base),
// replacing the previous std::map. This gives O(1) price-level access,
// cache-friendly best-price scans, and eliminates red-black tree overhead.

#include "common.h"
#include <iostream>
#include <chrono>
#include <thread>

// ── Sliding-window book side ───────────────────────────────────────

// Each side of the book (bids or asks) is a flat array of FIFO queues
// plus a cached index to the best (most competitive) price level.
struct BookSide {
    std::deque<Order> levels[WINDOW_SIZE];
    int best_idx;  // index of the best price level, or -1 if empty

    BookSide() : best_idx(-1) {}
};

// Map a logical price to its zero-based slot within the window.
// Caller guarantees the result is in [0, WINDOW_SIZE).
inline int idx(unsigned price, int base) {
    return (int)price - base;
}

// Check whether a price falls inside the current window.
inline bool in_window(unsigned price, int base) {
    int i = idx(price, base);
    return i >= 0 && i < WINDOW_SIZE;
}

// ── Best-price queries ─────────────────────────────────────────────

// Scan the bid side top-down to find the highest price with resting
// orders.  Stores the result in best_idx and returns the price (0 if
// the side is empty).
unsigned find_best_bid(BookSide &bids, int base) {
    for (int i = WINDOW_SIZE - 1; i >= 0; i--) {
        if (!bids.levels[i].empty()) {
            bids.best_idx = i;
            return (unsigned)(base + i);
        }
    }
    bids.best_idx = -1;
    return 0;
}

// Scan the ask side bottom-up to find the lowest price with resting
// orders.  Stores the result in best_idx and returns the price (0 if
// the side is empty).
unsigned find_best_ask(BookSide &asks, int base) {
    for (int i = 0; i < WINDOW_SIZE; i++) {
        if (!asks.levels[i].empty()) {
            asks.best_idx = i;
            return (unsigned)(base + i);
        }
    }
    asks.best_idx = -1;
    return 0;
}

// ── Incremental best-price updates ─────────────────────────────────
//
// After a fill empties the best level, walk in the less-competitive
// direction until we hit the next occupied slot.  These are called
// inside the tight matching loop, so they avoid re-scanning the
// entire window.

void update_best_bid(BookSide &bids) {
    if (bids.best_idx < 0) return;
    if (!bids.levels[bids.best_idx].empty()) return;
    int i = bids.best_idx - 1;
    while (i >= 0 && bids.levels[i].empty()) i--;
    bids.best_idx = i;
}

void update_best_ask(BookSide &asks) {
    if (asks.best_idx < 0) return;
    if (!asks.levels[asks.best_idx].empty()) return;
    int i = asks.best_idx + 1;
    while (i < WINDOW_SIZE && asks.levels[i].empty()) i++;
    asks.best_idx = (i < WINDOW_SIZE) ? i : -1;
}

// ── Window recentering ─────────────────────────────────────────────
//
// When the mid-price drifts too close to an edge of the window, we
// shift base so the mid sits at the center again.  Only active orders
// in the overlap region are relocated; orders that fall outside the
// new window are impossible in practice because WINDOW_SIZE is 4× the
// active range and recentering happens at the 25%/75% marks.

void recenter_side(BookSide &side, int old_base, int new_base) {
    if (new_base == old_base) return;

    std::deque<Order> tmp[WINDOW_SIZE];
    for (int i = 0; i < WINDOW_SIZE; i++) {
        if (side.levels[i].empty()) continue;
        unsigned price = (unsigned)(old_base + i);
        int new_i = idx(price, new_base);
        if (new_i >= 0 && new_i < WINDOW_SIZE)
            tmp[new_i].swap(side.levels[i]);
    }

    // Remap the cached best index to the new base
    if (side.best_idx >= 0) {
        unsigned best_price = (unsigned)(old_base + side.best_idx);
        side.best_idx = idx(best_price, new_base);
    }

    for (int i = 0; i < WINDOW_SIZE; i++)
        side.levels[i].swap(tmp[i]);
}

// ── Utility ────────────────────────────────────────────────────────

void sleep(float num_seconds) {
    unsigned num_milliseconds = num_seconds * 1000;
    std::this_thread::sleep_for(std::chrono::milliseconds(num_milliseconds));
}

// ── Main simulation ────────────────────────────────────────────────

int main() {
    // Window covers [base, base + WINDOW_SIZE).  Start with mid near
    // the center, clamped so the lowest price (1) is always covered.
    int base_price = (int)MID_START - WINDOW_SIZE / 2;
    if (base_price < 1) base_price = 1;

    BookSide bids, asks;
    unsigned mid = MID_START;

    for (;;) {

        // ── 1. Mid-price random walk ─────────────────────────
        // Step by -1, 0, or +1, but never let the mid drop so low
        // that generated prices could go below 1.
        mid += (mid - MAX_MID_DISTANCE - 1 > 0)
                   ? (unsigned)(rand() % 3 - 1)
                   : 1U;

        // ── 2. Recenter the window if needed ────────────────
        // Shift base when the mid is within 25% of either edge so
        // that newly-generated orders (mid ± MAX_MID_DISTANCE)
        // always land inside the window.
        int rel = (int)mid - base_price;
        if (rel < WINDOW_SIZE / 4 || rel > WINDOW_SIZE * 3 / 4) {
            int new_base = (int)mid - WINDOW_SIZE / 2;
            if (new_base < 1) new_base = 1;
            recenter_side(bids, base_price, new_base);
            recenter_side(asks, base_price, new_base);
            base_price = new_base;
        }

        // ── 3. Order arrival ────────────────────────────────
        Order o = generate_order(mid);
        int i = idx(o.price, base_price);
        if (o.side == BUY)
            bids.levels[i].push_back(o);
        else
            asks.levels[i].push_back(o);

        // ── 4. Matching engine ──────────────────────────────
        // Price-time priority: within a price level the oldest
        // order (front of deque) is filled first.
        unsigned best_bid = find_best_bid(bids, base_price);
        unsigned best_ask = find_best_ask(asks, base_price);

        while (best_bid && best_ask && best_bid >= best_ask) {
            int bid_i = bids.best_idx;
            int ask_i = asks.best_idx;

            unsigned bid_vol  = bids.levels[bid_i][0].quantity;
            unsigned ask_vol  = asks.levels[ask_i][0].quantity;
            unsigned fill_vol = std::min(bid_vol, ask_vol);

            bids.levels[bid_i][0].quantity -= fill_vol;
            asks.levels[ask_i][0].quantity -= fill_vol;

            if (bids.levels[bid_i][0].quantity == 0) {
                bids.levels[bid_i].pop_front();
                update_best_bid(bids);
            }
            if (asks.levels[ask_i][0].quantity == 0) {
                asks.levels[ask_i].pop_front();
                update_best_ask(asks);
            }

            best_bid = (bids.best_idx >= 0)
                           ? (unsigned)(base_price + bids.best_idx)
                           : 0U;
            best_ask = (asks.best_idx >= 0)
                           ? (unsigned)(base_price + asks.best_idx)
                           : 0U;
        }

        // ── 5. Output ───────────────────────────────────────
        std::cout << best_bid << " | " << best_ask << "\n";

        sleep(SLEEP_TIME);
    }

    return 0;
}
