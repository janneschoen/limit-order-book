#include "common.h"
#include <iostream>
#include <map>
#include <deque>
#include <chrono>
#include <thread>

void sleep(float num_seconds){
    unsigned num_milliseconds = num_seconds * 1000;
    std::this_thread::sleep_for(std::chrono::milliseconds(num_milliseconds));
}

int main(){

    std::map<unsigned, std::deque<Order>> bids;
    std::map<unsigned, std::deque<Order>> asks;

    unsigned mid_price = MID_START;

    for(int i = 0; 1; i ++){

        // Move mid price randomly by -1, 0, or 1
        // Constrain: If negative move could lead to price < 1, move up
        mid_price += mid_price - MAX_MID_DISTANCE - 1 > 0 ? rand() % 3 - 1 : 1;

        // Generate a random order
        Order new_order = generate_order(mid_price);

        // Add new order to queue of bids / asks based on price
        if(new_order.side == BUY){
            bids[new_order.price].push_back(new_order);
        } else{
            asks[new_order.price].push_back(new_order);
        }

        // Get best bid / ask
        unsigned best_bid = get_best_price(BUY, bids);
        unsigned best_ask = get_best_price(SELL, asks);
 
        while(
            best_bid && best_ask && // there are bids and asks
            best_bid >= best_ask    // book is crossed
        ){
            // Calculate how much of best orders can be filled
            unsigned bid_vol = bids[best_bid][0].quantity;
            unsigned ask_vol = asks[best_ask][0].quantity;
            unsigned fill_vol = std::min(bid_vol, ask_vol);

            // Fill orders as possible
            bids[best_bid][0].quantity -= fill_vol;
            asks[best_ask][0].quantity -= fill_vol;

            // Remove fully filled orders and clean up empty price levels
            if(bids[best_bid][0].quantity == 0){
                bids[best_bid].pop_front();
                if(bids[best_bid].empty()){
                    bids.erase(best_bid);
                }
            }
            if(asks[best_ask][0].quantity == 0){
                asks[best_ask].pop_front();
                if(asks[best_ask].empty()){
                    asks.erase(best_ask);
                }
            }

            // Recalculate best prices for next iteration
            best_bid = get_best_price(BUY, bids);
            best_ask = get_best_price(SELL, asks);
        }

        std::cout << best_bid << " | " << best_ask << "\n";

        sleep(SLEEP_TIME);
    }

    return 0;
}