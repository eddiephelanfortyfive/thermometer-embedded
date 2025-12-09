#ifndef CIRCULAR_BUFFER_HPP
#define CIRCULAR_BUFFER_HPP

#include <cstddef>
#include <array>

// Fixed-capacity, header-only circular buffer.
// - No dynamic allocation (storage is embedded).
// - Single-producer / single-consumer friendly (no internal locking).
// - T should be copyable.
// - Methods are non-blocking; push/pop return false on full/empty.
template<typename T, std::size_t Capacity>
class CircularBuffer {
public:
    static_assert(Capacity > 0, "CircularBuffer capacity must be greater than zero");

    CircularBuffer() : head_index(0), tail_index(0), count(0) {}

    bool push(const T& value) {
        if (isFull()) {
            return false;
        }
        storage[head_index] = value;
        head_index = (head_index + 1U) % Capacity;
        ++count;
        return true;
    }

    bool pop(T& out_value) {
        if (isEmpty()) {
            return false;
        }
        out_value = storage[tail_index];
        tail_index = (tail_index + 1U) % Capacity;
        --count;
        return true;
    }

    bool peek(T& out_value) const {
        if (isEmpty()) {
            return false;
        }
        out_value = storage[tail_index];
        return true;
    }

    bool isFull() const {
        return count == Capacity;
    }

    bool isEmpty() const {
        return count == 0U;
    }

    std::size_t getCount() const {
        return count;
    }

    std::size_t getCapacity() const {
        return Capacity;
    }

    void clear() {
        head_index = 0U;
        tail_index = 0U;
        count = 0U;
    }

private:
    std::array<T, Capacity> storage;
    std::size_t head_index;
    std::size_t tail_index;
    std::size_t count;
};

#endif // CIRCULAR_BUFFER_HPP


