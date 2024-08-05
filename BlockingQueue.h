#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <limits>

//Avoid issues with windows.h
#ifndef NOMINMAX
#define NOMINMAX
#endif

template<typename T>
class BlockingQueue {
private:
    std::queue<T> queue;
    mutable std::mutex mutex;
    std::condition_variable notEmpty;
    std::condition_variable notFull;
    size_t capacity;

public:
    //Use parentheses around the entire expression to avoid it being mistaken for a macro invocation.
    BlockingQueue(size_t maxCapacity = (std::numeric_limits<size_t>::max)())
        : capacity(maxCapacity) {}
    ~BlockingQueue() {}

    /*Adds an item to this BlockingQueue instance.
    @param item The item to add.*/
    void add(T item) {
        std::unique_lock<std::mutex> lock(mutex);
        notFull.wait(lock, [this]() { return queue.size() < capacity; });
        queue.push(std::move(item));
        notEmpty.notify_one();
    }

    /*Tries to take an item from this BlockingQueue instance.
    @returns std::nullopt or the item that was taken.*/
    std::optional<T> tryTake() {
        std::unique_lock<std::mutex> lock(mutex);

        if (queue.empty())
            return std::nullopt;

        T item = std::move(queue.front());
        queue.pop();
        notFull.notify_one();

        return item;
    }

    /*Takes an item from this BlockingQueue instance.
    @returns The item that was taken.*/
    T take() {
        std::unique_lock<std::mutex> lock(mutex);
        notEmpty.wait(lock, [this]() { return !queue.empty(); });
        T item = std::move(queue.front());
        queue.pop();
        notFull.notify_one();

        return item;
    }

    /*The number of items in this BlockingQueue instance.*/
    size_t count() const {
        std::lock_guard<std::mutex> lock(mutex);
        return queue.size();
    }

    /*Is this BlockingQueue instance empty?
    @returns True if it is, false otherwise.*/
    bool isEmpty() const {
        std::lock_guard<std::mutex> lock(mutex);
        return queue.empty();
    }
};