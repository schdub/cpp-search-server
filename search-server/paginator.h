#pragma once
#include <vector>
#include <ostream>

template <class Iter>
class IteratorRange {
public:
    IteratorRange() = default;
    IteratorRange(Iter first, Iter last, size_t size)
        : first_(first), last_(last), size_(size)
    {}

    size_t size() const {
        return size_;
    }

    auto begin() const {
        return first_;
    }

    auto end() const {
        return last_;
    }

private:
    /*typename vector<IteratorRange>::iterator*/
    Iter first_;
    Iter last_;
    size_t size_;
};

template <typename Iter>
class Paginator {
public:

    Paginator(Iter first, Iter last, size_t size) {
        if (size > 0) {
            size_t total =  distance(first, last);
            size_t cur = 0;
            Iter beg = first;
            for (Iter it = first;;) {
                if (it == last) {
                    if (cur != 0) {
                        iterators_.push_back(IteratorRange(beg, it, cur));
                    }
                    break;
                }
                if (++cur != size) {
                    ++it;
                } else {
                    iterators_.push_back(IteratorRange(beg, ++it, size));
                    beg = it;
                    cur = 0;
                }
                --total;
            }
        }
    }

    auto begin() const {
        return iterators_.begin();
    }

    auto end() const {
        return iterators_.end();
    }

private:
    std::vector<IteratorRange<Iter>> iterators_;
};

template <typename Iterator>
std::ostream& operator<<(std::ostream& out, const IteratorRange<Iterator>& range) {
    for (Iterator it = range.begin(); it != range.end(); ++it) {
        out << *it;
    }
    return out;
}

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(std::begin(c), std::end(c), page_size);
}