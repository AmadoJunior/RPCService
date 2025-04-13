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

std::unique_ptr<std::pmr::memory_resource, BumpMemoryManager::CustomDeleter> BumpMemoryManager::createClientResource(size_t clientBufferSize, bool synchronizedPool) {
    // Allocate Client Buffer from Main Pool
    std::byte* rawBuffer = new std::byte[clientBufferSize];

    // MBR
    auto clientBufferPtr = new std::pmr::monotonic_buffer_resource(
        rawBuffer,
        clientBufferSize
    );

    // Create Pool Resource
    std::pmr::memory_resource* poolResource;
    if (synchronizedPool) {
        poolResource = new std::pmr::synchronized_pool_resource(clientBufferPtr);
    }
    else {
        poolResource = new std::pmr::unsynchronized_pool_resource(clientBufferPtr);
    }

    // Deleter
    BumpMemoryManager::CustomDeleter deleter =
        [rawBuffer, clientBufferPtr, synchronizedPool](std::pmr::memory_resource* resource) {
        // Release Pool
        if (synchronizedPool) {
            auto* pool = static_cast<std::pmr::synchronized_pool_resource*>(resource);
            pool->release();
            delete pool;
        }
        else {
            auto* pool = static_cast<std::pmr::unsynchronized_pool_resource*>(resource);
            pool->release();
            delete pool;
        }

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