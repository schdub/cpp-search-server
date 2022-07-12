#include "string_processing.h"
#include <algorithm>

using namespace std;

vector<string_view> SplitIntoWords(string_view text) {
    vector<string_view> words;
    size_t e, b;
    b = text.find_first_not_of(' ');
    while (b != string::npos) {
        e = text.find_first_of(' ', b+1);
        if (e == string::npos) {
            if (text.size() - b) {
                words.push_back( text.substr(b, text.size()-b) );
            }
            break;
        }
        words.push_back( text.substr(b, e-b) );
        b = text.find_first_not_of(' ', e+1);
    }
    return words;
}

bool HasSpecialSymbols(string_view text) {
    bool result = std::any_of(text.begin(), text.end(), [](char ch) {
        return (ch >= 0 && ch <= 31);
    });
    return result;
}
