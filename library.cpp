#include "library.h"
#include <algorithm>
#include <ctime>
#include <fstream>
#include <sstream>
#include <stdexcept>

static long long nowSeconds() {
    return static_cast<long long>(std::time(nullptr));
}

static std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> parts;
    std::string item;
    std::istringstream ss(s);
    while (std::getline(ss, item, delim)) parts.push_back(item);
    return parts;
}

std::string Library::clean(std::string s) {
    for (char& c : s)
        if (c == '|' || c == '\n' || c == '\r') c = ' ';
    return s;
}

Book& Library::findBook(int id) {
    for (auto& b : books)
        if (b.getId() == id) return b;
    throw std::runtime_error("Book not found");
}

Person& Library::findMember(int id) {
    for (auto& m : members)
        if (m->getId() == id) return *m;
    throw std::runtime_error("Member not found");
}

int Library::addBook(const std::string& title, const std::string& author,
                     const std::string& isbn, const std::string& location) {
    if (title.empty() || author.empty())
        throw std::runtime_error("Title and author are required");
    if (location.empty())
        throw std::runtime_error("Shelf location is required");

    books.emplace_back(nextBookId, clean(title), clean(author), clean(isbn), clean(location));
    return nextBookId++;
}

void Library::removeBook(int id) {
    for (auto it = books.begin(); it != books.end(); ++it) {
        if (it->getId() == id) {
            if (!it->isAvailable())
                throw std::runtime_error("Cannot delete a book that is currently issued");
            books.erase(it);
            return;
        }
    }
    throw std::runtime_error("Book not found");
}

void Library::setLocation(int id, const std::string& location) {
    if (location.empty())
        throw std::runtime_error("Shelf location is required");
    findBook(id).setLocation(clean(location));
}

int Library::addMember(const std::string& type, const std::string& name) {
    if (name.empty()) throw std::runtime_error("Member name is required");

    if (type == "Student")
        members.push_back(std::make_unique<Student>(nextMemberId, clean(name)));
    else if (type == "Faculty")
        members.push_back(std::make_unique<Faculty>(nextMemberId, clean(name)));
    else
        throw std::runtime_error("Unknown member type");

    return nextMemberId++;
}

int Library::countIssued(int memberId) const {
    int count = 0;
    for (const auto& b : books)
        if (b.getIssuedTo() == memberId) ++count;
    return count;
}

void Library::issueBook(int bookId, int memberId) {
    Book& book = findBook(bookId);
    Person& member = findMember(memberId);

    if (!book.isAvailable())
        throw std::runtime_error("This book is already issued");

    if (countIssued(memberId) >= member.maxBooks())
        throw std::runtime_error(member.getName() + " has reached the limit of " +
                                 std::to_string(member.maxBooks()) + " books");

    book.issue(memberId, nowSeconds() + member.loanDays() * 86400LL);
}

int Library::returnBook(int bookId) {
    Book& book = findBook(bookId);
    if (book.isAvailable())
        throw std::runtime_error("This book is not issued");

    long long lateSeconds = nowSeconds() - book.getDueDate();
    int fine = 0;
    if (lateSeconds > 0) {
        long long lateDays = (lateSeconds + 86399) / 86400;
        fine = static_cast<int>(lateDays) * FINE_PER_DAY;
    }
    book.giveBack();
    return fine;
}

// ---------- File format ----------
// M|id|type|name
// B|id|title|author|isbn|issuedTo|dueDate|location
bool Library::load(const std::string& path) {
    std::ifstream in(path);
    if (!in) return false;

    std::string line;
    while (std::getline(in, line)) {
        auto f = split(line, '|');
        if (f.size() < 4) continue;

        if (f[0] == "M") {
            int id = std::stoi(f[1]);
            if (f[2] == "Student")
                members.push_back(std::make_unique<Student>(id, f[3]));
            else if (f[2] == "Faculty")
                members.push_back(std::make_unique<Faculty>(id, f[3]));
            nextMemberId = std::max(nextMemberId, id + 1);
        }
        else if (f[0] == "B" && f.size() >= 7) {
            int id = std::stoi(f[1]);
            // Older data files have no location field, so use a default
            std::string loc = (f.size() >= 8 && !f[7].empty()) ? f[7] : "Not assigned";
            books.emplace_back(id, f[2], f[3], f[4], loc, std::stoi(f[5]), std::stoll(f[6]));
            nextBookId = std::max(nextBookId, id + 1);
        }
    }
    return true;
}

void Library::save(const std::string& path) const {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("Could not write data file");

    for (const auto& m : members)
        out << "M|" << m->getId() << "|" << m->getType() << "|" << m->getName() << "\n";

    for (const auto& b : books)
        out << "B|" << b.getId() << "|" << b.getTitle() << "|" << b.getAuthor() << "|"
            << b.getIsbn() << "|" << b.getIssuedTo() << "|" << b.getDueDate() << "|"
            << b.getLocation() << "\n";
}
// ---------- Member editing ----------
void Library::updateMember(int id, const std::string& name) {
    if (name.empty()) throw std::runtime_error("Member name is required");
    findMember(id).setName(clean(name));
}

void Library::removeMember(int id) {
    for (auto it = members.begin(); it != members.end(); ++it) {
        if ((*it)->getId() == id) {
            if (countIssued(id) > 0)
                throw std::runtime_error("Member still has books issued. Return them first.");
            members.erase(it);
            return;
        }
    }
    throw std::runtime_error("Member not found");
}