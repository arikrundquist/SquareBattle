
#ifndef PROCESS_QUEUES_H
#define PROCESS_QUEUES_H

#include <semaphore.h>
#include <functional>
#include <cstdint>
#include <atomic>

class Semaphore {
    sem_t s;
public:
    Semaphore(size_t amount) {
        sem_init(&s, 1, amount);
    }
    ~Semaphore() {
        sem_destroy(&s);
    }
    inline void down() {
        while(sem_wait(&s));
    }
    inline void up() {
        while(sem_post(&s));
    }
};

template <size_t buffer_size, typename T>
class CircularBuffer {
public:
    static constexpr size_t size = buffer_size / sizeof(T);
private:
    Semaphore writer{size}, reader{0}, lock{1};
    volatile size_t ridx = 0, widx = 0;
    T arr[size];
public:
    CircularBuffer() = default;

    void send(const T &data) {
        writer.down();
        lock.down();
        size_t addr = widx;
        widx = (addr + 1) % size;
        lock.up();
        arr[addr] = data;
        reader.up();
    }
    void receive(T &data) {
        reader.down();
        lock.down();
        size_t addr = ridx;
        ridx = (addr + 1) % size;
        lock.up();
        data = arr[addr];
        writer.up();
    }
};

template <size_t size, typename Message, typename Response>
class InterprocessQueues {
    union MessageResponse {
        void *_;
        Message message;
        Response response;

        MessageResponse() : _(0) { }
    };
    CircularBuffer<size, MessageResponse> sender;
    CircularBuffer<size, MessageResponse> receiver;
public:
    InterprocessQueues() = default;

    inline void send(const Message &message) {
        MessageResponse mr;
        mr.message = message;
        sender.send(mr);
    }
    inline void send(const Response &response) {
        MessageResponse mr;
        mr.response = response;
        receiver.send(mr);
    }
    inline void receive(Message &message) {
        MessageResponse mr;
        receiver.receive(mr);
        message = mr.message;
    }
    inline void receive(Response &response) {
        MessageResponse mr;
        sender.receive(mr);
        response = mr.response;
    }

    static constexpr size_t capacity = decltype(sender)::size;
};

#endif
