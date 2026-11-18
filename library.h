#pragma once
#include "models.h"
#include <memory>
#include <string>
#include <vector>

class Library {
    std::vector<Book> books;
    std::vector<std::unique_ptr<Person>> members;   // polymorphic storage
    int nextBookId = 1;
    int nextMemberId = 1;

    static std::string clean(std::string s);        // removes '|' so the file format stays safe

public:
    static const int FINE_PER_DAY = 5;

    Book& findBook(int id);
    Person& findMember(int id);

    int addBook(const std::string& title, const std::string& author,
                const std::string& isbn, const std::string& location);
    void removeBook(int id);
    void setLocation(int id, const std::string& location);
    void updateMember(int id, const std::string& name);
    void removeMember(int id);

    int addMember(const std::string& type, const std::string& name);
    void issueBook(int bookId, int memberId);
    int returnBook(int bookId);
    int countIssued(int memberId) const;

    const std::vector<Book>& getBooks() const { return books; }
    const std::vector<std::unique_ptr<Person>>& getMembers() const { return members; }

    bool load(const std::string& path);
    void save(const std::string& path) const;
};