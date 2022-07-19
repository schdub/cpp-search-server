#include "string_processing.h"
#include <algorithm>

using namespace std;

vector<string_view> SplitIntoWords(string_view text) {
    vector<string_view> words;
    for (size_t range_begin = text.find_first_not_of(' '); range_begin != string::npos; ) {
        size_t range_end = text.find_first_of(' ', range_begin + 1);
        if (range_end == string::npos) {
            size_t diff = text.size() - range_begin;
            if (diff > 0) {
                words.push_back( text.substr(range_begin, diff) );
            }
            break;
        }
        words.push_back( text.substr(range_begin, range_end - range_begin) );
        range_begin = text.find_first_not_of(' ', range_end + 1);
    }
    return words;
}
