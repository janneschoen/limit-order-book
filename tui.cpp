// tui.cpp — FTXUI-based terminal visualization for the limit order book
//
// Renders a real-time Depth-of-Market (DOM) ladder:
//   - Vertical price column in the center (descending: high → low)
//   - Green horizontal bars on the left  → bid volume at each level
//   - Red   horizontal bars on the right → ask volume at each level
//   - Header: mid price, spread, best bid/ask, tick & fill counters
//   - Side panel: rolling trade tape of the last N fills
//   - Bottom bar: keyboard controls
//
// Build: see Makefile (requires FTXUI libraries)

#include "common.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include <algorithm>
#include <chrono>
#include <deque>
#include <iomanip>
#include <sstream>
#include <thread>
#include <vector>

// ── Simulation constants (tunable) ───────────────────────────────

#define VISIBLE_LEVELS 24   // number of price levels shown above/below mid
#define BAR_WIDTH      16   // characters per volume bar (each side)
#define TAPE_LENGTH    50   // max entries in the trade tape

// ── Sliding-window book side ─────────────────────────────────────

struct BookSide {
    std::deque<Order> levels[WINDOW_SIZE];
    int best_idx;  // index of the best (most competitive) price, or -1 if empty

    BookSide() : best_idx(-1) {}
};

inline int idx(unsigned price, int base) { return (int)price - base; }

inline bool in_window(unsigned price, int base) {
    int i = idx(price, base);
    return i >= 0 && i < WINDOW_SIZE;
}

// ── Trade record ──────────────────────────────────────────────────

struct Fill {
    unsigned price;
    unsigned volume;
    bool     taker_side;  // BUY = aggressive buyer (hit the ask),
                          // SELL = aggressive seller (hit the bid)
    unsigned tick;
};

// ── Book operations ──────────────────────────────────────────────

// Total resting volume at a single price level.
unsigned level_volume(const BookSide &side, int i) {
    if (i < 0 || i >= WINDOW_SIZE) return 0;
    unsigned vol = 0;
    for (auto &o : side.levels[i])
        vol += o.quantity;
    return vol;
}

// Scan bid side top-down for the highest price with orders.
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

// Scan ask side bottom-up for the lowest price with orders.
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

// Walk downward from the current best after a fill empties a level.
void update_best_bid(BookSide &bids) {
    if (bids.best_idx < 0) return;
    if (!bids.levels[bids.best_idx].empty()) return;
    int i = bids.best_idx - 1;
    while (i >= 0 && bids.levels[i].empty()) i--;
    bids.best_idx = i;
}

// Walk upward from the current best after a fill empties a level.
void update_best_ask(BookSide &asks) {
    if (asks.best_idx < 0) return;
    if (!asks.levels[asks.best_idx].empty()) return;
    int i = asks.best_idx + 1;
    while (i < WINDOW_SIZE && asks.levels[i].empty()) i++;
    asks.best_idx = (i < WINDOW_SIZE) ? i : -1;
}

// Shift the window base so the mid sits at the center again.
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

    if (side.best_idx >= 0) {
        unsigned best_price = (unsigned)(old_base + side.best_idx);
        side.best_idx = idx(best_price, new_base);
    }

    for (int i = 0; i < WINDOW_SIZE; i++)
        side.levels[i].swap(tmp[i]);
}

// ── Formatting helpers ────────────────────────────────────────────

// Right-align an unsigned in a field of `width` chars.
std::string fmt_u(unsigned val, int width) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%*u", width, val);
    return buf;
}

// Render a horizontal bar using █ and ░ characters for a fraction [0,1].
// `right_fill`: if true the bar grows from the right edge (for bids),
//                otherwise it grows from the left edge (for asks).
std::string render_bar(float fraction, int bar_width, bool right_fill) {
    int filled = (int)(fraction * (float)bar_width + 0.5f);
    if (filled > bar_width) filled = bar_width;
    if (filled < 0) filled = 0;
    int empty = bar_width - filled;

    std::string bar;
    const char *block = "█";
    if (right_fill) {
        for (int i = 0; i < empty; i++)  bar += ' ';
        for (int i = 0; i < filled; i++) bar += block;
    } else {
        for (int i = 0; i < filled; i++) bar += block;
        for (int i = 0; i < empty; i++)  bar += ' ';
    }
    return bar;
}

// ── Main ──────────────────────────────────────────────────────────

int main() {
    using namespace ftxui;

    // ── State ──────────────────────────────────────────────────
    BookSide bids, asks;
    unsigned mid = MID_START;
    int base_price = (int)MID_START - WINDOW_SIZE / 2;
    if (base_price < 1) base_price = 1;

    unsigned tick      = 0;
    unsigned fill_count = 0;
    unsigned best_bid  = 0;
    unsigned best_ask  = 0;
    bool     paused    = false;
    int      speed_ms  = (int)(SLEEP_TIME * 1000);  // tick interval in ms

    std::deque<Fill> tape;

    // ── Screen ─────────────────────────────────────────────────
    auto screen = ScreenInteractive::Fullscreen();

    // ── Keyboard handler ───────────────────────────────────────
    auto event_handler = CatchEvent([&](Event event) {
        if (event == Event::Character('q') || event == Event::Character('Q')) {
            screen.Exit();
            return true;
        }
        if (event == Event::Character('p') || event == Event::Character('P')) {
            paused = !paused;
            return true;
        }
        if (event == Event::Character('r') || event == Event::Character('R')) {
            // Reset state
            bids = BookSide();
            asks = BookSide();
            mid = MID_START;
            base_price = (int)MID_START - WINDOW_SIZE / 2;
            if (base_price < 1) base_price = 1;
            tick = 0;
            fill_count = 0;
            best_bid = 0;
            best_ask = 0;
            tape.clear();
            return true;
        }
        if (event == Event::Character('>') || event == Event::Character('.')) {
            // Speed up (reduce sleep interval)
            speed_ms = std::max(10, speed_ms - 20);
            return true;
        }
        if (event == Event::Character('<') || event == Event::Character(',')) {
            // Slow down (increase sleep interval)
            speed_ms = std::min(1000, speed_ms + 20);
            return true;
        }
        return false;
    });

    // ── Renderer ───────────────────────────────────────────────
    auto renderer = Renderer([&] {

        // --- Header row ---
        auto header = hbox({
            text(" MID: ") | dim,
            text(std::to_string(mid)) | bold | color(Color::Cyan),
            separator(),
            text(" SPREAD: ") | dim,
            text(best_bid && best_ask
                     ? std::to_string(best_ask - best_bid)
                     : "--") | bold | color(Color::Yellow),
            separator(),
            text(" BID: ") | dim,
            text(best_bid ? fmt_u(best_bid, 0) : "---") | color(Color::Green),
            separator(),
            text(" ASK: ") | dim,
            text(best_ask ? fmt_u(best_ask, 0) : "---") | color(Color::Red),
            separator(),
            text(" TICK: ") | dim,
            text(std::to_string(tick)) | bold,
            separator(),
            text(" FILLS: ") | dim,
            text(std::to_string(fill_count)) | bold | color(Color::Yellow),
        });

        // --- Price ladder ---

        // Determine the visible price range centered on mid.
        int half_range = VISIBLE_LEVELS / 2;
        int top_price  = (int)mid + half_range;
        int bot_price  = (int)mid - half_range + 1;  // +1 because we iterate
                                                      // top_price down to
                                                      // bot_price inclusive

        // Find the maximum volume in the visible window to normalise bars.
        unsigned max_vol = 1;  // avoid division by zero
        for (int p = top_price; p >= bot_price; p--) {
            if (p < 1) continue;
            if (!in_window((unsigned)p, base_price)) continue;
            int i = idx((unsigned)p, base_price);
            max_vol = std::max(max_vol, level_volume(bids, i));
            max_vol = std::max(max_vol, level_volume(asks, i));
        }

        Elements rows;

        // Column header — mirrors the data row layout so labels sit
        // directly above the volume numbers, not the bars.
        rows.push_back(
            hbox({
                text(std::string(BAR_WIDTH, ' ')),    // placeholder for bid bars
                text("BIDS") | dim | color(Color::Green),
                text(" ") | dim,
                text("PRICE") | dim,
                text(" ") | dim,
                text("ASKS") | dim | color(Color::Red),
                text(std::string(BAR_WIDTH, ' ')),    // placeholder for ask bars
            }) | size(HEIGHT, EQUAL, 1));

        rows.push_back(separator());

        for (int p = top_price; p >= bot_price; p--) {
            if (p < 1) continue;

            unsigned bid_vol = 0, ask_vol = 0;
            float bid_frac = 0.f, ask_frac = 0.f;

            if (in_window((unsigned)p, base_price)) {
                int i = idx((unsigned)p, base_price);
                bid_vol = level_volume(bids, i);
                ask_vol = level_volume(asks, i);
                bid_frac = (float)bid_vol / (float)max_vol;
                ask_frac = (float)ask_vol / (float)max_vol;
            }

            // Colour the price if it's the best bid or ask.
            Color price_color = Color::Default;
            auto price_style = nothing;
            if (best_bid && p == (int)best_bid) {
                price_color = Color::Green;
                price_style = bold;
            } else if (best_ask && p == (int)best_ask) {
                price_color = Color::Red;
                price_style = bold;
            }
            // Highlight the spread region
            if (best_bid && best_ask && p < (int)best_bid && p > (int)best_ask) {
                price_color = Color::Yellow;
            }

            // Build one row:  [bar] [vol] [price] [vol] [bar]
            auto bid_vol_text = bid_vol ? text(fmt_u(bid_vol, 4)) | color(Color::Green)
                                        : text("    ") | dim;
            auto ask_vol_text = ask_vol ? text(fmt_u(ask_vol, 4)) | color(Color::Red)
                                        : text("    ") | dim;

            // Bid bar (fills from right edge → toward center)
            auto bid_bar =
                bid_frac > 0.001f
                    ? text(render_bar(bid_frac, BAR_WIDTH, /*right_fill=*/true)) |
                          color(Color::Green)
                    : text(std::string(BAR_WIDTH, ' ')) | dim;

            // Ask bar (fills from left edge → toward center)
            auto ask_bar =
                ask_frac > 0.001f
                    ? text(render_bar(ask_frac, BAR_WIDTH, /*right_fill=*/false)) |
                          color(Color::Red)
                    : text(std::string(BAR_WIDTH, ' ')) | dim;

            auto price_text =
                text(fmt_u((unsigned)p, 5)) | color(price_color) | price_style;

            rows.push_back(hbox({
                bid_bar,
                bid_vol_text,
                text(" ") | dim,  // spacer
                price_text,
                text(" ") | dim,  // spacer
                ask_vol_text,
                ask_bar,
            }));
        }

        auto ladder =
            vbox(std::move(rows)) | vscroll_indicator | yframe | flex;

        // --- Trade tape ---
        Elements tape_rows;
        tape_rows.push_back(
            text(" TRADE TAPE ") | bold | hcenter | color(Color::Yellow));
        tape_rows.push_back(separator());

        // Show newest fills at the top.
        int shown = 0;
        for (auto it = tape.rbegin(); it != tape.rend() && shown < TAPE_LENGTH;
             ++it, ++shown) {
            const char *arrow = (it->taker_side == BUY) ? "↑" : "↓";
            Color c = (it->taker_side == BUY) ? Color::Green : Color::Red;
            auto row = hbox({
                text(std::string(" ") + arrow + " ") | color(c) | bold,
                text(fmt_u(it->volume, 4)) | bold,
                text(" @ ") | dim,
                text(fmt_u(it->price, 5)) | color(c) | bold,
                text("  t=" + std::to_string(it->tick)) | dim,
            });
            tape_rows.push_back(row);
        }
        if (tape.empty()) {
            tape_rows.push_back(text("  (no fills yet)") | dim | center);
        }

        auto tape_panel =
            vbox(std::move(tape_rows)) | vscroll_indicator | yframe | flex |
            border;

        // --- Footer / controls ---
        auto controls = hbox({
            text(" [Q]uit ") | dim,
            text(paused ? " [P]aused " : " [P]ause ") | dim,
            text(" [</>]Speed:" + std::to_string(speed_ms) + "ms ") | dim,
            text(" [R]eset ") | dim,
            filler(),
            text(" Limit Order Book — FTXUI TUI ") | dim,
        });

        // --- Final layout ---
        return vbox({
            header | border,
            hbox({
                ladder | flex | border,
                tape_panel | size(WIDTH, GREATER_THAN, 28),
            }) | flex,
            controls,
        });
    });

    // Compose the keyboard handler into the renderer.
    renderer |= event_handler;

    // ── Main loop ──────────────────────────────────────────────
    Loop loop(&screen, renderer);

    // We use a frame-timer approach: every N ms we run one tick,
    // redraw, and poll for input.
    using Clock = std::chrono::steady_clock;
    auto next_tick = Clock::now();

    while (!loop.HasQuitted()) {
        auto now = Clock::now();

        // Process FTXUI events (keyboard, resize, etc.).
        loop.RunOnce();

        if (loop.HasQuitted()) break;

        // Run the simulation if enough time has passed and not paused.
        if (!paused && now >= next_tick) {
            next_tick = now + std::chrono::milliseconds(speed_ms);
            tick++;

            // --- 1. Mid-price random walk ---
            mid += (mid - MAX_MID_DISTANCE - 1 > 0)
                       ? (unsigned)(rand() % 3 - 1)
                       : 1U;

            // --- 2. Recenter window if needed ---
            int rel = (int)mid - base_price;
            if (rel < WINDOW_SIZE / 4 || rel > WINDOW_SIZE * 3 / 4) {
                int new_base = (int)mid - WINDOW_SIZE / 2;
                if (new_base < 1) new_base = 1;
                recenter_side(bids, base_price, new_base);
                recenter_side(asks, base_price, new_base);
                base_price = new_base;
            }

            // --- 3. Order arrival ---
            Order o = generate_order(mid);
            int i = idx(o.price, base_price);
            if (o.side == BUY)
                bids.levels[i].push_back(o);
            else
                asks.levels[i].push_back(o);

            // --- 4. Matching engine ---
            best_bid = find_best_bid(bids, base_price);
            best_ask = find_best_ask(asks, base_price);

            while (best_bid && best_ask && best_bid >= best_ask) {
                int bid_i = bids.best_idx;
                int ask_i = asks.best_idx;

                unsigned bid_qty = bids.levels[bid_i][0].quantity;
                unsigned ask_qty = asks.levels[ask_i][0].quantity;
                unsigned fill_qty = std::min(bid_qty, ask_qty);

                bids.levels[bid_i][0].quantity -= fill_qty;
                asks.levels[ask_i][0].quantity -= fill_qty;

                // Record the fill: trade price is the resting (bid) price.
                // Alternate aggressor side for visual variety.
                {
                    Fill f;
                    f.price      = best_bid;
                    f.volume     = fill_qty;
                    f.taker_side = (fill_count % 2 == 0) ? BUY : SELL;
                    f.tick       = tick;
                    tape.push_back(f);
                    if ((int)tape.size() > TAPE_LENGTH * 2)
                        tape.pop_front();
                }
                fill_count++;

                // Garbage-collect filled orders.
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

            // Update best_bid/best_ask for display when no cross occurred.
            if (!best_bid) best_bid = find_best_bid(bids, base_price);
            if (!best_ask) best_ask = find_best_ask(asks, base_price);

            // Request a redraw.
            screen.RequestAnimationFrame();
        }

        // Avoid busy-waiting: poll roughly every 10 ms.
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return 0;
}
