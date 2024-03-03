//
// Created by Han on 2024/3/3.
//

#ifndef SPSC_FIFO_SPSC_FIFO_H
#define SPSC_FIFO_SPSC_FIFO_H

#include <deque>
#include <atomic>
#include <future>

#define DEPTH_INFINITE 0xffffffff
#define DEFAULT_LATENCY 1

uint32_t getCurrentTime(){
    return 0;
}

/// Lock-free SPSC latency fifo
/// This fifo is only thread-safe if used with the following pre-conditions:
/// 1. Only a single producer and a single consumer can access the fifo from different threads
/// 2. The update() operation cannot happen simultaneously with other functions
/// 3. The getCurrentTime() function should be thread-safe
template<typename T>
struct spsc_latency_fifo{
public:
    spsc_latency_fifo() = default;
    explicit spsc_latency_fifo(uint32_t depth=DEPTH_INFINITE, uint32_t latency=DEFAULT_LATENCY)
        :depth_(depth), latency_(latency){}

    /// write with default latency using rvalue (std::move or temp)
    bool write(T&& data){
        // rvalue
        if(this->full()){
            return false;
        }
        size_.fetch_add(1, std::memory_order_acq_rel);
        input_queue_.template emplace_back({data, getCurrentTime()+latency_});
        return true;
    }
    /// write with default latency with lvalue (named variables)
    bool write(T& data){
        // lvalue
        if(this->full()){
            return false;
        }
        size_.fetch_add(1, std::memory_order_acq_rel);
        input_queue_.template emplace_back({data, getCurrentTime()+latency_});
        return true;
    }
    T read(){
        assert(this->valid());
        T tmp = output_queue_.front();
        output_queue_.pop_front();
        output_size_ -= 1;
        size_.fetch_sub(1, std::memory_order_acq_rel);
    }
    T peek_front(){
        assert(this->valid());
        return output_queue_.front();
    }
    inline bool full() const {
        return size_.load(std::memory_order_acquire) >= depth_;
    }
    inline bool empty() const {
        return size_.load(std::memory_order_acquire) == 0;
    }
    inline bool valid() const {
        return output_size_ > 0;
    }
    /// update() can only be called when there's no read/write
    void update(){
        const auto current_time = getCurrentTime();
        while(!input_queue_.empty()){
            if(input_queue_.front().second <= current_time){
                // ready
                output_queue_.template emplace_back(std::move(input_queue_.front().first));
                input_queue_.pop_front();
                output_size_++;
            }
            else{
                break;
            }
        }
    }

private:
    uint32_t depth_ = DEPTH_INFINITE;
    uint32_t latency_ = DEFAULT_LATENCY;
    std::atomic<uint32_t> size_{0}; // total size of fifo
    uint32_t output_size_{0}; // size of valid output
    std::deque<T> output_queue_;
    std::deque<std::pair<T, uint32_t>> input_queue_;
};

#endif //SPSC_FIFO_SPSC_FIFO_H
