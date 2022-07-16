#include "search_server.h"
#include "read_input_functions.h"
#include "string_processing.h"
#include <cmath>
#include <numeric>

using namespace std;

SearchServer::SearchServer(const string & stop_words) {
    SetStopWords(stop_words);
}

void SearchServer::SetStopWords(const string& text) {
    for (auto word : SplitIntoWords(text)) {
        if (HasSpecialSymbols(word)) {
            throw invalid_argument("SetStopWords: Invalid stop word='"s + string{word} + "'"s);
        }
        stop_words_.insert(string{word});
    }
}

void SearchServer::AddDocument(int document_id, string_view document, DocumentStatus status, const vector<int>& ratings) {
    if (document_id < 0) {
        throw invalid_argument("AddDocument: document_id < 0");
    }
    if (documents_.count(document_id) > 0) {
        throw invalid_argument("AddDocument: document_id=" + to_string(document_id) + " already exist");
    }
    const vector<string_view> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    auto & map_of_words_freq = document_to_word_freqs_[document_id];
    for (auto word : words) {
        auto it = word_to_document_freqs_.find(word);
        if (it == word_to_document_freqs_.end()) {
            auto it_out = word_to_document_freqs_.insert({string{word}, {}});
            if (it_out.second == false) {
                continue;
            } else {
                it = it_out.first;
            }
        }
        it->second[document_id] += inv_word_count;
        map_of_words_freq[it->first] += inv_word_count;
    }
    documents_.emplace(document_id,
        DocumentData{
            ComputeAverageRating(ratings),
            status
        });
    documents_indexes_.insert(document_id);
}

void SearchServer::RemoveDocument(int document_id) {
    RemoveDocument(std::execution::seq, document_id);
}

const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    const auto & doc_2_word_freqs_iter = document_to_word_freqs_.find(document_id);
    if (doc_2_word_freqs_iter == document_to_word_freqs_.end()) {
        static map<string_view, double> empty_map;
        return empty_map;
    }
    return doc_2_word_freqs_iter->second;
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(execution::seq, raw_query, status);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

tuple<vector<string_view>, DocumentStatus>
SearchServer::MatchDocument(string_view raw_query, int document_id) const {
    return MatchDocument(std::execution::seq, raw_query, document_id);
}

bool SearchServer::IsStopWord(string_view word) const {
    return stop_words_.count(word) > 0;
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(string_view text) const {
    vector<string_view> words;
    for (auto word : SplitIntoWords(text)) {
        if (HasSpecialSymbols(word)) {
            throw invalid_argument("SplitIntoWordsNoStop: invalid symbols='"s + string{text} + "'"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view text) const {
    if (text.empty()) {
        throw invalid_argument("ParseQueryWord: empty word"s);
    }
    if (HasSpecialSymbols(text)) {
        throw invalid_argument("ParseQueryWord: invalid symbols '"s + string{text} + "'"s);
    }
    QueryWord result;
    result.is_minus = false;
    result.is_stop = false;
    if (text[0] == '-') {
        result.is_minus = true;
        if (text.size() == 1) {
            throw invalid_argument("ParseQueryWord: empty minus word"s);
        } else {
            if (text[1] == '-') {
                throw invalid_argument("ParseQueryWord: minus word contents --"s);
            } else {
                result.data = string{text.substr(1)};
            }
        }
    }
    if (result.data.empty()) {
        result.data = string{text};
    }
    result.is_stop = IsStopWord(text);
    return result;
}

SearchServer::Query SearchServer::ParseQuery(string_view text) const {
    Query query;
    for (auto word : SplitIntoWords(text)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.insert(query_word.data);
            } else {
                query.plus_words.insert(query_word.data);
            }
        }
    }
    return query;
}

SearchServer::Query SearchServer::ParseQueryWithCache(string_view text) const {
    auto it_to_query_cache = query_cache_.find(string{text});
    if (it_to_query_cache != query_cache_.end()) {
        return it_to_query_cache->second;
    }
    Query query = ParseQuery(text);
    query_cache_[string{text}] = query;
    return query;
}

// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(const string& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

std::set<int>::const_iterator SearchServer::begin() const {
    return documents_indexes_.begin();
}

std::set<int>::const_iterator SearchServer::end() const {
    return documents_indexes_.end();
}


