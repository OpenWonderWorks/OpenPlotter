/**
 * ============================================================================
 * OpenPlotter — Lock-Free Ring Buffer
 * ============================================================================
 *
 * Single-producer, single-consumer ring buffer safe for use between
 * main loop (producer) and ISR (consumer) without disabling interrupts.
 *
 * Used by the motion planner (producer) and stepper ISR (consumer).
 *
 * ============================================================================
 */

#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <string.h>

template <typename T, uint8_t SIZE>
class RingBuffer {
    static_assert((SIZE & (SIZE - 1)) == 0 || true,
                  "SIZE should ideally be a power of 2 for modulo optimization");
public:
    RingBuffer() : _head(0), _tail(0) {}

    /**
     * Push an item into the buffer.
     * @return true if successful, false if buffer is full.
     */
    bool push(const T& item) {
        uint8_t nextHead = (_head + 1) % SIZE;
        if (nextHead == _tail) {
            return false; // Buffer full
        }
        _buffer[_head] = item;
        _head = nextHead;
        return true;
    }

    /**
     * Pop an item from the buffer.
     * @return true if successful, false if buffer is empty.
     */
    bool pop(T& item) {
        if (_head == _tail) {
            return false; // Buffer empty
        }
        item = _buffer[_tail];
        _tail = (_tail + 1) % SIZE;
        return true;
    }

    /**
     * Peek at the next item without removing it.
     * @return pointer to the item, or nullptr if empty.
     */
    T* peek() {
        if (_head == _tail) return nullptr;
        return &_buffer[_tail];
    }

    /**
     * Peek at item at a specific index from tail.
     * @param offset 0 = tail (next to pop), 1 = second item, etc.
     * @return pointer to the item, or nullptr if out of range.
     */
    T* peekAt(uint8_t offset) {
        if (offset >= count()) return nullptr;
        uint8_t idx = (_tail + offset) % SIZE;
        return &_buffer[idx];
    }

    /**
     * Get a mutable reference to the item at the head (most recently pushed).
     * Useful for modifying the last planned block.
     * @return pointer to the head item, or nullptr if empty.
     */
    T* peekHead() {
        if (_head == _tail) return nullptr;
        uint8_t idx = (_head == 0) ? (SIZE - 1) : (_head - 1);
        return &_buffer[idx];
    }

    /**
     * @return number of items currently in the buffer.
     */
    uint8_t count() const {
        if (_head >= _tail) {
            return _head - _tail;
        }
        return SIZE - _tail + _head;
    }

    /**
     * @return number of free slots available.
     */
    uint8_t available() const {
        return (SIZE - 1) - count();  // -1 because one slot is always unused
    }

    /**
     * @return true if the buffer is empty.
     */
    bool isEmpty() const {
        return _head == _tail;
    }

    /**
     * @return true if the buffer is full.
     */
    bool isFull() const {
        return ((_head + 1) % SIZE) == _tail;
    }

    /**
     * Clear the buffer (not ISR-safe — call only from main loop).
     */
    void clear() {
        _head = 0;
        _tail = 0;
    }

    /**
     * @return the maximum capacity of the buffer.
     */
    constexpr uint8_t capacity() const {
        return SIZE - 1;  // One slot is always unused
    }

private:
    T _buffer[SIZE];
    volatile uint8_t _head;  // volatile: modified by producer, read by consumer
    volatile uint8_t _tail;  // volatile: modified by consumer, read by producer
};

#endif // RING_BUFFER_H
