#pragma once

#include <ostream>

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

struct Document {
    Document() = default;
    Document(int id, double relevance, int rating);

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

const double RELEVANCE_CMP_EPSILON = 1.0e-6;

struct CompareByRelevance {
    bool operator()(const Document& lhs, const Document& rhs) {
        if (std::abs(lhs.relevance - rhs.relevance) < RELEVANCE_CMP_EPSILON *
            std::max(std::abs(lhs.relevance), std::abs(rhs.relevance))) {
            return lhs.rating > rhs.rating;
        } else {
            return lhs.relevance > rhs.relevance;
        }
    }
};

std::ostream& operator<<(std::ostream& out, const Document& document);
