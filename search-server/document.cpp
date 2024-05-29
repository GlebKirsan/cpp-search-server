#include "document.h"
#include <string>

using namespace std;

Document::Document(int id, double relevance, int rating)
    : id(id), relevance(relevance), rating(rating) {}

ostream& operator<<(ostream& os, const Document& doc) {
    os << "{ document_id = "s << doc.id << ", relevance = "s << doc.relevance
        << ", rating = "s << doc.rating << " }"s;
    return os;
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