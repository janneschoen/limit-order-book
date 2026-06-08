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
        mid_price += mid_price > MAX_MID_DISTANCE ? rand() % 3 - 1 : 1;

        // Generate a random order
        Order new_order = generate_order(mid_price);
        new_order.timestamp = i;

        // Add new order to queue of bids / asks based on price
        if(new_order.side == BUY){
            bids[new_order.price].push_back(new_order);
        } else{
            asks[new_order.price].push_back(new_order);
        }

        // Get best bid / ask if they exist
        int num_bids = bids.size();
        int num_asks = asks.size();
    
        int best_bid, best_ask;
        if(num_bids) best_bid = bids.rbegin()->second[0].price;
        if(num_asks) best_ask = asks.begin()->second[0].price;

        // Print spread
        std::cout << "Bid " << (num_bids ? std::to_string(best_bid) : "None") << " | " << (num_asks ? std::to_string(best_ask) : "None") << " ask\n";

        // TODO: order filling

        sleep(0.1);
    }

    return 0;
}