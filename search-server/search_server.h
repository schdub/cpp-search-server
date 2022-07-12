#pragma once
#include "string_processing.h"
#include "document.h"

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <string>
#include <vector>
#include <tuple>
#include <set>
#include <map>
#include <execution>
#include <typeinfo>

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double RELEVANCE_CMP_EPSILON = 1.0e-6;

class SearchServer {
public:
    SearchServer() = default;

    explicit SearchServer(const std::string & stop_words);

    template<class StringContainer>
    SearchServer(const StringContainer & container);
    void SetStopWords(const std::string& text);

    void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    void RemoveDocument(int document_id);

    template <typename ExecutionPolicy>
    void RemoveDocument(ExecutionPolicy && policy, int document_id);

    const std::map<std::string_view, double>&
    GetWordFrequencies(int document_id) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const;

    template <typename Predicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, Predicate predicate) const;

    int GetDocumentCount() const;

    std::tuple<std::vector<std::string_view>, DocumentStatus>
    MatchDocument(std::string_view raw_query, int document_id) const;

    template <typename ExecutionPolicy>
    std::tuple<std::vector<std::string_view>, DocumentStatus>
    MatchDocument(ExecutionPolicy && policy, std::string_view raw_query, int document_id) const;

    std::set<int>::const_iterator begin() const;
    std::set<int>::const_iterator end() const;

private:
    struct DocumentData {
        int rating = 0;
        DocumentStatus status = DocumentStatus::ACTUAL;
    };

    struct Query {
        std::set<std::string, std::less<>> plus_words;
        std::set<std::string, std::less<>> minus_words;
    };

    std::set<std::string, std::less<>> stop_words_;
    std::map<std::string, std::map<int, double>, std::less<>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> documents_indexes_;
    mutable std::map<std::string, Query, std::less<>> query_cache_;

    bool IsStopWord(std::string_view word) const;

    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string_view text) const;

    Query ParseQuery(std::string_view text) const;

    Query ParseQueryWithCache(std::string_view text) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(const std::string& word) const;

    template<typename Predicate>
    std::vector<Document> FindAllDocuments(const Query& query, Predicate predicate) const;
};

template<class StringContainer>
SearchServer::SearchServer(const StringContainer & container) {
    for (const auto & value : container) {
        SetStopWords(value);
    }
}

template <typename Predicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, Predicate predicate) const {
    const Query query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(query, predicate);

    sort(matched_documents.begin(), matched_documents.end(),
         [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < RELEVANCE_CMP_EPSILON *
                std::max(std::abs(lhs.relevance), std::abs(rhs.relevance))) {
                return lhs.rating > rhs.rating;
            } else {
                return lhs.relevance > rhs.relevance;
            }
         });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template<typename Predicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, Predicate predicate) const {
    std::map<int, double> document_to_relevance;
    for (const std::string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const DocumentData & doc_data = documents_.at(document_id);
            if ( predicate(document_id, doc_data.status, doc_data.rating) ) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (const std::string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({
            document_id,
            relevance,
            documents_.at(document_id).rating
        });
    }
    return matched_documents;
}

template <typename ExecutionPolicy>
void SearchServer::RemoveDocument(ExecutionPolicy && policy, int document_id) {
    auto iterator = document_to_word_freqs_.find(document_id);
    if (iterator == document_to_word_freqs_.end()) {
        return;
    }
    const auto & m = iterator->second;
    std::vector<std::string> to_delete;
    to_delete.reserve(m.size());
    for (const auto & [word, _] : m) {
        to_delete.emplace_back(word);
    }
    std::for_each(policy,
        to_delete.begin(), to_delete.end(),
        [document_id, this](const std::string & word) {
            word_to_document_freqs_[word].erase(document_id);
        }
    );
    document_to_word_freqs_.erase(document_id);
    documents_indexes_.erase(document_id);
    documents_.erase(document_id);
}

template <typename ExecutionPolicy>
std::tuple<std::vector<std::string_view>, DocumentStatus>
SearchServer::MatchDocument(ExecutionPolicy && policy, std::string_view raw_query, int document_id) const {
    auto it_to_documents_data = documents_.find(document_id);
    if (it_to_documents_data == documents_.end()) {
        throw std::out_of_range("MatchDocument: document_id "
            + std::to_string(document_id) + " not found.");
    }
    Query query;
    if (typeid(policy) == typeid(std::execution::parallel_policy)) {
        query = ParseQueryWithCache(raw_query);
    } else {
        query = ParseQuery(raw_query);
    }
    for (const std::string & minus_word : query.minus_words) {
        auto it_to_set = word_to_document_freqs_.find(minus_word);
        if (it_to_set == word_to_document_freqs_.end() || 
            it_to_set->second.count(document_id)) {
            return { {}, it_to_documents_data->second.status };
        }
    }
    std::vector<std::string_view> matched_words;
    matched_words.reserve(query.plus_words.size());
    for (const std::string & plus_word : query.plus_words) {
        auto it_to_set = word_to_document_freqs_.find(plus_word);
        if (it_to_set != word_to_document_freqs_.end() &&
            it_to_set->second.count(document_id)) {
            matched_words.push_back(it_to_set->first);
        }
    }
    return { matched_words, it_to_documents_data->second.status };
}
