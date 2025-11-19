#include "allocator.h"
#include <stdexcept>
#include <cstring>

namespace ome {

Arena::Arena(size_t block_size)
    : block_size_(block_size)
    , current_offset_(0)
    , bytes_allocated_(0) {
    
    if (block_size_ < 1024) {
        throw std::invalid_argument("Block size must be at least 1KB");
    }
    
    allocate_new_block();
}

Arena::~Arena() {
    // Blocks automatically cleaned up by unique_ptr
}

void Arena::allocate_new_block(size_t min_size) {
    size_t size = std::max(block_size_, min_size);
    blocks_.push_back(std::make_unique<Block>(size));
    current_offset_ = 0;
    bytes_allocated_ += size;
}

void* Arena::allocate_bytes(size_t size, size_t alignment) {
    if (size == 0) {
        return nullptr;
    }
    
    if (blocks_.empty()) {
        allocate_new_block();
    }
    
    Block* current_block = blocks_.back().get();
    
    // Align current offset
    size_t aligned_offset = align_up(current_offset_, alignment);
    
    // Check if allocation fits in current block
    if (aligned_offset + size > current_block->size) {
        // Need new block
        if (size > block_size_) {
            // Allocate oversized block for this allocation
            allocate_new_block(size + alignment);
            current_block = blocks_.back().get();
            aligned_offset = 0;
        } else {
            allocate_new_block();
            current_block = blocks_.back().get();
            aligned_offset = align_up(current_offset_, alignment);
        }
    }
    
    void* ptr = current_block->data + aligned_offset;
    current_offset_ = aligned_offset + size;
    
    return ptr;
}

void Arena::reset() {
    // Keep first block, delete others
    if (blocks_.size() > 1) {
        auto first_block = std::move(blocks_[0]);
        blocks_.clear();
        blocks_.push_back(std::move(first_block));
        bytes_allocated_ = blocks_[0]->size;
    }
    
    current_offset_ = 0;
}

} // namespace ome
