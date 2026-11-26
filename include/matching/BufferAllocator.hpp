#pragma once

#include <memory>
#include <stdexcept>
#include <vector>

template <size_t chunk_size, size_t capacity>
class BufferMemoryPool {
   public:
    static const size_t CHUNK_SIZE = chunk_size;

    BufferMemoryPool() {
        buffer_ = std::vector<std::byte>(capacity);
        for (int i = 0; i < capacity; i += chunk_size) {
            pool_.push_back(&buffer_[i]);
        }
    }

    void* malloc() {
        if (pool_.empty()) {
            throw std::runtime_error("out of memory");
        }
        auto ret = pool_.back();
        pool_.pop_back();
        return static_cast<void*>(ret);
    }
    void free(void* t) {
        auto p = static_cast<std::byte*>(t);
        pool_.push_back(p);
    }

    std::vector<std::byte> buffer_;
    std::vector<std::byte*> pool_;
};

template <typename T, typename MemoryPool>
class BufferAllocator {
   public:
    explicit BufferAllocator(MemoryPool* pool) : pool_(pool) {}

    template <typename U>
    BufferAllocator(const BufferAllocator<U, MemoryPool>& other) noexcept
        : pool_(other.pool_) {}

    BufferAllocator() noexcept = delete;

    using value_type = T;

    template <typename U>
    struct rebind {
        using other = BufferAllocator<U, MemoryPool>;
    };

    T* allocate(std::size_t n) {
        if (n != 1)
            throw std::runtime_error("not allow multiple obj allocation");
        static_assert(sizeof(T) <= MemoryPool::CHUNK_SIZE);
        return reinterpret_cast<T*>(pool_->malloc());
    }

    void deallocate(T* p, std::size_t n) noexcept { pool_->free(p); }

    template <typename U>
    bool operator==(const BufferAllocator<U, MemoryPool>& other) const noexcept {
        return pool_ == other.pool_;
    }

    template <typename U>
    bool operator!=(const BufferAllocator<U, MemoryPool>& other) const noexcept {
        return pool_ != other.pool_;
    }

    MemoryPool* pool_;
};

template <typename Alloc>
concept CustomAlloc =
    requires(Alloc a, typename std::allocator_traits<Alloc>::pointer p,
             typename std::allocator_traits<Alloc>::size_type n) {
        typename Alloc::value_type;
        {
            std::allocator_traits<Alloc>::allocate(a, n)
        } -> std::same_as<typename std::allocator_traits<Alloc>::pointer>;
        { std::allocator_traits<Alloc>::deallocate(a, p, n) } noexcept;
    };

static_assert(CustomAlloc<BufferAllocator<int, BufferMemoryPool<1, 1>>>,
              "PoolAllocator does not satisfy CppAllocator!");
