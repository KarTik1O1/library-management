# Library Management System (C++ OOP + Web)

A library app with a C++ backend (OOP) and a browser frontend.

## Requirements
- Linux, macOS, or Windows with WSL (Ubuntu)
- g++ with C++17 support

## Run it
```bash
g++ -std=c++17 -Wall -Wextra -O2 -o library server.cpp library.cpp -pthread
export LIBRARY_ADMIN_PASSWORD='choose-your-own-password'
./library
```

Open http://localhost:8080 in your browser.

Visitors can browse books. Log in as admin to change anything.