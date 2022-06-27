#pragma once
#include "document.h"
#include "search_server.h"

#include <vector>
#include <deque>
#include <string>

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);

    // сделаем "обёртки" для всех методов поиска, чтобы сохранять результаты для нашей статистики
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
        QueryResult result;
        result.documents_ = search_server_.FindTopDocuments(raw_query, document_predicate);
        AddToQueue( result );
        return result.documents_;
    }
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(const std::string& raw_query);
    int GetNoResultRequests() const;

private:
    struct QueryResult {
        std::vector<Document> documents_;
    };
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    int empty_request_count_ = 0;
    const SearchServer & search_server_;

    void AddToQueue(const QueryResult & result);
};
