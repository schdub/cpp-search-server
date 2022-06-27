#include "request_queue.h"
#include "document.h"

#include <deque>

RequestQueue::RequestQueue(const SearchServer& search_server)
    : search_server_(search_server)
{}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status) {
    QueryResult result;
    result.documents_ = search_server_.FindTopDocuments(raw_query, status);
    AddToQueue( result );
    return result.documents_;
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query) {
    QueryResult result;
    result.documents_ = search_server_.FindTopDocuments(raw_query);
    AddToQueue( result );
    return result.documents_;
}

int RequestQueue::GetNoResultRequests() const {
    return empty_request_count_;
}

void RequestQueue::AddToQueue(const QueryResult & result) {
    if (result.documents_.empty()) {
        empty_request_count_ += 1;
    }
    requests_.push_front(result);
    while (requests_.size() > min_in_day_) {
        const QueryResult & r = requests_.back();
        if (r.documents_.empty()) {
            empty_request_count_ -= 1;
        }
        requests_.pop_back();
    }
}