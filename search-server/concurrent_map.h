#pragma once

#include <map>
#include <string>
#include <vector>
#include <cassert>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct Access {
        Value& ref_to_value;
        std::mutex& ref_mutex;
        Access(Value& value, std::mutex& mutex)
            : ref_to_value(value)
            , ref_mutex(mutex)
        {}
        ~Access() { ref_mutex.unlock(); }
    };

    explicit ConcurrentMap(size_t bucket_count) {
        buckets_.resize( bucket_count );
        mutexes_.resize( bucket_count );
    }

    Access operator[](const Key& key) {
        size_t bucket_index = (size_t)((uint64_t) key % buckets_.size());
        std::mutex & m = mutexes_[bucket_index];
        m.lock();
        auto & map = buckets_[bucket_index];
        auto & value = map[key];
        return Access{value, m};
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        assert( buckets_.size() == mutexes_.size() );
        std::map<Key, Value> out;
        for (size_t index = 0; index < buckets_.size(); ++index) {
            std::lock_guard lock(mutexes_[index]);
            for (const auto & [k, v] : buckets_[index]) {
                out[k] = v;
            }
        }
        return out;
    }

    void erase(const Key & key) {
        size_t bucket_index = (size_t)((uint64_t) key % buckets_.size());
        std::mutex & m = mutexes_[bucket_index];
        m.lock();
        auto & map = buckets_[bucket_index];
        map.erase(key);
    }

private:
    struct mutex_wrapper : std::mutex {
        mutex_wrapper() = default;
        mutex_wrapper(mutex_wrapper const&) noexcept : std::mutex() {}
        bool operator==(mutex_wrapper const&other) noexcept { return this==&other; }
    };
    std::vector<std::map<Key,Value>> buckets_;
    std::vector<mutex_wrapper> mutexes_;
};