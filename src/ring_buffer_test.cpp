// g++ ring_buffer_test.cpp -lpthread
#include <iostream>
#include <thread>
#include <chrono>
#include "ring_buffer.hpp"


RingBuffer<uint64_t> queue(1);

void producer(void) {
    // generate an item and put it into the queue
    uint64_t item = 0;
    while (1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        queue.push(std::move(item));
        item++;
    }
}

int main() {

    std::thread prod(producer);

    uint64_t step = 0;

    while (1) {
        // run half the speed of the producer
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        // get item from the queue and print it
        uint64_t item = queue.pop();
        std::cout << "Consumer in step " << step << ": Item: " << item << " (qsize = " << queue.size() << ")"<< std::endl;

        step++;
    }

    return 0;
}
