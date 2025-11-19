#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <vector>

namespace ome {

// Arena allocator for fast bump-pointer allocation
// Provides O(1) allocation and batch deallocation
class Arena {
public:
    explicit Arena(size_t block_size = 16 * 1024 * 1024);  // Default 16MB
    ~Arena();
    
    // Non-copyable, non-movable
    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;
    
    // Allocate memory for type T
    template<typename T, typename... Args>
    T* allocate(Args&&... args) {
        void* ptr = allocate_bytes(sizeof(T), alignof(T));
        return new(ptr) T(std::forward<Args>(args)...);
    }
    
    // Allocate raw bytes with alignment
    void* allocate_bytes(size_t size, size_t alignment = alignof(std::max_align_t));
    
    // Reset arena - invalidates all previous allocations
    void reset();
    
    // Get statistics
    size_t bytes_allocated() const { return bytes_allocated_; }
    size_t bytes_used() const { return current_offset_; }
    size_t num_blocks() const { return blocks_.size(); }
    
private:
    struct Block {
        uint8_t* data;
        size_t size;
        
        Block(size_t s) : size(s) {
            data = static_cast<uint8_t*>(::operator new(s));
        }
        
        ~Block() {
            ::operator delete(data);
        }
    };
    
    std::vector<std::unique_ptr<Block>> blocks_;
    size_t block_size_;
    size_t current_offset_;
    size_t bytes_allocated_;
    
    void allocate_new_block(size_t min_size = 0);
    
    // Align pointer
    static size_t align_up(size_t n, size_t alignment) {
        return (n + alignment - 1) & ~(alignment - 1);
    }
};

// Pool allocator for fixed-size objects
// Maintains a free list for efficient reuse
template<typename T>
class Pool {
public:
    explicit Pool(size_t initial_capacity = 1024)
        : arena_(sizeof(T) * initial_capacity) {
        expand(initial_capacity);
    }
    
    // Allocate object from pool
    template<typename... Args>
    T* allocate(Args&&... args) {
        if (free_list_.empty()) {
            expand(capacity_);
            capacity_ *= 2;  // Exponential growth
        }
        
        T* ptr = free_list_.back();
        free_list_.pop_back();
        
        // Construct object
        new(ptr) T(std::forward<Args>(args)...);
        ++allocated_count_;
        
        return ptr;
    }
    
    // Return object to pool
    void deallocate(T* ptr) {
        if (ptr) {
            ptr->~T();  // Destroy object
            free_list_.push_back(ptr);
            --allocated_count_;
        }
    }
    
    // Statistics
    size_t allocated_count() const { return allocated_count_; }
    size_t free_count() const { return free_list_.size(); }
    
private:
    Arena arena_;
    std::vector<T*> free_list_;
    size_t capacity_ = 1024;
    size_t allocated_count_ = 0;
    
    void expand(size_t count) {
        for (size_t i = 0; i < count; ++i) {
            T* ptr = static_cast<T*>(arena_.allocate_bytes(sizeof(T), alignof(T)));
            free_list_.push_back(ptr);
        }
    }
};

} // namespace ome
