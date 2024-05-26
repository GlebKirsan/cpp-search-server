#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <iterator>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using namespace std;

template <typename F, typename S>
ostream& operator<<(ostream& output, const pair<F, S>& p) {
    output << p.first << ": "s << p.second;
    return output;
}

template <typename Container>
void Print(ostream& output, const Container& items) {
    bool first_item = true;
    for (const auto& item : items) {
        if (!first_item) {
            output << ", "s;
        }
        output << item;
        first_item = false;
    }
}

template <typename T>
ostream& operator<<(ostream& output, const vector<T>& items) {
    output << "["s;
    Print(output, items);
    output << "]"s;
    return output;
}

template <typename T>
ostream& operator<<(ostream& output, const set<T>& items) {
    output << "{"s;
    Print(output, items);
    output << "}"s;
    return output;
}

template <typename K, typename V>
ostream& operator<<(ostream& output, const map<K, V>& items) {
    output << "{"s;
    Print(output, items);
    output << "}"s;
    return output;
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str,
                     const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

void AssertImpl(bool value, const string& expr_str, const string& file,
                const string& func, unsigned line, const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "Assert("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) \
    AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) \
    AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

#define ASSERT(a) AssertImpl((a), #a, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(a, hint) \
    AssertImpl((a), #a, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename TestFunc>
void RunTestImpl(const TestFunc& func, const string& test_name) {
    func();
    cerr << test_name << " OK"s << endl;
}

#define RUN_TEST(func) RunTestImpl(func, #func)

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

vector<Word> SplitIntoWords(const string& text) {
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

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    set<string> non_empty_strings;
    for (const string& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

enum class DocumentStatus { ACTUAL, IRRELEVANT, BANNED, REMOVED };

struct Document {
    DocumentId id = 0;
    double relevance = 0.0;
    DocumentStatus status = DocumentStatus::ACTUAL;
    int rating = 0;

    Document() = default;

    Document(DocumentId id, double relevance, int rating)
        : id(id), relevance(relevance), rating(rating) {}
};

class SearchServer {
   public:
    using DocumentIds = set<DocumentId>;

    SearchServer() = default;

    SearchServer(const string& stop_words)
        : SearchServer(SplitIntoWords(stop_words)) {}

    template <typename Container>
    SearchServer(const Container& stop_words) {
        set<Word> uniquie_stop_words = MakeUniqueNonEmptyStrings(stop_words);
        bool has_invalid_stop_word =
            any_of(uniquie_stop_words.begin(), uniquie_stop_words.end(),
                   [this](const Word& word) { return this->IsInvalid(word); });
        if (has_invalid_stop_word) {
            throw invalid_argument("Invalid stop word"s);
        }
        stop_words_ = uniquie_stop_words;
    }

    void AddDocument(DocumentId document_id, const string& document,
                     DocumentStatus status, const vector<int>& ratings) {
        if (document_id < 0 ||
            document_id_to_rating_and_rating_.count(document_id) != 0 ||
            IsInvalid(document)) {
            throw invalid_argument("Bad document to add"s);
        }
        const vector<Word> words = SplitIntoWordsNoStop(document);
        document_id_to_rating_and_rating_[document_id] =
            pair(ComputeAverageRating(ratings), status);
        double step = 1. / words.size();
        for (const Word& word : words) {
            tf_[word][document_id] += step;
        }
        document_id_orders_.push_back(document_id);
    }

    vector<Document> FindTopDocuments(
        const string& raw_query,
        DocumentStatus status = DocumentStatus::ACTUAL) const {
        return FindTopDocuments(
            raw_query,
            [status]([[maybe_unused]] int document_id, DocumentStatus status_,
                     [[maybe_unused]] int rating) {
                return status_ == status;
            });
    }

    template <typename Predicate>
    vector<Document> FindTopDocuments(const string& raw_query,
                                      Predicate filter) const {
        Query query_words = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query_words);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 return lhs.relevance > rhs.relevance ||
                        (abs(lhs.relevance - rhs.relevance) < EPSILON &&
                         lhs.rating > rhs.rating);
             });
        matched_documents.erase(
            remove_if(matched_documents.begin(), matched_documents.end(),
                      [filter](const Document& lhs) {
                          return !filter(lhs.id, lhs.status, lhs.rating);
                      }),
            matched_documents.end());
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    int GetDocumentCount() const { return document_id_orders_.size(); }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
                                                        int document_id) const {
        Query query = ParseQuery(raw_query);
        DocumentStatus status =
            document_id_to_rating_and_rating_.at(document_id).second;
        for (const Word& word : query.minus_words) {
            if (tf_.at(word).count(document_id)) {
                return tuple(vector<Word>(), status);
            }
        }

        set<Word> matched_words;
        for (const Word& word : query.plus_words) {
            if (tf_.count(word) && tf_.at(word).count(document_id)) {
                matched_words.insert(word);
            }
        }

        return tuple(vector<Word>{matched_words.begin(), matched_words.end()},
                     status);
    }

    int GetDocumentId(int index) const { return document_id_orders_.at(index); }

   private:
    struct Query {
        set<Word> plus_words;
        set<Word> minus_words;
    };

    map<Word, map<DocumentId, double>> tf_;
    set<Word> stop_words_;
    map<DocumentId, pair<int, DocumentStatus>>
        document_id_to_rating_and_rating_;
    vector<DocumentId> document_id_orders_;

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

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        if (IsInvalid(text)) {
            throw invalid_argument("Invalid query"s);
        }
        bool is_minus = (text[0] == '-');
        if (is_minus) {
            if (text.size() == 1 || text[1] == '-') {
                throw invalid_argument("Invalid minus word"s);
            }
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
    }

    Query ParseQuery(const string& text) const {
        Query query;
        for (const Word& word : SplitIntoWordsNoStop(text)) {
            QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    double CalculateIDF(size_t documents_number) const {
        return log(static_cast<double>(document_id_orders_.size()) /
                   documents_number);
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }

        return std::accumulate(ratings.begin(), ratings.end(), 0) /
               static_cast<int>(ratings.size());
    }

    vector<Document> FindAllDocuments(const Query& query_words) const {
        vector<Document> matched_documents;
        map<DocumentId, double> relevance;
        for (const Word& word : query_words.plus_words) {
            if (tf_.count(word) == 0) {
                continue;
            }
            const map<DocumentId, double>& tf = tf_.at(word);
            double idf = CalculateIDF(tf.size());
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
        for (const auto& [document_id, rel] : relevance) {
            const auto [rating, status] =
                document_id_to_rating_and_rating_.at(document_id);
            matched_documents.push_back({document_id, rel, rating});
            matched_documents.back().status = status;
        }
        return matched_documents;
    }

    bool IsInvalid(const string& text) const {
        return any_of(text.begin(), text.end(),
                      [](const char& c) { return 0 <= c && c <= 31; });
    }
};

void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Stop words must be excluded from documents"s);
    }
}

void TestExcludeDocumentsContainingMinusWordsFromSearchResults() {
    SearchServer server;
    server.AddDocument(0, "cat in the city"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(1, "cat in boots"s, DocumentStatus::ACTUAL, {1});
    {
        const auto found_docs = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(found_docs.size(), 2u);
    }
    {
        const auto found_docs = server.FindTopDocuments("cat -boots"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, 0);
    }
    {
        const auto found_docs = server.FindTopDocuments("cat -city"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, 1);
    }
}

void TestExcludeAllMatchedWordsIfMinusWordMatched() {
    SearchServer server;
    server.AddDocument(0, "black cat is in the city"s, DocumentStatus::ACTUAL,
                       {1});
    {
        const auto [words, status] = server.MatchDocument("black cat"s, 0);
        ASSERT_EQUAL(count(words.begin(), words.end(), "cat"s), 1);
        ASSERT_EQUAL(count(words.begin(), words.end(), "black"s), 1);
    }
    {
        const auto [words, status] = server.MatchDocument("cat -black"s, 0);
        ASSERT(words.empty());
    }
}

void TestSortMatchedDocumentsByRelevanceDescending() {
    SearchServer server;
    server.AddDocument(0, "white cat with black tail"s, DocumentStatus::ACTUAL,
                       {1});
    server.AddDocument(1, "cat eats milk"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(2, "dog likes milk"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(3, "dog sees a cat near the tree"s,
                       DocumentStatus::ACTUAL, {1});
    {
        const auto docs = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(docs.size(), 3u);
        ASSERT(docs.front().relevance > docs.back().relevance);
        for (size_t i = 1; i < docs.size(); ++i) {
            ASSERT(docs[i - 1].relevance >= docs[i].relevance);
        }
    }
}

void TestDocumentRatingIsAnAverageOfAllRatings() {
    SearchServer server;
    server.AddDocument(0, "cat"s, DocumentStatus::ACTUAL, {5, 7, 12});
    const auto docs = server.FindTopDocuments("cat"s);
    ASSERT_EQUAL(docs.size(), 1u);
    ASSERT_EQUAL(docs.front().rating, (5 + 7 + 12) / 3);
}

void TestDocumentsAreFilteredUsingPredicate() {
    SearchServer server;
    server.AddDocument(0, "black cat"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(1, "white cat"s, DocumentStatus::ACTUAL, {1});
    ASSERT_EQUAL(server.FindTopDocuments("cat"s).size(), 2u);
    const auto docs = server.FindTopDocuments(
        "cat"s,
        [](int document_id, [[maybe_unused]] DocumentStatus document_status,
           [[maybe_unused]] int rating) { return document_id == 1; });
    ASSERT_EQUAL(docs.size(), 1u);
    ASSERT(docs.front().id == 1);
}

void TestDocumentStatusFiltering() {
    SearchServer server;
    server.AddDocument(0, "black cat"s, DocumentStatus::IRRELEVANT, {1});
    server.AddDocument(1, "white cat"s, DocumentStatus::BANNED, {1});
    {
        const auto banned_docs =
            server.FindTopDocuments("cat"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(banned_docs.size(), 1u);
        ASSERT_EQUAL(banned_docs.front().id, 0);
    }
    {
        const auto banned_docs =
            server.FindTopDocuments("cat"s, DocumentStatus::BANNED);
        ASSERT_EQUAL(banned_docs.size(), 1u);
        ASSERT_EQUAL(banned_docs.front().id, 1);
    }
}

bool NearlyEquals(double a, double b) { return abs(a - b) < EPSILON; }

void TestDocumentRelevanceCalculation() {
    SearchServer server;
    server.AddDocument(0, "one"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(1, "two three"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(2, "three four five"s, DocumentStatus::ACTUAL, {1});
    {
        const auto docs = server.FindTopDocuments("one"s);
        ASSERT_EQUAL(docs.size(), 1u);
        ASSERT_EQUAL(docs[0].id, 0);
        ASSERT(NearlyEquals(docs[0].relevance,
                            (log(server.GetDocumentCount() * 1.0 / 1) * 1.0)));
    }
    {
        const auto docs = server.FindTopDocuments("four"s);
        ASSERT_EQUAL(docs.size(), 1u);
        ASSERT_EQUAL(docs[0].id, 2);
        ASSERT(NearlyEquals(
            docs[0].relevance,
            (log(server.GetDocumentCount() * 1.0 / 1) * (1.0 / 3))));
    }
    {
        const auto docs = server.FindTopDocuments("four five"s);
        ASSERT_EQUAL(docs.size(), 1u);
        ASSERT_EQUAL(docs[0].id, 2);
        ASSERT(NearlyEquals(
            docs[0].relevance,
            (log(server.GetDocumentCount() * 1.0 / 1) * (2.0 / 3))));
    }
    {
        const auto docs = server.FindTopDocuments("one three"s);
        ASSERT_EQUAL(docs.size(), 3u);
        ASSERT_EQUAL(docs[0].id, 0);
        ASSERT(
            NearlyEquals(docs[0].relevance,
                         (log(server.GetDocumentCount() * 1.0 / 1) * (1.0))));
        ASSERT_EQUAL(docs[1].id, 1);
        ASSERT(NearlyEquals(
            docs[1].relevance,
            (log(server.GetDocumentCount() * 1.0 / 2) * (1.0 / 2))));
        ASSERT_EQUAL(docs[2].id, 2);
        ASSERT(NearlyEquals(
            docs[2].relevance,
            (log(server.GetDocumentCount() * 1.0 / 2) * (1.0 / 3))));
    }
}
ostream& operator<<(ostream& output, DocumentStatus document_status) {
    switch (document_status) {
        case DocumentStatus::ACTUAL:
            output << "ACTUAL"s;
            break;
        case DocumentStatus::IRRELEVANT:
            output << "IRRELEVANT"s;
            break;
        case DocumentStatus::BANNED:
            output << "BANNED"s;
            break;
        case DocumentStatus::REMOVED:
            output << "REMOVED"s;
            break;
        default:
            output << "<unknown>"s;
            break;
    }
    return output;
}

void TestMatchingDocuments() {
    SearchServer server("a the and"s);
    server.AddDocument(0, "a quick brown fox jumps over the lazy dog"s,
                       DocumentStatus::BANNED, {1, 2, 3});
    const auto [words, status] =
        server.MatchDocument("a lazy cat and the brown dog"s, 0);
    set<string> matched_words;
    for (const auto& word : words) {
        matched_words.insert(word);
    }
    const set<string> expected_matched_words = {"lazy"s, "dog"s, "brown"s};
    ASSERT_EQUAL(matched_words, expected_matched_words);
    ASSERT_EQUAL(status, DocumentStatus::BANNED);
}

void TestGettingDocumentCount() {
    SearchServer server;
    ASSERT_EQUAL(server.GetDocumentCount(), 0);
    server.AddDocument(0, "cat drinks milk"s, DocumentStatus::ACTUAL, {1});
    ASSERT_EQUAL(server.GetDocumentCount(), 1);
    server.AddDocument(2, "dog eats a bone"s, DocumentStatus::ACTUAL, {1});
    ASSERT_EQUAL(server.GetDocumentCount(), 2);
}

// Точка входа для тестирования поисковой системы
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludeDocumentsContainingMinusWordsFromSearchResults);
    RUN_TEST(TestExcludeAllMatchedWordsIfMinusWordMatched);
    RUN_TEST(TestSortMatchedDocumentsByRelevanceDescending);
    RUN_TEST(TestDocumentRatingIsAnAverageOfAllRatings);
    RUN_TEST(TestDocumentsAreFilteredUsingPredicate);
    RUN_TEST(TestDocumentStatusFiltering);
    RUN_TEST(TestDocumentRelevanceCalculation);
    RUN_TEST(TestMatchingDocuments);
    RUN_TEST(TestGettingDocumentCount);
}

int main() { TestSearchServer(); }