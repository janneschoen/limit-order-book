#include "common.h"
#include <iostream>
#include <map>
#include <deque>
#include <chrono>
#include <thread>

void sleep(float num_seconds){
    int num_milliseconds = num_seconds * 1000;
    std::this_thread::sleep_for(std::chrono::milliseconds(num_milliseconds));
}

int main(){

    std::map<int, std::deque<Order>> bids;
    std::map<int, std::deque<Order>> asks;

    int mid_price = MID_START;

    for(int i = 0; 1; i ++){

        // Move mid price randomly by -1, 0, or 1
        // If negative move could lead to price < 1, move up
        mid_price += mid_price - MAX_MID_DISTANCE - 1 > 0 ? rand() % 3 - 1 : 1;

        // Generate a random order
        Order new_order = generate_order(mid_price);
        new_order.timestamp = i;

        // Add new order to queue of bids / asks based on price
        if(new_order.side == BUY){
            bids[new_order.price].push_back(new_order);
        } else{
            asks[new_order.price].push_back(new_order);
        }

        // Get best bid / ask
        int best_bid = get_best_price(BUY, bids);
        int best_ask = get_best_price(SELL, asks);

        std::cout << best_bid << " | " << best_ask << "\n";

        // TODO: order filling


        sleep(0.0001);
    }

    return 0;
}