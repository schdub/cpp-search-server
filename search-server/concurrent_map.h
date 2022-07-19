#pragma once

#include <map>
#include <string>
#include <vector>
#include <cassert>
#include <list>
#include <future>
#include <algorithm>

using namespace std::string_literals;
using Futures = std::list<std::future<void>>;

template<typename Container>
void AsyncsWait(Container & futures) {
    for (auto & f : futures) f.wait();
}

template<typename Container, typename Function>
void AsyncsRun(const Container & container, Function function, Futures & futures, size_t max_threads) {
    if (container.size() == 0) return;
    using Iterator = typename Container::const_iterator;
    max_threads = std::min(max_threads, container.size());
    size_t chunk_size = container.size() / max_threads;
    Iterator ita;
    Iterator itb = container.begin();
    for (size_t i = 0; i < max_threads-1; ++i) {
        ita = itb; itb = std::next(itb, chunk_size);
        futures.emplace_back( std::async([ita, itb, function]() {
            std::for_each(ita, itb, function);
        } ));
    }
    ita = itb; itb = container.end();
    futures.emplace_back( std::async([ita, itb, function]() {
        std::for_each(ita, itb, function);
    } ));
}

template <typename Key, typename Value>
class ConcurrentMap {
private:
    struct Bucket {
        std::mutex mutex;
        std::map<Key, Value> map;
    };
 
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);
 
    struct Access {
        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;
 
        Access(const Key& key, Bucket& bucket)
            : guard(bucket.mutex)
            , ref_to_value(bucket.map[key]) {
        }
    };
 
    explicit ConcurrentMap(size_t bucket_count)
        : buckets_(bucket_count) {
    }
 
    Access operator[](const Key& key) {
        auto& bucket = buckets_[static_cast<uint64_t>(key) % buckets_.size()];
        return {key, bucket};
    }

    const std::map<Key, Value> & GetMap(size_t index) const {
        return buckets_[index].map;
    }
 
    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;
        for (auto& [mutex, map] : buckets_) {
            std::lock_guard g(mutex);
            result.insert(map.begin(), map.end());
        }
        return result;
    }

    size_t size() const {
        size_t size = 0;
        for (size_t i = 0; i < buckets_.size(); ++i) {
            size += buckets_[i].map.size();
        }
        return size;
    }

    void erase(const Key key) {
        size_t bucket_index = (size_t)((uint64_t) key % buckets_.size());
        auto & bucket = buckets_[bucket_index];
        std::lock_guard lock(bucket.mutex);
        bucket.map.erase(key);
    }
 
private:
    std::vector<Bucket> buckets_;
};


