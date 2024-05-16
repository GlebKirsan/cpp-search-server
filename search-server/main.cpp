#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <iterator>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using namespace std;

using DocumentId = int;
using Word = string;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

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

vector<Word> SplitIntoWords(const string &text) {
  istringstream is(text);
  return {istream_iterator<Word>(is), istream_iterator<Word>()};
}

vector<int> ReadRating() {
  int count;
  cin >> count;
  vector<int> ratings(count, 0);
  for (int i = 0; i < count; ++i) {
    cin >> ratings[i];
  }
  ReadLine();
  return ratings;
}

enum class DocumentStatus { ACTUAL, IRRELEVANT, BANNED, REMOVED };

struct Document {
  DocumentId id;
  double relevance;
  DocumentStatus status;
  int rating;
};

class SearchServer {
public:
  using DocumentIds = set<DocumentId>;

  void SetStopWords(const string &text) {
    for (const Word &word : SplitIntoWords(text)) {
      stop_words_.insert(word);
    }
  }

  void AddDocument(DocumentId document_id, const string &document,
                   DocumentStatus status, const vector<int> &ratings) {
    const vector<Word> words = SplitIntoWordsNoStop(document);
    double step = 1. / words.size();
    document_id_to_rating_[document_id] = ComputeAverageRating(ratings);
    document_id_to_status_[document_id] = status;
    for (const Word &word : words) {
      tf_[word][document_id] += step;
    }
    ++document_count_;
  }

  vector<Document> FindTopDocuments(
      const string &raw_query,
      DocumentStatus required_status = DocumentStatus::ACTUAL) const {
    return FindTopDocuments(raw_query,
                            [required_status]([[maybe_unused]] int document_id,
                                              DocumentStatus status,
                                              [[maybe_unused]] int rating) {
                              return status == required_status;
                            });
  }

  template <typename Predicate>
  vector<Document> FindTopDocuments(const string &raw_query,
                                    Predicate filter) const {
    const Query query_words = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(query_words);

    sort(matched_documents.begin(), matched_documents.end(),
         [](const Document &lhs, const Document &rhs) {
           return lhs.relevance > rhs.relevance ||
                  (abs(lhs.relevance - rhs.relevance) < EPSILON &&
                   lhs.rating > rhs.rating);
         });
    matched_documents.erase(
        remove_if(matched_documents.begin(), matched_documents.end(),
                  [filter](const Document &lhs) {
                    return !filter(lhs.id, lhs.status, lhs.rating);
                  }),
        matched_documents.end());
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
      matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
  }

  int GetDocumentCount() const { return document_count_; }

  tuple<vector<Word>, DocumentStatus>
  MatchDocument(const string &raw_query, DocumentId document_id) const {
    const Query query = ParseQuery(raw_query);
    DocumentStatus status = document_id_to_status_.at(document_id);
    for (const Word &word : query.minus_words) {
      if (tf_.at(word).count(document_id)) {
        return tuple(vector<Word>{}, status);
      }
    }

    set<Word> matched_words;
    for (const Word &word : query.plus_words) {
      if (tf_.at(word).count(document_id)) {
        matched_words.insert(word);
      }
    }

    return tuple(vector<Word>{matched_words.begin(), matched_words.end()},
                 status);
  }

private:
  struct Query {
    set<Word> plus_words;
    set<Word> minus_words;
  };

  map<Word, map<DocumentId, double>> tf_;
  set<Word> stop_words_;
  map<DocumentId, int> document_id_to_rating_;
  map<DocumentId, DocumentStatus> document_id_to_status_;
  int document_count_ = 0;

  bool IsStopWord(const Word &word) const {
    return stop_words_.count(word) > 0;
  }

  vector<string> SplitIntoWordsNoStop(const string &text) const {
    vector<string> words;
    for (const Word &word : SplitIntoWords(text)) {
      if (!IsStopWord(word)) {
        words.push_back(word);
      }
    }
    return words;
  }

  Query ParseQuery(const string &text) const {
    Query query;
    for (const Word &query_word : SplitIntoWordsNoStop(text)) {
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

  double CalculateIDF(size_t documents_number) const {
    return log(static_cast<double>(document_count_) / documents_number);
  }

  static int ComputeAverageRating(const vector<int> &ratings) {
    if (ratings.empty()) {
      return 0;
    }

    return std::accumulate(ratings.begin(), ratings.end(), 0) /
           static_cast<int>(ratings.size());
  }

  vector<Document> FindAllDocuments(const Query &query_words) const {
    vector<Document> matched_documents;
    map<DocumentId, double> relevance;
    for (const Word &word : query_words.plus_words) {
      if (tf_.count(word) == 0) {
        continue;
      }
      const map<DocumentId, double> &tf = tf_.at(word);
      double idf = CalculateIDF(tf.size());
      for (const auto &[document_id, tf_value] : tf) {
        relevance[document_id] += idf * tf_value;
      }
    }
    for (const Word &word : query_words.minus_words) {
      if (tf_.count(word) == 0) {
        continue;
      }
      for (const auto &[document_id, tf_value] : tf_.at(word)) {
        relevance.erase(document_id);
      }
    }
    for (const auto &[document_id, rel] : relevance) {
      int rating = document_id_to_rating_.at(document_id);
      DocumentStatus status = document_id_to_status_.at(document_id);
      matched_documents.push_back({document_id, rel, status, rating});
    }
    return matched_documents;
  }
};

void PrintDocument(const Document &document) {
  cout << "{ "s << "document_id = "s << document.id << ", "s << "relevance = "s
       << document.relevance << ", "s << "rating = "s << document.rating
       << " }"s << endl;
}

int main() {
  SearchServer search_server;
  search_server.SetStopWords("и в на"s);
  search_server.AddDocument(0, "белый кот и модный ошейник"s,
                            DocumentStatus::ACTUAL, {8, -3});
  search_server.AddDocument(1, "пушистый кот пушистый хвост"s,
                            DocumentStatus::ACTUAL, {7, 2, 7});
  search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s,
                            DocumentStatus::ACTUAL, {5, -12, 2, 1});
  search_server.AddDocument(3, "ухоженный скворец евгений"s,
                            DocumentStatus::BANNED, {9});
  cout << "ACTUAL by default:"s << endl;
  for (const Document &document :
       search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
    PrintDocument(document);
  }
  cout << "BANNED:"s << endl;
  for (const Document &document : search_server.FindTopDocuments(
           "пушистый ухоженный кот"s, DocumentStatus::BANNED)) {
    PrintDocument(document);
  }
  cout << "Even ids:"s << endl;
  for (const Document &document : search_server.FindTopDocuments(
           "пушистый ухоженный кот"s,
           [](int document_id, [[maybe_unused]] DocumentStatus status,
              [[maybe_unused]] int rating) { return document_id % 2 == 0; })) {
    PrintDocument(document);
  }
  return 0;
}