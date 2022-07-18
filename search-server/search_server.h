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
#include <future>
#include <list>
#include <thread>

#include "concurrent_map.h"

const int MAX_RESULT_DOCUMENT_COUNT = 5;

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

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const;

    template <typename Predicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, Predicate predicate) const;

    template <typename ExecutionPolicy, typename Predicate>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query, Predicate predicate) const;

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
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> documents_indexes_;
    mutable std::map<std::string, Query> query_cache_;

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
    std::vector<Document> FindAllDocuments(const Query& query,
                                           Predicate predicate) const;

    template<typename Predicate>
    std::vector<Document> FindAllDocuments(std::execution::parallel_policy,
                                           const Query& query,
                                           Predicate predicate) const;

    template<typename Predicate>
    std::vector<Document> FindAllDocuments(std::execution::sequenced_policy,
                                           const Query& query,
                                           Predicate predicate) const;
};

template<class StringContainer>
SearchServer::SearchServer(const StringContainer & container) {
    for (const auto & value : container) {
        SetStopWords(value);
    }
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query, DocumentStatus status) const {
    auto pred = [status](int, DocumentStatus predicate_status, int) {
        return predicate_status == status;
    };
    return FindTopDocuments(policy, raw_query, pred);
}

template <typename Predicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, Predicate predicate) const {
    return FindTopDocuments(std::execution::seq, raw_query, predicate);
}

template <typename ExecutionPolicy, typename Predicate>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query, Predicate predicate) const {
    constexpr bool is_par_exec = std::is_same_v<std::decay_t<ExecutionPolicy>, std::execution::parallel_policy>;
    const Query query = (is_par_exec ? ParseQueryWithCache(raw_query) :
                                       ParseQuery(raw_query));
    auto matched_documents = FindAllDocuments(policy, query, predicate);
    std::sort(policy, matched_documents.begin(), matched_documents.end(), CompareByRelevance());
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template<typename Predicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, Predicate predicate) const {
    return FindAllDocuments(std::execution::seq, query, predicate);
}

template<typename Predicate>
std::vector<Document> SearchServer::FindAllDocuments(std::execution::sequenced_policy, const Query& query, Predicate predicate) const {
    std::map<int, double> document_to_relevance;
    for (const std::string& word : query.plus_words) {
        auto it_word2doc = word_to_document_freqs_.find(word);
        if (it_word2doc != word_to_document_freqs_.end()) {
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : it_word2doc->second) {
                const DocumentData & doc_data = documents_.at(document_id);
                if ( predicate(document_id, doc_data.status, doc_data.rating) ) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }
    }

    for (const std::string& word : query.minus_words) {
        auto it_word2doc = word_to_document_freqs_.find(word);
        if (it_word2doc != word_to_document_freqs_.end()) {
            for (const auto [document_id, _] : it_word2doc->second) {
                document_to_relevance.erase(document_id);
            }
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

template<typename Predicate>
std::vector<Document> SearchServer::FindAllDocuments(std::execution::parallel_policy, const Query& query, Predicate predicate) const {
    const size_t max_threads = std::thread::hardware_concurrency();
    ConcurrentMap<int, double> document_to_relevance(max_threads);
    {
        auto check_plus_word = [this, &document_to_relevance, &predicate](const std::string& word) {
            auto it_word2doc = word_to_document_freqs_.find(word);
            if (it_word2doc != word_to_document_freqs_.end()) {
                const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                for (const auto [document_id, term_freq] : it_word2doc->second) {
                    const DocumentData & doc_data = documents_.at(document_id);
                    if ( predicate(document_id, doc_data.status, doc_data.rating) ) {
                        document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                    }
                }
            }
        };
        Futures futures;
        AsyncsRun(query.plus_words, check_plus_word, futures, max_threads);
        AsyncsWait(futures);
    }

    {
        auto check_minus_word = [this, &document_to_relevance](const std::string& word) {
            auto it_word2doc = word_to_document_freqs_.find(word);
            if (it_word2doc != word_to_document_freqs_.end()) {
                for (const auto [document_id, _] : it_word2doc->second) {
                    document_to_relevance.erase(document_id);
                }
            }
        };
        Futures futures;
        AsyncsRun(query.minus_words, check_minus_word, futures, max_threads);
        AsyncsWait(futures);
    }

    std::vector<Document> matched_documents;
    auto matched_documents_transform = [this, &matched_documents](const std::map<int, double> & map, size_t it) {
        for (const auto [document_id, relevance] : map) {
            matched_documents[it++] = Document{
                document_id,
                relevance,
                documents_.at(document_id).rating
            };
        }
    };
    matched_documents.resize(document_to_relevance.size());
    Futures futures;
    size_t ita, itb = 0;
    for (size_t i = 0; i < max_threads; ++i) {
        const auto & map = document_to_relevance.GetMap(i);
        ita = itb;
        itb = ita + map.size();
        futures.push_back( std::async(std::launch::async, matched_documents_transform, map, ita ) );
    }
    AsyncsWait(futures);
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
