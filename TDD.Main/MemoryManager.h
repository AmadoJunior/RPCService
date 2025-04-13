#pragma once

#ifdef MEM_MANAGER_EXPORTS
#define MEM_MANAGER __declspec(dllexport)
#else
#define MEM_MANAGER __declspec(dllimport)
#endif

#include <memory>
#include <memory_resource>
#include <functional>

class MEM_MANAGER MemoryManager {
private:
    std::unique_ptr<std::byte[]> buffer_;
    std::pmr::monotonic_buffer_resource mbr_;
    std::pmr::synchronized_pool_resource pool_;

public:
    using CustomDeleter = std::function<void(std::pmr::memory_resource*)>;

    // Constructor with Size
    MemoryManager(size_t bufferSize);

    // Destructor
    ~MemoryManager() = default;

    std::pmr::memory_resource* getResource();
    std::unique_ptr<std::pmr::memory_resource, CustomDeleter> createClientResource(size_t clientBufferSize = 256 * 1024);

    // Delete Copy/Move Operations
    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;
    MemoryManager(MemoryManager&&) = delete;
    MemoryManager& operator=(MemoryManager&&) = delete;
};