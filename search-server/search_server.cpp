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
    for (const string& word : SplitIntoWords(text)) {
        if (HasSpecialSymbols(word)) {
            throw invalid_argument("SetStopWords: Invalid stop word='" + word + "'");
        }
        stop_words_.insert(word);
    }
}

void SearchServer::AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
    if (document_id < 0) {
        throw invalid_argument("AddDocument: document_id < 0");
    }
    if (documents_.count(document_id) > 0) {
        throw invalid_argument("AddDocument: document_id=" + to_string(document_id) + " already exist");
    }
    const vector<string> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    auto & map_of_words_freq = document_to_word_freqs_[document_id];
    for (const string& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        map_of_words_freq[word] += inv_word_count;
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

const map<string, double>& SearchServer::GetWordFrequencies(int document_id) const {
    const auto & doc_2_word_freqs_iter = document_to_word_freqs_.find(document_id);
    if (doc_2_word_freqs_iter == document_to_word_freqs_.end()) {
        static map<string, double> empty_map;
        return empty_map;
    }
    return doc_2_word_freqs_iter->second;
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentStatus status) const {
    auto pred = [status](int, DocumentStatus predicate_status, int) {
        return predicate_status == status;
    };
    return FindTopDocuments(raw_query, pred);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const string& raw_query, int document_id) const {
    const Query query = ParseQuery(raw_query);
    vector<string> matched_words;
    for (const string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }
    return {matched_words, documents_.at(document_id).status};
}

bool SearchServer::IsStopWord(const string& word) const {
    return stop_words_.count(word) > 0;
}

vector<string> SearchServer::SplitIntoWordsNoStop(const string& text) const {
    vector<string> words;
    for (const string& word : SplitIntoWords(text)) {
        if (HasSpecialSymbols(word)) {
            throw invalid_argument("SplitIntoWordsNoStop: invalid symbols='" + text + "'");
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

SearchServer::QueryWord SearchServer::ParseQueryWord(string text) const {
    if (text.empty()) {
        throw invalid_argument("ParseQueryWord: empty word");
    }

    if (HasSpecialSymbols(text)) {
        throw invalid_argument("ParseQueryWord: invalid symbols '" + text + "'");
    }

    QueryWord result;
    result.is_minus = false;
    result.is_stop = false;
    if (text[0] == '-') {
        result.is_minus = true;
        if (text.size() == 1) {
            throw invalid_argument("ParseQueryWord: empty minus word");
        } else {
            if (text[1] == '-') {
                throw invalid_argument("ParseQueryWord: minus word contents --");
            } else {
                text = text.substr(1);
            }
        }
    }
    result.data = text;
    result.is_stop = IsStopWord(text);
    return result;
}

SearchServer::Query SearchServer::ParseQuery(const string& text) const {
    Query query;
    for (const string& word : SplitIntoWords(text)) {
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


