#include "remove_duplicates.h"
#include "search_server.h"

#include <string>
#include <vector>
#include <map>
#include <set>
#include <iostream>

void RemoveDuplicates(SearchServer & search_server) {
    std::map< std::set<std::string_view>, std::vector<int> > index;
    for (auto doc_id : search_server) {
        std::set<std::string_view> set_of_strings;
        for (auto & [word, _] : search_server.GetWordFrequencies(doc_id)) {
            set_of_strings.insert(word);
        }
        index[set_of_strings].push_back(doc_id);
    }
    for (auto & [ss, vectr] : index) {
        if (vectr.size() != 1) {
            std::sort(vectr.begin(), vectr.end());
            for (size_t i = 1; i < vectr.size(); ++i) {
                int document_id = vectr[i];
                std::cout << "Found duplicate document id " << document_id << std::endl;
                search_server.RemoveDocument(document_id);
            }
        }
    }
}
