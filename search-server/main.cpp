#include <iostream>
#include <string>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <numeric>

using namespace std;

// ////////////////////////////////////////////////////////////////////////////////////////// //
/* Подставьте вашу реализацию класса SearchServer сюда */
// ////////////////////////////////////////////////////////////////////////////////////////// //

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILLON = 1e-6;

struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

ostream& operator<<(ostream& os, DocumentStatus status) {
    switch(status) {
    case DocumentStatus::ACTUAL:     os << "ACTUAL"s;     break;
    case DocumentStatus::IRRELEVANT: os << "IRRELEVANT"s; break;
    case DocumentStatus::BANNED:     os << "BANNED"s;     break;
    case DocumentStatus::REMOVED:    os << "REMOVED"s;    break;
    }
    return os;
}

template<typename T>
static bool AreEqual(T f1, T f2) {
    return (std::fabs(f1 - f2) <= EPSILLON * std::fmax(std::fabs(f1), std::fabs(f2)));
}

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id,
            DocumentData{
                ComputeAverageRating(ratings),
                status
            });
    }

    template <typename Predicate>
    vector<Document> FindTopDocuments(const string& raw_query, Predicate predicate) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, predicate);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < EPSILLON) {
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

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        auto pred = [status](int, DocumentStatus predicate_status, int) {
            return predicate_status == status;
        };
        return FindTopDocuments(raw_query, pred);
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
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

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {
            text,
            is_minus,
            IsStopWord(text)
        };
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
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
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template<typename Predicate>
    vector<Document> FindAllDocuments(const Query& query, Predicate predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
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

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
            });
        }
        return matched_documents;
    }
};


// ////////////////////////////////////////////////////////////////////////////////////////// //
/*
   Подставьте сюда вашу реализацию макросов
   ASSERT, ASSERT_EQUAL, ASSERT_EQUAL_HINT, ASSERT_HINT и RUN_TEST
*/
// ////////////////////////////////////////////////////////////////////////////////////////// //

template <typename T>
void RunTestImpl(const T & func_name) {
    cerr << func_name << " OK" << endl;
}

#ifndef RUN_TEST
#define RUN_TEST(func) { func(); RunTestImpl(#func); }
#endif

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

template <>
void AssertEqualImpl(const double& t, const double& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (AreEqual(t, u) == false) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#ifndef ASSERT_EQUAL
#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#endif

#ifndef ASSERT_EQUAL_HINT
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))
#endif

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#ifndef ASSERT
#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#endif

#ifndef ASSERT_HINT
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))
#endif


// ////////////////////////////////////////////////////////////////////////////////////////// //
// -------- Начало модульных тестов поисковой системы ----------
// ////////////////////////////////////////////////////////////////////////////////////////// //

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }
}


// Добавление документов. Добавленный документ должен находиться по
// поисковому запросу, который содержит слова из документа
void TestAdditionOfDocuments() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    SearchServer server;
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    {
        const auto found_docs = server.FindTopDocuments("nothig"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 0, "Найдено несуществующее слово.");
    }
    {
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    {
        const auto found_docs = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    {
        const auto found_docs = server.FindTopDocuments("the"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    {
        const auto found_docs = server.FindTopDocuments("city"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    {
        const auto found_docs = server.FindTopDocuments("          cat from city         "s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
}

// Поддержка минус-слов. Документы, содержащие минус-слова поискового
// запроса, не должны включаться в результаты поиска.

void TestExcludeMinusWords() {
    constexpr int dog_index = 0;
    vector< int > doc_ids = {
        10,
        11,
    };
    vector< string > content = {
        "dog in the city"s,
        "cat in the city"s,
    };
    const vector<vector<int>> ratings = {
        {1, 2, 3},
        {4, 1, -3},
    };
    const DocumentStatus statuses[] = {
        DocumentStatus::ACTUAL,
        DocumentStatus::ACTUAL,
    };
    SearchServer server;
    for (size_t i = 0; i < doc_ids.size(); ++i) {
        server.AddDocument(doc_ids[i], content[i], statuses[i], ratings[i]);
    }
    {
        const auto found_docs = server.FindTopDocuments("dog city"s);
        ASSERT_EQUAL(found_docs.size(), 2);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_ids[dog_index]);
    }
    {
        const auto found_docs = server.FindTopDocuments("-cat city"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        ASSERT_EQUAL(found_docs[0].id, doc_ids[dog_index]);
    }
}

// Матчинг документов. При матчинге документа по поисковому запросу должны быть
// возвращены все слова из поискового запроса, присутствующие в документе. Если
// есть соответствие хотя бы по одному минус-слову, должен возвращаться пустой
// список слов.

void TestMatchingDocuments() {
    constexpr int dog_index = 0;
    constexpr int cat_index = 1;
    vector< int > doc_ids = {
        10,
        11,
    };
    vector< string > content = {
        "dog in the city"s,
        "cat in the city"s,
    };
    const vector<vector<int>> ratings = {
        {1, 2, 3},
        {4, 1, -3},
    };
    const DocumentStatus statuses[] = {
        DocumentStatus::ACTUAL,
        DocumentStatus::ACTUAL,
    };
    SearchServer server;
    for (size_t i = 0; i < doc_ids.size(); ++i) {
        server.AddDocument(doc_ids[i], content[i], statuses[i], ratings[i]);
    }
    {
        const string query = "dog city"s;
        const auto & [words, status] = server.MatchDocument(query, doc_ids[dog_index]);
        ASSERT_EQUAL(words.size(), 2);
        ASSERT_EQUAL(status, statuses[dog_index]);
        ASSERT(query.find(words[0]) != string::npos);
        ASSERT(query.find(words[1]) != string::npos);
    }
    {
        const string query = "cat city"s;
        const auto & [words, status] = server.MatchDocument(query, doc_ids[dog_index]);
        ASSERT_EQUAL(words.size(), 1);
        ASSERT_EQUAL(status, statuses[dog_index]);
        ASSERT(words[0] == "city"s);
    }
    {
        const string query = "-cat city"s;
        const auto & [words, status] = server.MatchDocument(query, doc_ids[cat_index]);
        ASSERT(words.empty());
    }
    {
        const string query = "-cat city"s;
        const auto & [words, status] = server.MatchDocument(query, doc_ids[dog_index]);
        ASSERT_EQUAL(words.size(), 1);
        ASSERT_EQUAL(status, statuses[dog_index]);
        ASSERT(query.find(words[0]) != string::npos);
    }
    {
        const string query = "cat city"s;
        const auto & [words, status] = server.MatchDocument(query, doc_ids[cat_index]);
        ASSERT_EQUAL(words.size(), 2);
        ASSERT_EQUAL(status, statuses[cat_index]);
        ASSERT(query.find(words[0]) != string::npos);
        ASSERT(query.find(words[1]) != string::npos);
    }
}

// Сортировка найденных документов по релевантности. Возвращаемые при поиске
// документов результаты должны быть отсортированы в порядке убывания релевантности.

void TestSortDocumentsByRelevancy() {
    vector< int > doc_ids = {
        10,
        11,
        22,
        33,
    };
    vector< string > content = {
        "detroit rock city"s,
        "paradise city in full of glory wisdom"s,
        "cat in the city"s,
        "dog in the city in great"s,
    };
    const vector<vector<int>> ratings = {
        {1, 2, 3},
        {3, 3, 3},
        {1, 1, 3},
        {4, 1, -3},
    };
    const DocumentStatus statuses[] = {
        DocumentStatus::ACTUAL,
        DocumentStatus::ACTUAL,
        DocumentStatus::ACTUAL,
        DocumentStatus::ACTUAL,
    };
    SearchServer server;
    for (size_t i = 0; i < doc_ids.size(); ++i) {
        server.AddDocument(doc_ids[i], content[i], statuses[i], ratings[i]);
    }
    {
        const auto found_docs = server.FindTopDocuments("in city"s);
        ASSERT_EQUAL(found_docs.size(), 4);
        ASSERT(found_docs[0].relevance >= found_docs[1].relevance);
        ASSERT(found_docs[1].relevance >= found_docs[2].relevance);
        ASSERT(found_docs[2].relevance >= found_docs[3].relevance);
    }
}

// Вычисление рейтинга документов. Рейтинг добавленного документа равен среднему
// арифметическому оценок документа.

void TestCalculationOfRating() {
    constexpr int dog_index = 0;
    constexpr int cat_index = 1;
    vector< int > doc_ids = {
        10,
        11,
    };
    vector< string > content = {
        "dog in the city in great"s,
        "cat in the city"s,
    };
    const vector<vector<int>> ratings = {
        {1, 2, 3},
        {4, 1, 3},
    };
    const DocumentStatus statuses[] = {
        DocumentStatus::ACTUAL,
        DocumentStatus::ACTUAL,
    };
    SearchServer server;
    for (size_t i = 0; i < doc_ids.size(); ++i) {
        server.AddDocument(doc_ids[i], content[i], statuses[i], ratings[i]);
    }
    {
        const auto found_docs = server.FindTopDocuments("in city"s);
        ASSERT_EQUAL(found_docs.size(), 2);
        ASSERT_EQUAL(found_docs[0].id, doc_ids[dog_index]);
        {
            const auto & v = ratings[dog_index];
            int avg_rating = accumulate(v.begin(), v.end(), 0) / v.size();
            ASSERT_EQUAL( found_docs[0].rating, avg_rating );
        }
        ASSERT_EQUAL(found_docs[1].id, doc_ids[cat_index]);
        {
            const auto & v = ratings[dog_index];
            int avg_rating = accumulate(v.begin(), v.end(), 0) / v.size();
            ASSERT_EQUAL( found_docs[1].rating, avg_rating );
        }
    }
}

// Фильтрация результатов поиска с использованием предиката,
// задаваемого пользователем.

void TestUsageDocumentsPredicate() {
    constexpr int dog_index = 0;
    constexpr int dog_banned_index = 1;
    vector< int > doc_ids = {
        10,
        11,
        12,
    };
    vector< string > content = {
        "dog in the city in great surface"s,
        "dog in the city"s,
        "cat in the city"s,
    };
    const vector<vector<int>> ratings = {
        {1, 2, 3},
        {4, 4, 3},
        {4, 1, 3},
    };
    const DocumentStatus statuses[] = {
        DocumentStatus::ACTUAL,
        DocumentStatus::BANNED,
        DocumentStatus::ACTUAL,
    };
    SearchServer server;
    for (size_t i = 0; i < doc_ids.size(); ++i) {
        server.AddDocument(doc_ids[i], content[i], statuses[i], ratings[i]);
    }
    {
        const auto found_docs = server.FindTopDocuments("dog"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        ASSERT_EQUAL(found_docs[0].id, doc_ids[dog_index]);
    }
    {
        const auto found_docs = server.FindTopDocuments("dog"s, [](int, DocumentStatus status, int) {
            return status == DocumentStatus::BANNED;
        });
        ASSERT_EQUAL(found_docs.size(), 1);
        ASSERT_EQUAL(found_docs[0].id, doc_ids[dog_banned_index]);
    }
}

// Поиск документов, имеющих заданный статус.

void TestFindByStatus() {
    constexpr int dog_index = 0;
    constexpr int dog_banned_index = 1;
    constexpr int cat_index = 2;
    constexpr int cat_irrelevant_index = 3;
    vector< int > doc_ids = {
        10,
        11,
        12,
        13,
    };
    vector< string > content = {
        "dog in the city in great"s,
        "dog in the city the great"s,
        "cat in the city"s,
        "irrelevant cat is not"s,
    };
    const vector<vector<int>> ratings = {
        {1, 2, 3},
        {4, 4, 3},
        {4, 1, 3},
        {1, 1, 2},
    };
    const DocumentStatus statuses[] = {
        DocumentStatus::ACTUAL,
        DocumentStatus::BANNED,
        DocumentStatus::ACTUAL,
        DocumentStatus::IRRELEVANT,
    };
    SearchServer server;
    for (size_t i = 0; i < doc_ids.size(); ++i) {
        server.AddDocument(doc_ids[i], content[i], statuses[i], ratings[i]);
    }
    {
        const auto found_docs = server.FindTopDocuments("dog"s, DocumentStatus::ACTUAL);
        ASSERT_EQUAL(found_docs.size(), 1);
        ASSERT_EQUAL(found_docs[0].id, doc_ids[dog_index]);
    }
    {
        const auto found_docs = server.FindTopDocuments("dog"s, DocumentStatus::BANNED);
        ASSERT_EQUAL(found_docs.size(), 1);
        ASSERT_EQUAL(found_docs[0].id, doc_ids[dog_banned_index]);
    }
    {
        const auto found_docs = server.FindTopDocuments("cat"s, DocumentStatus::ACTUAL);
        ASSERT_EQUAL(found_docs.size(), 1);
        ASSERT_EQUAL(found_docs[0].id, doc_ids[cat_index]);
    }
    {
        const auto found_docs = server.FindTopDocuments("cat"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(found_docs.size(), 1);
        ASSERT_EQUAL(found_docs[0].id, doc_ids[cat_irrelevant_index]);
    }
}

// Корректное вычисление релевантности найденных документов.

void TestCalculationDocumentsRelevancy() {
    constexpr int dog_0_index = 0;
    constexpr int dog_1_index = 1;
    constexpr int dog_2_index = 2;
    constexpr int dog_3_index = 3;
    vector< int > doc_ids = {
        10,
        11,
        12,
        13,
    };
    vector< string > content = {
        "irrelevant dog is not"s,
        "dog in the city not the great idea"s,
        "dog in the city in great"s,
        "dog in the city"s,
    };
    const vector<vector<int>> ratings = {
        {1, 2, 3},
        {4, 4, 3},
        {4, 1, 3},
        {1, 1, 2},
    };
    const double relevances[] = {
        0.17328679513998632,
        0.08664339756999316,
        0.0,
        0.0,
    };

    SearchServer server;
    for (size_t i = 0; i < doc_ids.size(); ++i) {
        server.AddDocument(doc_ids[i], content[i], DocumentStatus::ACTUAL, ratings[i]);
    }
    {
        const string query = "dog not"s;
        const auto found_docs = server.FindTopDocuments(query);
        ASSERT_EQUAL(found_docs.size(), 4);
        ASSERT_EQUAL(found_docs[3].relevance, relevances[dog_3_index]);
        ASSERT_EQUAL(found_docs[2].relevance, relevances[dog_2_index]);
        ASSERT_EQUAL(found_docs[1].relevance, relevances[dog_1_index]);
        ASSERT_EQUAL(found_docs[0].relevance, relevances[dog_0_index]);
    }
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAdditionOfDocuments);
    RUN_TEST(TestExcludeMinusWords);
    RUN_TEST(TestMatchingDocuments);
    RUN_TEST(TestSortDocumentsByRelevancy);
    RUN_TEST(TestCalculationOfRating);
    RUN_TEST(TestUsageDocumentsPredicate);
    RUN_TEST(TestFindByStatus);
    RUN_TEST(TestCalculationDocumentsRelevancy);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}
