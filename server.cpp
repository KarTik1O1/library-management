#include "httplib.h"
#include "library.h"
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <iostream>
#include <mutex>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>

// ---------- Helpers ----------
static std::string esc(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"' || c == '\\') { out += '\\'; out += c; }
        else if (c == '\n') out += "\\n";
        else out += c;
    }
    return out;
}

static std::string jsonMsg(bool ok, const std::string& msg) {
    return std::string("{\"ok\":") + (ok ? "true" : "false") +
           ",\"message\":\"" + esc(msg) + "\"}";
}

static int intParam(const httplib::Request& req, const std::string& key) {
    std::string v = req.get_param_value(key);
    try {
        return std::stoi(v);
    } catch (...) {
        throw std::runtime_error("Please select a valid " + key);
    }
}

// ---------- Session helpers ----------
static const char* SESSION_COOKIE = "session";

static std::string makeToken() {
    std::random_device rd;
    std::mt19937_64 gen((static_cast<unsigned long long>(rd()) << 32) ^ rd() ^
                        static_cast<unsigned long long>(std::time(nullptr)));
    std::uniform_int_distribution<unsigned long long> dist;
    std::ostringstream o;
    o << std::hex << dist(gen) << dist(gen);
    return o.str();
}

static std::string tokenFrom(const httplib::Request& req) {
    std::string cookie = req.get_header_value("Cookie");
    std::string key = std::string(SESSION_COOKIE) + "=";
    size_t pos = cookie.find(key);
    if (pos == std::string::npos) return "";
    size_t start = pos + key.size();
    size_t end = cookie.find(';', start);
    return cookie.substr(start, end == std::string::npos ? std::string::npos : end - start);
}

// ---------- JSON builders ----------
static std::string memberName(const Library& lib, int id) {
    for (const auto& m : lib.getMembers())
        if (m->getId() == id) return m->getName();
    return "Unknown";
}

// Visitors get no borrower names or due dates. Admin gets everything.
static std::string booksJson(const Library& lib, bool admin) {
    long long now = static_cast<long long>(std::time(nullptr));
    std::ostringstream o;
    o << "[";
    bool first = true;
    for (const Book& b : lib.getBooks()) {
        bool available = b.isAvailable();
        bool overdue = !available && b.getDueDate() < now;

        if (!first) o << ",";
        first = false;
        o << "{\"id\":" << b.getId()
          << ",\"title\":\"" << esc(b.getTitle()) << "\""
          << ",\"author\":\"" << esc(b.getAuthor()) << "\""
          << ",\"isbn\":\"" << esc(b.getIsbn()) << "\""
          << ",\"location\":\"" << esc(b.getLocation()) << "\""
          << ",\"available\":" << (available ? "true" : "false")
          << ",\"overdue\":" << (overdue ? "true" : "false");
        if (admin) {
            o << ",\"issuedTo\":" << b.getIssuedTo()
              << ",\"holder\":\"" << esc(available ? "" : memberName(lib, b.getIssuedTo())) << "\""
              << ",\"due\":" << b.getDueDate();
        }
        o << "}";
    }
    o << "]";
    return o.str();
}

static std::string membersJson(const Library& lib) {
    std::ostringstream o;
    o << "[";
    bool first = true;
    for (const auto& m : lib.getMembers()) {
        if (!first) o << ",";
        first = false;
        o << "{\"id\":" << m->getId()
          << ",\"name\":\"" << esc(m->getName()) << "\""
          << ",\"type\":\"" << m->getType() << "\""
          << ",\"issued\":" << lib.countIssued(m->getId())
          << ",\"max\":" << m->maxBooks() << "}";
    }
    o << "]";
    return o.str();
}

// ---------- Sample catalogue ----------
struct Sample {
    const char* title;
    const char* author;
    const char* isbn;
    const char* location;
};

static const Sample SAMPLES[] = {
    {"Introduction to Algorithms", "Thomas H. Cormen", "9780262033848", "Floor 1 - Shelf A2"},
    {"Operating System Concepts", "Abraham Silberschatz", "9781118063330", "Floor 1 - Shelf A3"},
    {"Computer Networks", "Andrew S. Tanenbaum", "9780132126953", "Floor 1 - Shelf A4"},
    {"Database System Concepts", "Abraham Silberschatz", "9780078022159", "Floor 1 - Shelf B1"},
    {"Data Structures Using C", "Reema Thareja", "9780198099307", "Floor 1 - Shelf B2"},
    {"Design Patterns", "Erich Gamma et al.", "9780201633610", "Floor 2 - Shelf C1"},
    {"Effective C++", "Scott Meyers", "9780321334879", "Floor 2 - Shelf C2"},
    {"Python Crash Course", "Eric Matthes", "9781593276034", "Floor 2 - Shelf D1"},
    {"The Pragmatic Programmer", "Andrew Hunt, David Thomas", "9780135957059", "Floor 2 - Shelf D2"},
    {"Engineering Mathematics", "B. S. Grewal", "9788174091956", "Floor 3 - Shelf E1"},
    {"Sapiens", "Yuval Noah Harari", "9780062316097", "Floor 3 - Shelf F1"},
    {"The Art of Computer Programming", "Donald E. Knuth", "9780201896831", "Floor 3 - Shelf F2"},
    {"Code Complete", "Steve McConnell", "9780735619678", "Floor 2 - Shelf D4"},
    {"Refactoring", "Martin Fowler", "9780134757599", "Floor 2 - Shelf D5"},
    {"The Mythical Man-Month", "Frederick P. Brooks Jr.", "9780201835953", "Floor 2 - Shelf E2"},
    {"Thinking, Fast and Slow", "Daniel Kahneman", "9780374533557", "Floor 3 - Shelf F3"},
    {"Atomic Habits", "James Clear", "9780735211292", "Floor 3 - Shelf F4"},
    {"The Lean Startup", "Eric Ries", "9780307887894", "Floor 3 - Shelf F5"},
    {"Artificial Intelligence: A Modern Approach", "Stuart Russell, Peter Norvig", "9780134610999", "Floor 1 - Shelf A5"},
    {"Designing Data-Intensive Applications", "Martin Kleppmann", "9781449373320", "Floor 2 - Shelf E3"},
    {"Gödel, Escher, Bach", "Douglas R. Hofstadter", "9780465026562", "Floor 3 - Shelf G1"},
    {"Cracking the Coding Interview", "Gayle Laakmann McDowell", "9780984782857", "Floor 1 - Shelf B3"},
    {"Eloquent JavaScript", "Marijn Haverbeke", "9781593279509", "Floor 1 - Shelf B4"},
    {"Grokking Algorithms", "Aditya Bhargava", "9781617292231", "Floor 1 - Shelf B5"},
    {"Software Engineering", "Ian Sommerville", "9780133943030", "Floor 2 - Shelf E4"},
    {"Introduction to the Theory of Computation", "Michael Sipser", "9781133187790", "Floor 1 - Shelf A6"},
};

// Adds every sample whose ISBN is not already in the catalogue
static std::string addSamples(Library& lib) {
    int added = 0;
    for (const Sample& s : SAMPLES) {
        bool exists = false;
        for (const Book& b : lib.getBooks())
            if (b.getIsbn() == s.isbn) { exists = true; break; }
        if (!exists) {
            lib.addBook(s.title, s.author, s.isbn, s.location);
            ++added;
        }
    }
    return std::to_string(added) + " sample book(s) added";
}

static void seed(Library& lib) {
    lib.addBook("Let Us C", "Yashavant Kanetkar", "9788183331630", "Floor 1 - Shelf A1");
    lib.addBook("The C++ Programming Language", "Bjarne Stroustrup", "9780321563842", "Floor 2 - Shelf C3");
    lib.addBook("Clean Code", "Robert C. Martin", "9780132350884", "Floor 2 - Shelf D3");
    lib.addBook("Head First Design Patterns", "Eric Freeman", "9780596007126", "Floor 2 - Shelf C4");
    addSamples(lib);
    lib.addMember("Student", "Aarav Sharma");
    lib.addMember("Faculty", "Dr. Neha Patil");
}

// ---------- Main ----------
int main() {
    const char* pwEnv = std::getenv("LIBRARY_ADMIN_PASSWORD");
    if (!pwEnv || std::string(pwEnv).empty()) {
        std::cerr << "Set an admin password first, for example:\n"
                  << "  export LIBRARY_ADMIN_PASSWORD='choose-a-strong-password'\n";
        return 1;
    }
    const std::string ADMIN_PASSWORD = pwEnv;

    Library lib;
    const std::string DATA_FILE = "library_data.txt";

    if (!lib.load(DATA_FILE)) {
        seed(lib);
        lib.save(DATA_FILE);
    }

    std::mutex mtx;
    std::set<std::string> sessions;

    httplib::Server svr;
    svr.set_mount_point("/", "./public");

    // Admin-only actions: check login, run under lock, save, return JSON
    auto run = [&](const httplib::Request& req, httplib::Response& res,
                   const std::function<std::string()>& action) {
        std::lock_guard<std::mutex> lock(mtx);
        if (sessions.count(tokenFrom(req)) == 0) {
            res.status = 401;
            res.set_content(jsonMsg(false, "Admin login required"), "application/json");
            return;
        }
        try {
            std::string msg = action();
            lib.save(DATA_FILE);
            res.set_content(jsonMsg(true, msg), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(jsonMsg(false, e.what()), "application/json");
        }
    };

    // ----- Login / logout -----
    svr.Get("/api/me", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(mtx);
        bool admin = sessions.count(tokenFrom(req)) > 0;
        res.set_content(std::string("{\"admin\":") + (admin ? "true" : "false") + "}",
                        "application/json");
    });

    svr.Post("/api/login", [&](const httplib::Request& req, httplib::Response& res) {
        if (req.get_param_value("password") != ADMIN_PASSWORD) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            res.status = 401;
            res.set_content(jsonMsg(false, "Wrong password"), "application/json");
            return;
        }
        std::string token = makeToken();
        {
            std::lock_guard<std::mutex> lock(mtx);
            sessions.insert(token);
        }
        res.set_header("Set-Cookie",
                       std::string(SESSION_COOKIE) + "=" + token + "; Path=/; HttpOnly; SameSite=Strict");
        res.set_content(jsonMsg(true, "Logged in"), "application/json");
    });

    svr.Post("/api/logout", [&](const httplib::Request& req, httplib::Response& res) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            sessions.erase(tokenFrom(req));
        }
        res.set_header("Set-Cookie",
                       std::string(SESSION_COOKIE) + "=; Path=/; Max-Age=0; HttpOnly; SameSite=Strict");
        res.set_content(jsonMsg(true, "Logged out"), "application/json");
    });

    // ----- GET routes -----
    svr.Get("/api/books", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(mtx);
        bool admin = sessions.count(tokenFrom(req)) > 0;
        res.set_content(booksJson(lib, admin), "application/json");
    });

    svr.Get("/api/members", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(mtx);
        if (sessions.count(tokenFrom(req)) == 0) {
            res.status = 401;
            res.set_content(jsonMsg(false, "Admin login required"), "application/json");
            return;
        }
        res.set_content(membersJson(lib), "application/json");
    });

    // ----- POST routes (admin only) -----
    svr.Post("/api/books", [&](const httplib::Request& req, httplib::Response& res) {
        run(req, res, [&] {
            lib.addBook(req.get_param_value("title"),
                        req.get_param_value("author"),
                        req.get_param_value("isbn"),
                        req.get_param_value("location"));
            return std::string("Book added successfully");
        });
    });

    svr.Post("/api/books/sample", [&](const httplib::Request& req, httplib::Response& res) {
        run(req, res, [&] { return addSamples(lib); });
    });

    svr.Post("/api/books/delete", [&](const httplib::Request& req, httplib::Response& res) {
        run(req, res, [&] {
            lib.removeBook(intParam(req, "id"));
            return std::string("Book deleted");
        });
    });

    svr.Post("/api/books/location", [&](const httplib::Request& req, httplib::Response& res) {
        run(req, res, [&] {
            lib.setLocation(intParam(req, "id"), req.get_param_value("location"));
            return std::string("Location updated");
        });
    });

    svr.Post("/api/members", [&](const httplib::Request& req, httplib::Response& res) {
        run(req, res, [&] {
            lib.addMember(req.get_param_value("type"), req.get_param_value("name"));
            return std::string("Member added successfully");
        });
    });

    svr.Post("/api/issue", [&](const httplib::Request& req, httplib::Response& res) {
        run(req, res, [&] {
            lib.issueBook(intParam(req, "bookId"), intParam(req, "memberId"));
            return std::string("Book issued successfully");
        });
    });

    svr.Post("/api/return", [&](const httplib::Request& req, httplib::Response& res) {
        run(req, res, [&] {
            int fine = lib.returnBook(intParam(req, "bookId"));
            if (fine > 0)
                return "Book returned. Late fine: Rs. " + std::to_string(fine);
            return std::string("Book returned. No fine.");
        });
    });
    

    svr.Post("/api/members/update", [&](const httplib::Request& req, httplib::Response& res) {
        run(req, res, [&] {
            lib.updateMember(intParam(req, "id"), req.get_param_value("name"));
            return std::string("Member name updated");
        });
    });

    svr.Post("/api/members/delete", [&](const httplib::Request& req, httplib::Response& res) {
        run(req, res, [&] {
            lib.removeMember(intParam(req, "id"));
            return std::string("Member deleted");
        });
    });

    std::cout << "Library server running at http://localhost:8080\n";
    std::cout << "Library server running at http://localhost:8080\n";
    std::cout << "Admin login enabled. Press Ctrl+C to stop.\n";
    svr.listen("0.0.0.0", 8080);
    return 0;
}