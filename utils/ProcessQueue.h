
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
    CircularBuffer<size * sizeof(Message), Message> messages;
    CircularBuffer<size * sizeof(Response), Response> responses;
public:
    InterprocessQueues() = default;

    inline void send(const Message &message) {
        messages.send(message);
    }
    inline void send(const Response &response) {
        responses.send(response);
    }
    inline void receive(Message &message) {
        messages.receive(message);
    }
    inline void receive(Response &response) {
        responses.receive(response);
    }
};

#endif
