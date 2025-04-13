#pragma once

#ifdef MEM_MANAGER_EXPORTS
#define MEM_MANAGER __declspec(dllexport)
#else
#define MEM_MANAGER __declspec(dllimport)
#endif

#include <memory>
#include <memory_resource>
#include <functional>

class MEM_MANAGER BumpMemoryManager {
private:
    std::unique_ptr<std::byte[]> buffer_;
    std::pmr::monotonic_buffer_resource mbr_;
    std::pmr::synchronized_pool_resource pool_;

public:
    using CustomDeleter = std::function<void(std::pmr::memory_resource*)>;

    // Constructor with Size
    BumpMemoryManager(size_t bufferSize);

    // Destructor
    ~BumpMemoryManager() = default;

    std::pmr::memory_resource* getResource();
    std::unique_ptr<std::pmr::memory_resource, CustomDeleter> createClientResource(size_t clientBufferSize = 256 * 1024, bool synchronizedPool = false);

    // Delete Copy/Move Operations
    BumpMemoryManager(const BumpMemoryManager&) = delete;
    BumpMemoryManager& operator=(const BumpMemoryManager&) = delete;
    BumpMemoryManager(BumpMemoryManager&&) = delete;
    BumpMemoryManager& operator=(BumpMemoryManager&&) = delete;
};