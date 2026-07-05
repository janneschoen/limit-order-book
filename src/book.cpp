// book.cpp — sliding-window limit order book engine
//
// Implements the matching-engine operations declared in common.h:
// price indexing, volume queries, best-price scans, incremental
// best-price updates, and window recentering.

#include "common.h"
#include <algorithm>

// ── Price indexing ────────────────────────────────────────────────

int idx(unsigned price, int base) {
    return (int)price - base;
}

bool in_window(unsigned price, int base) {
    int i = idx(price, base);
    return i >= 0 && i < WINDOW_SIZE;
}

// ── Volume query ──────────────────────────────────────────────────

unsigned level_volume(const BookSide &side, int i) {
    if (i < 0 || i >= WINDOW_SIZE) return 0;
    unsigned vol = 0;
    for (const auto &o : side.levels[i])
        vol += o.quantity;
    return vol;
}

// ── Best-price queries ────────────────────────────────────────────

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

// ── Incremental best-price updates ────────────────────────────────

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

// ── Window recentering ────────────────────────────────────────────

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
