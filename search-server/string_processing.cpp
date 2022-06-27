#include "string_processing.h"
#include <algorithm>

using namespace std;

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

bool HasSpecialSymbols(const string & text) {
    bool result = std::any_of(text.begin(), text.end(), [](char ch) {
        return (ch >= 0 && ch <= 31);
    });
    return result;
}
