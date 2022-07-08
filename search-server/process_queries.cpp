#include "process_queries.h"
#include <algorithm>
#include <execution>

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {

    std::vector<std::vector<Document>> out;
    out.resize(queries.size());
    std::transform(
        std::execution::par,
        queries.begin(), queries.end(),
        out.begin(),
        [&search_server](const std::string & raw_query) {
            return search_server.FindTopDocuments(raw_query);
        }
    );

    return out;
}