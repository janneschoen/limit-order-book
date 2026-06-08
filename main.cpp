#include "common.h"
#include <iostream>
#include <map>
#include <chrono>
#include <thread>

#define MID_START 100       // Midpoint value at start of program
#define MAX_MID_DISTANCE 30 // Maximal distance of order price to midpoint
#define MAX_ORDER_QTY 10    // Maximal units one order can buy/sell

void sleep(float num_seconds){
    int num_milliseconds = num_seconds * 1000;
    std::this_thread::sleep_for(std::chrono::milliseconds(num_milliseconds));
}

int main(){

    std::map<int, Order_t> bids;
    std::map<int, Order_t> asks;

    int mid_price = MID_START;

    for(int i = 0; 1; i ++){

        // Random walk of mid price
        mid_price += mid_price > MAX_MID_DISTANCE ? rand() % 3 - 1 : 1; // move by -1, 0, or 1

        int num_bids = bids.size();
        int num_asks = asks.size();

        std::cout << "TIME: " << i << "\n";

        std::cout << "Mid: " << mid_price << "\n";

        std::cout << "Bids: " << num_bids << "\n";
        std::cout << "Asks: " << num_asks << "\n";

        std::cout << "\n";


        sleep(0.1);
    }

    return 0;
}