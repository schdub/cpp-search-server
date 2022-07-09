#include "test_example_functions.h"

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

#include "document.h"
#include "search_server.h"
#include "paginator.h"
#include "request_queue.h"
#include "remove_duplicates.h"
#include "log_duration.h"
#include "process_queries.h"

using namespace std;

std::ostream& operator<<(std::ostream& os, DocumentStatus status) {
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
    return (std::fabs(f1 - f2) <= 1e-6 * std::fmax(std::fabs(f1), std::fabs(f2)));
}

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
        ASSERT_EQUAL(found_docs.size(), 1u);
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
        ASSERT_EQUAL_HINT(found_docs.size(), 0u, "Найдено несуществующее слово.");
    }
    {
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    {
        const auto found_docs = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    {
        const auto found_docs = server.FindTopDocuments("the"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    {
        const auto found_docs = server.FindTopDocuments("city"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    {
        const auto found_docs = server.FindTopDocuments("          cat from city         "s);
        ASSERT_EQUAL(found_docs.size(), 1u);
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
        ASSERT_EQUAL(found_docs.size(), 2u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_ids[dog_index]);
    }
    {
        const auto found_docs = server.FindTopDocuments("-cat city"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
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
        ASSERT_EQUAL(words.size(), 2u);
        ASSERT_EQUAL(status, statuses[dog_index]);
        ASSERT(query.find(words[0]) != string::npos);
        ASSERT(query.find(words[1]) != string::npos);
    }
    {
        const string query = "cat city"s;
        const auto & [words, status] = server.MatchDocument(query, doc_ids[dog_index]);
        ASSERT_EQUAL(words.size(), 1u);
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
        ASSERT_EQUAL(words.size(), 1u);
        ASSERT_EQUAL(status, statuses[dog_index]);
        ASSERT(query.find(words[0]) != string::npos);
    }
    {
        const string query = "cat city"s;
        const auto & [words, status] = server.MatchDocument(query, doc_ids[cat_index]);
        ASSERT_EQUAL(words.size(), 2u);
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
        ASSERT_EQUAL(found_docs.size(), 4u);
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
        ASSERT_EQUAL(found_docs.size(), 2u);
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
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, doc_ids[dog_index]);
    }
    {
        const auto found_docs = server.FindTopDocuments("dog"s, [](int, DocumentStatus status, int) {
            return status == DocumentStatus::BANNED;
        });
        ASSERT_EQUAL(found_docs.size(), 1u);
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
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, doc_ids[dog_index]);
    }
    {
        const auto found_docs = server.FindTopDocuments("dog"s, DocumentStatus::BANNED);
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, doc_ids[dog_banned_index]);
    }
    {
        const auto found_docs = server.FindTopDocuments("cat"s, DocumentStatus::ACTUAL);
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, doc_ids[cat_index]);
    }
    {
        const auto found_docs = server.FindTopDocuments("cat"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(found_docs.size(), 1u);
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
        ASSERT_EQUAL(found_docs.size(), 4u);
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

void TestProcessQueries() {
    SearchServer search_server("and with"s);

    int id = 0;
    for (
        const string& text : {
            "funny pet and nasty rat"s,
            "funny pet with curly hair"s,
            "funny pet and not very nasty rat"s,
            "pet with rat and rat and rat"s,
            "nasty rat with curly hair"s,
        }
    ) {
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
    }

    const vector<string> queries = {
        "nasty rat -not"s,
        "not very funny nasty pet"s,
        "curly hair"s
    };
    id = 0;
    for (
        const auto& documents : ProcessQueries(search_server, queries)
    ) {
        cout << documents.size() << " documents for query ["s << queries[id++] << "]"s << endl;
    }
}

void TestProcessQueriesJoined() {
    SearchServer search_server("and with"s);

    int id = 0;
    for (
        const string& text : {
            "funny pet and nasty rat"s,
            "funny pet with curly hair"s,
            "funny pet and not very nasty rat"s,
            "pet with rat and rat and rat"s,
            "nasty rat with curly hair"s,
        }
    ) {
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
    }

    const vector<string> queries = {
        "nasty rat -not"s,
        "not very funny nasty pet"s,
        "curly hair"s
    };
    for (const Document& document : ProcessQueriesJoined(search_server, queries)) {
        cout << "Document "s << document.id << " matched with relevance "s << document.relevance << endl;
    }
}


void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s << endl;
}

void PrintMatchDocumentResult(int document_id, const vector<string>& words, DocumentStatus status) {
    cout << "{ "s
         << "document_id = "s << document_id << ", "s
         << "status = "s << static_cast<int>(status) << ", "s
         << "words ="s;
    for (const string& word : words) {
        cout << ' ' << word;
    }
    cout << "}"s << endl;
}

void AddDocument(SearchServer& search_server, int document_id, const string& document, DocumentStatus status,
                 const vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    } catch (const exception& e) {
        cout << "Ошибка добавления документа "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const string& raw_query) {
    cout << "Результаты поиска по запросу: "s << raw_query << endl;
    try {
    	LOG_DURATION_STREAM("Operation time", std::cout);
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    } catch (const exception& e) {
        cout << "Ошибка поиска: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer& search_server, const string& query) {
    try {
        cout << "Матчинг документов по запросу: "s << query << endl;
        for (const int document_id : search_server) {
            LOG_DURATION_STREAM("Operation time", std::cout);
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    } catch (const exception& e) {
        cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << endl;
    }
}

void AllTests() {
    {
        TestSearchServer();
    }

    cout << "//////////////////////////////////////////////////////////////" << endl;

    {
    SearchServer search_server("и в на"s);

    AddDocument(search_server, 1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    AddDocument(search_server, 1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2});
    AddDocument(search_server, -1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2});
    AddDocument(search_server, 3, "большой пёс скво\x12рец евгений"s, DocumentStatus::ACTUAL, {1, 3, 2});
    AddDocument(search_server, 4, "большой пёс скворец евгений"s, DocumentStatus::ACTUAL, {1, 1, 1});

    FindTopDocuments(search_server, "пушистый -пёс"s);
    FindTopDocuments(search_server, "пушистый --кот"s);
    FindTopDocuments(search_server, "пушистый -"s);

    MatchDocuments(search_server, "пушистый пёс"s);
    MatchDocuments(search_server, "модный -кот"s);
    MatchDocuments(search_server, "модный --пёс"s);
    MatchDocuments(search_server, "пушистый - хвост"s);
    }

    cout << "//////////////////////////////////////////////////////////////" << endl;

    {
    SearchServer search_server("and with"s);

    search_server.AddDocument(1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2, 3});
    search_server.AddDocument(3, "big cat nasty hair"s, DocumentStatus::ACTUAL, {1, 2, 8});
    search_server.AddDocument(4, "big dog cat Vladislav"s, DocumentStatus::ACTUAL, {1, 3, 2});
    search_server.AddDocument(5, "big dog hamster Borya"s, DocumentStatus::ACTUAL, {1, 1, 1});

    const auto search_results = search_server.FindTopDocuments("curly dog"s);
    int page_size = 2;
    const auto pages = Paginate(search_results, page_size);

    // Выводим найденные документы по страницам
    for (auto page = pages.begin(); page != pages.end(); ++page) {
        cout << *page << endl;
        cout << "Page break"s << endl;
    }
    }

    cout << "//////////////////////////////////////////////////////////////" << endl;

    {
    SearchServer search_server("and in at"s);
    RequestQueue request_queue(search_server);

    search_server.AddDocument(1, "curly cat curly tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "curly dog and fancy collar"s, DocumentStatus::ACTUAL, {1, 2, 3});
    search_server.AddDocument(3, "big cat fancy collar "s, DocumentStatus::ACTUAL, {1, 2, 8});
    search_server.AddDocument(4, "big dog sparrow Eugene"s, DocumentStatus::ACTUAL, {1, 3, 2});
    search_server.AddDocument(5, "big dog sparrow Vasiliy"s, DocumentStatus::ACTUAL, {1, 1, 1});

    // 1439 запросов с нулевым результатом
    for (int i = 0; i < 1439; ++i) {
        request_queue.AddFindRequest("empty request"s);
    }
    // все еще 1439 запросов с нулевым результатом
    request_queue.AddFindRequest("curly dog"s);
    // новые сутки, первый запрос удален, 1438 запросов с нулевым результатом
    request_queue.AddFindRequest("big collar"s);
    // первый запрос удален, 1437 запросов с нулевым результатом
    request_queue.AddFindRequest("sparrow"s);
    cout << "Total empty requests: "s << request_queue.GetNoResultRequests() << endl;
    }

    cout << "//////////////////////////////////////////////////////////////" << endl;

    {
    SearchServer search_server("and with"s);

    AddDocument(search_server, 1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, {7, 2, 7});
    AddDocument(search_server, 2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2});

    // дубликат документа 2, будет удалён
    AddDocument(search_server, 3, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2});

    // отличие только в стоп-словах, считаем дубликатом
    AddDocument(search_server, 4, "funny pet and curly hair"s, DocumentStatus::ACTUAL, {1, 2});

    // множество слов такое же, считаем дубликатом документа 1
    AddDocument(search_server, 5, "funny funny pet and nasty nasty rat"s, DocumentStatus::ACTUAL, {1, 2});

    // добавились новые слова, дубликатом не является
    AddDocument(search_server, 6, "funny pet and not very nasty rat"s, DocumentStatus::ACTUAL, {1, 2});

    // множество слов такое же, как в id 6, несмотря на другой порядок, считаем дубликатом
    AddDocument(search_server, 7, "very nasty rat and not very funny pet"s, DocumentStatus::ACTUAL, {1, 2});

    // есть не все слова, не является дубликатом
    AddDocument(search_server, 8, "pet with rat and rat and rat"s, DocumentStatus::ACTUAL, {1, 2});

    // слова из разных документов, не является дубликатом
    AddDocument(search_server, 9, "nasty rat with curly hair"s, DocumentStatus::ACTUAL, {1, 2});
    
    cout << "Before duplicates removed: "s << search_server.GetDocumentCount() << endl;
    RemoveDuplicates(search_server);
    cout << "After duplicates removed: "s << search_server.GetDocumentCount() << endl;
    }

    cout << "//////////////////////////////////////////////////////////////" << endl;

    TestProcessQueries();

    cout << "//////////////////////////////////////////////////////////////" << endl;

    TestProcessQueriesJoined();
}
