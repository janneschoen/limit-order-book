// common.h — type definitions and simulation constants for the limit order book
//
// Declarations only.  Implementations live in book.cpp and orders.cpp.

#ifndef COMMON_H
#define COMMON_H

#include <deque>

// ── Simulation constants (tunable) ────────────────────────────────

#define MID_START        100   // initial mid-price
#define MID_STEP_EVERY   50    // update mid every N ticks
#define MAX_MID_DISTANCE 30    // max offset of an order's price from the mid
#define MAX_ORDER_QTY    10    // max quantity per order

// Sliding window: flat array indexed by (price - base).
// Buffer is 4× the active range so window recentering is infrequent.
#define WINDOW_SIZE 256

// Order side constants
#define BUY  0
#define SELL 1

// ── Data structures ───────────────────────────────────────────────

struct Order {
    bool     side;      // BUY (0) or SELL (1)
    unsigned quantity;  // number of units
    unsigned price;     // limit price
};

// Each side of the book (bids or asks) is a flat array of FIFO queues
// plus a cached index to the best (most competitive) price level.
struct BookSide {
    std::deque<Order> levels[WINDOW_SIZE];
    int best_idx;  // index of the best price level, or -1 if empty

    BookSide() : best_idx(-1) {}
};

// ── Book engine interface ────────────────────────────────────────

// Price <-> window-index mapping
int  idx(unsigned price, int base);
bool in_window(unsigned price, int base);

// Total resting volume at a single price level
unsigned level_volume(const BookSide &side, int i);

// Full-scan best-price queries (used after new order arrival / init)
unsigned find_best_bid(BookSide &bids, int base);
unsigned find_best_ask(BookSide &asks, int base);

// Incremental best-price updates after a fill empties a level
void update_best_bid(BookSide &bids);
void update_best_ask(BookSide &asks);

// Shift the window base so the mid sits at the centre again
void recenter_side(BookSide &side, int old_base, int new_base);

// ── Order generation ─────────────────────────────────────────────

Order generate_order(unsigned mid_price);

#endif
