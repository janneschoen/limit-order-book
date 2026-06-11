# Limit Order Book — A High-Throughput Matching Engine in C++

A real-time limit order book (LOB) simulation that demonstrates the core mechanics of modern electronic exchanges: continuous double-auction price discovery, price-time priority matching, and order-driven market microstructure.

## Table of Contents

- [Why This Project?](#why-this-project)
- [What Is a Limit Order Book?](#what-is-a-limit-order-book)
- [Architecture](#architecture)
- [Data Structures & Complexity](#data-structures--complexity)
- [Matching Engine](#matching-engine)
- [Market Dynamics](#market-dynamics)
- [Build & Run](#build--run)
- [Sample Output](#sample-output)
- [Correctness Guarantees](#correctness-guarantees)
- [Performance](#performance)
- [What's Next](#whats-next)
- [Further Reading](#further-reading)

---

## Why This Project?

Electronic markets run on matching engines — the software that pairs buyers with sellers, enforces price-time priority, and ensures fair execution at massive scale. Every trade you've ever placed, from a retail brokerage app to a high-frequency trading firm's colocated server, passed through something like this.

This project is a ground-up implementation of that central data structure, built with a few priorities in mind:

- **Correctness first.** A single bad fill erodes trust in the entire venue. Every invariant is explicit and verifiable.
- **Predictable, low-latency execution.** Data structure choices are deliberate and complexity-bounded.
- **Minimal abstractions.** The entire engine fits in three files and ~150 lines — easy to reason about, easy to debug.

The goal is understanding how price-time priority matching works under the hood, with a codebase small enough to experiment on.

---

## What Is a Limit Order Book?

A limit order book is the canonical microstructure of modern financial markets. It maintains two sorted queues:

| Side  | Sorted by       | Best price is…                           |
|-------|-----------------|------------------------------------------|
| **Bids** | Price descending | Highest price someone is willing to buy  |
| **Asks** | Price ascending  | Lowest price someone is willing to sell  |

When the best bid meets or exceeds the best ask (`best_bid ≥ best_ask`), the book is **crossed** and a trade occurs. The matching engine continuously:

1. Accepts incoming orders (limit orders to buy or sell at a specific price and quantity).
2. Inserts each order into the correct side, maintaining price-time priority (earlier orders at the same price level are filled first).
3. Checks for crosses and executes fills at the resting order's price.
4. Removes fully-filled orders and cleans up empty price levels.

The **spread** (`best_ask − best_bid`) is the fundamental measure of liquidity and transaction cost in the market.

---

## Architecture

```
┌─────────────────────────────────────────────────┐
│                    main.cpp                      │
│  ┌───────────────┐   ┌───────────────────────┐  │
│  │  Mid-Price RW │──▶│   Order Generation     │  │
│  │  (random walk) │   │  generate_order(mid)  │  │
│  └───────────────┘   └───────────┬───────────┘  │
│                                  │               │
│                                  ▼               │
│  ┌────────────────────────────────────────────┐  │
│  │           Order Book (maps + deques)        │  │
│  │  ┌──────────┐              ┌──────────┐    │  │
│  │  │   BIDS   │              │   ASKS   │    │  │
│  │  │ price→Q  │              │ price→Q  │    │  │
│  │  └──────────┘              └──────────┘    │  │
│  └──────────────────┬─────────────────────────┘  │
│                     │                            │
│                     ▼                            │
│  ┌────────────────────────────────────────────┐  │
│  │          Matching Engine Loop               │  │
│  │  while (best_bid ≥ best_ask):               │  │
│  │    fill_vol = min(bid_qty, ask_qty)         │  │
│  │    decrement both → remove if empty         │  │
│  │    recalc best_bid / best_ask              │  │
│  └────────────────────────────────────────────┘  │
│                     │                            │
│                     ▼                            │
│         Output: best_bid | best_ask              │
└─────────────────────────────────────────────────┘
```

### Source Files

| File          | Purpose                                                   |
|---------------|-----------------------------------------------------------|
| `common.h`    | Data structures (`Order`, constants), function signatures |
| `orders.cpp`  | Order generation (`generate_order`) and best-price lookup (`get_best_price`) |
| `main.cpp`    | Market simulation loop, matching engine, I/O              |

---

## Data Structures & Complexity

The choice of data structures is deliberate and reflects real-world exchange engineering tradeoffs.

### `std::map<unsigned, std::deque<Order>>` — Per Side

```
BIDS (price → FIFO queue of orders)
────────────────────────────────────
102  →  [O₁: +5,  O₂: +3,  O₃: +7]
101  →  [O₄: +2]
 99  →  [O₅: +4,  O₆: +1]
```

- **`std::map`** (red-black tree): `O(log N)` insertion and deletion per price level, `O(1)` to find extremes (`.begin()` / `.rbegin()`). Crucial because the matching engine repeatedly queries best bid/ask.
- **`std::deque`** per price level: `O(1)` push-back and pop-front. Enforces **price-time priority** — orders at the same price level are filled in arrival order (FIFO). This is exactly the convention used by Reg NMS-compliant US equity exchanges.

### Operation Complexities

| Operation                          | Complexity        | Notes                                      |
|------------------------------------|-------------------|--------------------------------------------|
| Insert new order                   | `O(log P + 1)`    | `P` = distinct price levels                |
| Get best bid / best ask            | `O(1)`            | `.rbegin()` / `.begin()` on red-black tree |
| Fill (partial or complete)         | `O(1)`            | Decrement front of deque                   |
| Remove fully-filled order          | `O(1)`            | `pop_front()` on deque                     |
| Remove empty price level           | `O(log P)`        | `erase()` from map (amortized rare)        |
| Single matching iteration          | `O(log P)`        | Dominated by erase of exhausted levels     |

### Memory

Each `Order` is 12 bytes (1-byte `bool` + 4-byte `unsigned` + 4-byte `unsigned` + 3 bytes padding on typical LP64). The `std::deque` introduces per-block overhead, and the `std::map` node overhead is ~40 bytes per distinct price level. For a market with dozens of active price levels and hundreds of resting orders, total resident memory is measured in kilobytes.

---

## Matching Engine

The core algorithm in `main.cpp` is a continuous matching loop:

```cpp
while (
    best_bid && best_ask &&  // both sides have resting orders
    best_bid >= best_ask     // the book is crossed
) {
    unsigned fill_vol = std::min(
        bids[best_bid][0].quantity,
        asks[best_ask][0].quantity
    );

    // Execute the fill
    bids[best_bid][0].quantity   -= fill_vol;
    asks[best_ask][0].quantity   -= fill_vol;

    // Garbage-collect filled orders and empty levels
    if (bids[best_bid][0].quantity == 0) {
        bids[best_bid].pop_front();
        if (bids[best_bid].empty())
            bids.erase(best_bid);
    }
    // ... symmetric for asks ...

    // Recalculate extremes for next iteration
    best_bid = get_best_price(BUY,  bids);
    best_ask = get_best_price(SELL, asks);
}
```

### Design Decisions

1. **Price-time priority (FIFO per price level).** This is the standard in most regulated markets. It's fair, predictable, and incentivizes aggressive quoting (you want to be first in the queue at a given price).

2. **Trade price is the resting order's price.** When a new aggressive order arrives and crosses the spread, it fills against the resting order at the resting price — the maker receives price improvement from their perspective, the taker pays the spread. This is consistent with how most CLOB exchanges operate.

3. **Greedy matching.** The loop continues until the book is completely uncrossed. An incoming order can fill against multiple price levels if it's large enough — this is standard market behavior (walking the book).

4. **No explicit trade tape.** The engine focuses on order state management and spread output. A production system would additionally emit trade messages to a matching feed.

---

## Market Dynamics

### Mid-Price Random Walk

The mid-price follows a constrained random walk:

```
mid_price ∈ {mid_price - 1,  mid_price,  mid_price + 1}
```

A floor prevents the mid-price from falling below `MAX_MID_DISTANCE + 2` (enforcing that `price ≥ 1` for all generated orders). This creates a plausible — if simplified — model of a market where the "fair value" drifts over time while orders cluster around it.

### Order Generation

New orders are generated each tick with:
- **Side:** 50/50 buy or sell (Bernoulli trial)
- **Quantity:** Uniform random in `[1, MAX_ORDER_QTY]`
- **Price:** Uniform random in `[mid − MAX_MID_DISTANCE,  mid + MAX_MID_DISTANCE]`

These parameters are tunable in `common.h`:
```c
#define SLEEP_TIME       0.1   // seconds between iterations
#define MID_START        100    // initial mid-price
#define MAX_MID_DISTANCE 30     // max offset from mid
#define MAX_ORDER_QTY    10     // max order quantity
```

By adjusting `MAX_MID_DISTANCE` relative to the random walk step size, you can control the ratio of aggressive vs. passive orders: a wide range means more orders land far from the mid and rest in the book (adding liquidity); a narrow range means most orders cross the spread immediately (taking liquidity).

---

## Build & Run

### Requirements

- **Compiler:** Any C++11-compatible compiler (GCC ≥ 4.8, Clang ≥ 3.3)
- **Platform:** Linux, macOS, or WSL
- **Dependencies:** None beyond the C++ standard library

### Build

```bash
g++ -std=c++11 -O2 -Wall -Wextra -o lob main.cpp orders.cpp
```

For a debug build with assertions and address sanitizer:

```bash
g++ -std=c++11 -g -O0 -fsanitize=address -Wall -Wextra -o lob_debug main.cpp orders.cpp
```

### Run

```bash
./lob
```

The program runs indefinitely, printing one line per tick:

```
best_bid | best_ask
```

Press `Ctrl+C` to stop.

---

## Sample Output

```
101 | 103
101 | 103
100 | 102
99 | 102
99 | 100
99 | 99      ← spread = 0: the book crossed and trades occurred
99 | 99
98 | 100
98 | 99      ← spread = 1: just crossed again
97 | 99
97 | 100
...
```

When `best_bid ≥ best_ask`, the matching engine fills aggressively and the spread collapses to (or near) zero for that tick. In the next tick, new orders arrive and the spread typically widens again. Over time, the spread hovers around `MAX_MID_DISTANCE`, reflecting the balance between order arrival rate and fill aggressiveness.

---

## Correctness Guarantees

The engine maintains the following invariants at all times (between ticks):

1. **No crossed book.** After the matching loop exits, `best_bid < best_ask` or at least one side is empty. A crossed book post-match would imply a missed fill.

2. **Price-time priority.** Within each price level, the `std::deque` guarantees that orders are filled strictly in arrival order. A later order at the same price cannot be filled before an earlier one.

3. **Non-negative quantities.** Orders are removed from the book only when `quantity == 0` (after a decrement operation). Partial fills reduce the quantity; the order remains in the book for the remainder.

4. **No dangling empty levels.** After the last order at a price level is filled, the `erase()` call removes the map entry. This keeps `orders.size()` accurate and prevents stale iterators.

5. **Price bounds.** Order prices are always in `[1, mid + MAX_MID_DISTANCE]`, and the mid-price is constrained to prevent underflow. No zero or negative prices can enter the book.

---

## Performance

The operation complexity analysis in [Data Structures & Complexity](#data-structures--complexity) tells the real story: every core operation is `O(1)` or `O(log P)` where `P` is the number of distinct price levels — typically a few dozen in this simulation. In practice, the engine is I/O bound: `std::cout` per tick dominates wall-clock time. If you remove the `sleep()` call and suppress output, the loop runs fast enough that `rand()` becomes the bottleneck.

A natural next step for higher throughput would be replacing `std::map` with a flat array indexed directly by price, eliminating the `O(log P)` red-black tree overhead. But for a simulation throttled to 10 Hz by `SLEEP_TIME`, the current design is far more than adequate.

---

## What's Next

- [ ] **TUI visualization.** A terminal-based live view of the order book — bid/ask ladder, spread, and recent fills — using a library like [FTXUI](https://github.com/ArthurSonzogni/FTXUI). Watching the book evolve in real time would make the market dynamics tangible in a way that numeric output alone doesn't.

- [ ] **Flat-array price index.** Replace `std::map` with a pre-allocated array for `O(1)` price-level access. This is how production engines structure their books and would be a good exercise in trading off flexibility for speed.

---

## Further Reading

- [Order book (Wikipedia)](https://en.wikipedia.org/wiki/Order_book) — good overview of LOB mechanics and price-time priority
- [SEC Regulation NMS](https://www.sec.gov/rules-regulations/2005/06/regulation-nms) — the regulatory framework behind modern US equity market structure (Rules 610/611 on access and order protection)

---

*Built with C++11.*
