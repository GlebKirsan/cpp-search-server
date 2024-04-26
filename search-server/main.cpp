#include <algorithm>
#include <iostream>
#include <set>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <sstream>
#include <iterator>
#include <cmath>

using namespace std;

using DocumentId = int;
using TF = double;
using IDF = double;
using Word = string;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result = 0;
    cin >> result;
    ReadLine();
    return result;
}

vector<Word> SplitIntoWords(const string& text) {
    istringstream is(text);
    return {istream_iterator<Word>(is), istream_iterator<Word>()};
}

struct Document {
    DocumentId id;
    double relevance;
};

class SearchServer {
public:
    using DocumentIds = set<DocumentId>;
    
    void SetStopWords(const string& text) {
        for (const Word& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(DocumentId document_id, const string& document) {
        const vector<Word> words = SplitIntoWordsNoStop(document);
        double step = 1. / words.size();
        for (const Word& word : words) {
            tf_[word][document_id] += step;
        }
        ++document_count_;
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        const Query query_words = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query_words);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 return lhs.relevance > rhs.relevance;
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

private:
    struct Query {
        set<Word> plus_words;
        set<Word> minus_words;
    };
    
    map<Word, map<DocumentId, TF>> tf_;
    set<Word> stop_words_;
    int document_count_ = 0;

    bool IsStopWord(const Word& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const Word& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    Query ParseQuery(const string& text) const {
        Query query;
        for (const Word& query_word : SplitIntoWordsNoStop(text)) {
            bool is_minus = query_word[0] == '-';
            if (is_minus) {
                string word = query_word.substr(1);
                query.minus_words.insert(word);
            } else {
                query.plus_words.insert(query_word);
            }
        }
        return query;
    }
    
    IDF CalculateIDF(size_t documents_number) const {
        return log(static_cast<IDF>(document_count_) / documents_number);
    } 

    vector<Document> FindAllDocuments(const Query& query_words) const {
        vector<Document> matched_documents;
        map<DocumentId, double> relevance;
        for (const Word& word : query_words.plus_words) {
            if (tf_.count(word) == 0) {
                continue;
            }
            const map<DocumentId, TF>& tf = tf_.at(word);
            IDF idf = CalculateIDF(tf.size());
            for (const auto& [document_id, tf_value] : tf) {
                relevance[document_id] += idf * tf_value;
            }
        }
        for (const Word& word : query_words.minus_words) {
            if (tf_.count(word) == 0) {
                continue;
            }
            for (const auto& [document_id, tf_value] : tf_.at(word)) {
                relevance.erase(document_id);
            }
        }
        for (const auto& [doc, rel] : relevance) {
            matched_documents.push_back({doc, rel});
        }
        return matched_documents;
    }
};

SearchServer CreateSearchServer() {
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());

    const int document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        search_server.AddDocument(document_id, ReadLine());
    }

    return search_server;
}

int main() {
    const SearchServer search_server = CreateSearchServer();

    const string query = ReadLine();
    for (const auto& [document_id, relevance] : search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "s << document_id << ", "
             << "relevance = "s << relevance << " }"s << endl;
    }
}