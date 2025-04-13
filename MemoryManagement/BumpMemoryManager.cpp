#include "BumpMemoryManager.h"

BumpMemoryManager::BumpMemoryManager(size_t bufferSize)
    : buffer_(std::make_unique<std::byte[]>(bufferSize)),
    mbr_(buffer_.get(), bufferSize),
    pool_(&mbr_)
{

}

std::pmr::memory_resource* BumpMemoryManager::getResource() {
    return &pool_;
}

std::unique_ptr<std::pmr::memory_resource, BumpMemoryManager::CustomDeleter> BumpMemoryManager::createClientResource(size_t clientBufferSize) {
    // Allocate Client Buffer from Main Pool
    std::byte* rawBuffer = new std::byte[clientBufferSize];

    // MBR
    auto clientBufferPtr = new std::pmr::monotonic_buffer_resource(
        rawBuffer,
        clientBufferSize
    );

    // Create Pool Resource
    auto poolResource = new std::pmr::unsynchronized_pool_resource(clientBufferPtr);

    // Deleter
    BumpMemoryManager::CustomDeleter deleter =
        [rawBuffer, clientBufferPtr](std::pmr::memory_resource* resource) {
        // Release Pool
        auto* pool = static_cast<std::pmr::unsynchronized_pool_resource*>(resource);
        pool->release();
        delete pool;

        // Release Monotonic Buffer
        clientBufferPtr->release();
        delete clientBufferPtr;

        // Delete Raw Buffer
        delete[] rawBuffer;
        };

    return std::unique_ptr<std::pmr::memory_resource, BumpMemoryManager::CustomDeleter>(
        static_cast<std::pmr::memory_resource*>(poolResource),
        std::move(deleter)
    );
}