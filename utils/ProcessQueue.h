
#ifndef PROCESS_QUEUES_H
#define PROCESS_QUEUES_H

#include <semaphore.h>
#include <functional>
#include <cstdint>

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

template <size_t buffer_size>
class CircularBuffer {
    size_t ridx = 0, widx = 0;
    volatile uint8_t buffer[buffer_size-3*sizeof(Semaphore)-2*sizeof(size_t)];
    Semaphore writer{sizeof(buffer)}, reader{0}, lock{1};
public:
    CircularBuffer() = default;
    template <typename T>
    void write(const T &obj) {
        auto bytes = (uint8_t *) &obj;
        lock.down();
        size_t addr = widx;
        widx = (widx + sizeof(T)) % sizeof(buffer);
        lock.up();
        for(size_t offset = 0; offset < sizeof(T); offset++) {
            writer.down();
            buffer[(addr + offset) % sizeof(buffer)] = bytes[offset];
            reader.up();
        }
    }
    template <typename T>
    void read(T &obj) {
        auto bytes = (uint8_t *) &obj;
        lock.down();
        size_t addr = ridx;
        ridx = (ridx + sizeof(T)) % sizeof(buffer);
        lock.up();
        for(size_t offset = 0; offset < sizeof(T); offset++) {
            reader.down();
            bytes[offset] = buffer[(addr + offset) % sizeof(buffer)];
            writer.up();
        }
    }

    static_assert(sizeof(CircularBuffer<buffer_size>) == buffer_size);
};

template <size_t buffer_size>
class MessageQueue {
    static constexpr int64_t buff_size = buffer_size-8*sizeof(Semaphore)-4*sizeof(size_t);
    static_assert(buff_size > 0);
    volatile uint8_t buffer[buff_size];
    size_t ridx1 = 0, widx1 = 0, ridx2 = 0, widx2 = 0;
    Semaphore writer1{sizeof(buffer)}, reader1{0}, writer2{0}, reader2{0};
    Semaphore w1lock{1}, r1lock{1}, w2lock{1}, r2lock{1};
    
    template <size_t num>
    inline void move(Semaphore &slock, Semaphore &s1, Semaphore &s2, volatile uint8_t *dest, volatile uint8_t *src, size_t daddr, size_t saddr) {
        for(size_t offset = 0; offset < num; offset++) {
            s1.down();
            dest[(daddr + offset) % sizeof(buffer)] = src[(saddr + offset) % sizeof(buffer)];
            s2.up();
        }
        slock.up();
    }
public:
    static constexpr size_t size = buff_size;
    MessageQueue() {
        static_assert(sizeof(MessageQueue<buffer_size>) == buffer_size);
    };
    template <typename T>
    void send(const T &obj) {
        w1lock.down();
        size_t addr = widx1;
        widx1 = (widx1 + sizeof(T)) % sizeof(buffer);
        move<sizeof(T)>(w1lock, writer1, reader1, buffer, (uint8_t *) &obj, addr, 0);
    }
    template <typename T>
    void receive(T &obj) {
        r1lock.down();
        size_t addr = ridx1;
        ridx1 = (ridx1 + sizeof(T)) % sizeof(buffer);
        move<sizeof(T)>(r1lock, reader1, writer2, (uint8_t *) &obj, buffer, 0, addr);
    }
    template <typename T>
    void respond(const T &obj) {
        w2lock.down();
        size_t addr = widx2;
        widx2 = (widx2 + sizeof(T)) % sizeof(buffer);
        move<sizeof(T)>(w2lock, writer2, reader2, buffer, (uint8_t *) &obj, addr, 0);
    }
    template <typename T>
    void response(T &obj) {
        r2lock.down();
        size_t addr = ridx2;
        ridx2 = (ridx2 + sizeof(T)) % sizeof(buffer);
        move<sizeof(T)>(r2lock, reader2, writer1, (uint8_t *) &obj, buffer, 0, addr);
    }
};

template <size_t buffer_size, typename Message, typename Response>
class ProcessQueue {
    MessageQueue<buffer_size> queue;
    union MessageReponse {
        Message message;
        Response response;
    };

public:
    void serve(std::function<Response(const Message &)> callback) {
        MessageReponse data;
        queue.receive(data);
        data.response = callback(data.message);
        queue.respond(data);
    }
    Response operator()(const Message &message) {
        MessageReponse data;
        data.message = message;
        queue.send(data);
        queue.response(data);
        return data.response;
    }
};

static_assert(sizeof(ProcessQueue<1024, int, int>) == 1024);

#endif
