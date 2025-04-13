#pragma once
#include <memory_resource>

template<typename T>
class PMRDeleter {
private:
    std::pmr::memory_resource* resource_;

public:
    explicit PMRDeleter(std::pmr::memory_resource* resource)
        : resource_(resource) {
    }

    void operator()(T* ptr) const {
        if (ptr) {
            // Call destructor
            ptr->~T();

            // Deallocate memory
            resource_->deallocate(ptr, sizeof(T), alignof(T));
        }
    }
};

template<typename T, typename... Args>
std::unique_ptr<T, PMRDeleter<T>> make_pmr_unique_ptr(std::pmr::memory_resource* resource, Args&&... args) {
    // Allocate
    void* memory = resource->allocate(sizeof(T), alignof(T));

    try {
        // Construct Object
        T* object = new (memory) T(std::forward<Args>(args)...);

        // Return unique_ptr with Custom Deleter
        return std::unique_ptr<T, PMRDeleter<T>>(object, PMRDeleter<T>(resource));
    }
    catch (...) {
        // Deallocate Memory
        resource->deallocate(memory, sizeof(T), alignof(T));
        throw;
    }
}

template<typename T, typename... Args>
std::shared_ptr<T> make_pmr_shared_ptr(std::pmr::memory_resource* resource, Args&&... args) {
    // Allocate
    void* memory = resource->allocate(sizeof(T), alignof(T));

    try {
        // Construct Object
        T* object = new (memory) T(std::forward<Args>(args)...);

        // Create shared_ptr with Custom Deleter
        return std::shared_ptr<T>(
            object,
            [resource](T* ptr) {
                if (ptr) {
                    ptr->~T();
                    resource->deallocate(ptr, sizeof(T), alignof(T));
                }
            }
        );
    }
    catch (...) {
        // Deallocate Memory
        resource->deallocate(memory, sizeof(T), alignof(T));
        throw;
    }
}