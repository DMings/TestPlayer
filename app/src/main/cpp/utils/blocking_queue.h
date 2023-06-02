//
// Created by odm on 2023/3/8.
//

#ifndef LDSTREAMS_BLOCKING_QUEUE_H
#define LDSTREAMS_BLOCKING_QUEUE_H

#include <pthread.h>
#include <vector>

typedef unsigned long ulong;

template<typename T>
class BlockingQueue {
private:
    pthread_mutex_t mutex_;
    pthread_cond_t not_full_;
    pthread_cond_t not_empty_;
    volatile ulong head_;
    volatile ulong tail_;
    ulong capacity_;
    std::vector<T> queue_;
    volatile bool cannot_wait_;
public:
    explicit BlockingQueue(ulong capacity) :
            capacity_(capacity),
            queue_(capacity + 1),
            head_(0),
            tail_(0) {
        cannot_wait_ = false;
        pthread_mutex_init(&mutex_, NULL);
        pthread_cond_init(&not_full_, NULL);
        pthread_cond_init(&not_empty_, NULL);
    }

    ~BlockingQueue() {
        queue_.clear();
        pthread_mutex_destroy(&mutex_);
        pthread_cond_destroy(&not_full_);
        pthread_cond_destroy(&not_empty_);
    }

    ulong size() const {
        // 注tail_/head_采用volatile修饰，此方法并没有加锁保护
        return (tail_ - head_ + capacity_) % capacity_;
    }

    void push(const T &e) {
        pthread_mutex_lock(&mutex_);
        while (is_full() && !cannot_wait_) { // [1] 为什么要使用while
            pthread_cond_wait(&not_full_, &mutex_); // [2]为什么要传入互斥锁
        }
        if (cannot_wait_) {
            pthread_cond_signal(&not_empty_);
            pthread_mutex_unlock(&mutex_);
            return;
        }

        queue_[tail_++] = e;
        tail_ %= (capacity_ + 1);
        pthread_cond_signal(&not_empty_); // [3] 采用signal还是broadcast
        pthread_mutex_unlock(&mutex_);
    }

    void pop(T &t) {
        pthread_mutex_lock(&mutex_);
        while (is_empty() && !cannot_wait_) {
            pthread_cond_wait(&not_empty_, &mutex_);
        }
        if (cannot_wait_) {
            pthread_cond_signal(&not_full_);
            pthread_mutex_unlock(&mutex_);
            return;
        }
        T res = queue_[head_++];
        head_ %= (capacity_ + 1);
        pthread_cond_signal(&not_full_);
        pthread_mutex_unlock(&mutex_);
        t = res;
    }

    void cannotWait() {
        pthread_mutex_lock(&mutex_);
        cannot_wait_ = true;
        pthread_cond_broadcast(&not_empty_);
        pthread_mutex_unlock(&mutex_);
    }

private:
    inline bool is_empty() {
        return tail_ == head_;
    }

    inline bool is_full() {
        return (head_ + capacity_ - tail_) % (capacity_ + 1) == 0;
    }
};


#endif //LDSTREAMS_BLOCKING_QUEUE_H
