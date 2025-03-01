#include "msgpack/msgpack.hpp"

struct Example {
    std::map<std::string, bool> map;

    template<class T>
    void pack(T& pack) {
        pack(map);
    }
};