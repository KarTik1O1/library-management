#pragma once
#include <string>
#include <utility>

// ---------- Abstract base class ----------
class Person {
protected:
    int id;
    std::string name;

public:
    Person(int id, std::string name) : id(id), name(std::move(name)) {}
    virtual ~Person() = default;

    int getId() const { return id; }
    const std::string& getName() const { return name; }
    void setName(std::string n) { name = std::move(n); }

    // Pure virtual functions make Person ABSTRACT (cannot create a Person object)
    // Each member type defines its own rules -> POLYMORPHISM
    virtual std::string getType() const = 0;
    virtual int maxBooks() const = 0;
    virtual int loanDays() const = 0;
};

// ---------- Derived classes (INHERITANCE) ----------
class Student : public Person {
public:
    Student(int id, std::string name) : Person(id, std::move(name)) {}
    std::string getType() const override { return "Student"; }
    int maxBooks() const override { return 3; }
    int loanDays() const override { return 14; }
};

class Faculty : public Person {
public:
    Faculty(int id, std::string name) : Person(id, std::move(name)) {}
    std::string getType() const override { return "Faculty"; }
    int maxBooks() const override { return 5; }
    int loanDays() const override { return 30; }
};

// ---------- Book (ENCAPSULATION: data is private, accessed via methods) ----------
// ---------- Book (ENCAPSULATION: data is private, accessed via methods) ----------
class Book {
    int id;
    std::string title, author, isbn, location;   // location = shelf, e.g. "Floor 2 - Shelf C1"
    int issuedTo;        // member id, 0 means available
    long long dueDate;   // unix timestamp in seconds, 0 when not issued

public:
    Book(int id, std::string title, std::string author, std::string isbn,
         std::string location, int issuedTo = 0, long long dueDate = 0)
        : id(id), title(std::move(title)), author(std::move(author)),
          isbn(std::move(isbn)), location(std::move(location)),
          issuedTo(issuedTo), dueDate(dueDate) {}

    int getId() const { return id; }
    const std::string& getTitle() const { return title; }
    const std::string& getAuthor() const { return author; }
    const std::string& getIsbn() const { return isbn; }
    const std::string& getLocation() const { return location; }
    int getIssuedTo() const { return issuedTo; }
    long long getDueDate() const { return dueDate; }

    bool isAvailable() const { return issuedTo == 0; }
    void issue(int memberId, long long due) { issuedTo = memberId; dueDate = due; }
    void giveBack() { issuedTo = 0; dueDate = 0; }
    void setLocation(const std::string& loc) { location = loc; }
};