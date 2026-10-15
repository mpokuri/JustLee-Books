
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <filesystem>
#include <sqlite3.h>

using namespace std;

// ============================================
// STRUCTURES AND ENUM
// ============================================

struct Book {
    string isbn;
    string title;
    string category;
    double retail = 0;
    double discount = 0;
};

struct Customer {
    int id = 0;
    string firstName;
    string lastName;
    string address;
    string city;
    string state;
    string zip;
};

struct CartItem {
    Book book;
    int quantity;
};

enum class Menu {
    Browse = 1,
    Search = 2,
    Details = 3,
    Customer = 4,
    Cart = 5,
    PlaceOrder = 6,
    History = 7,
    Categories = 8,
    Exit = 9
};

// ============================================
// DATABASE CONNECTION
// ============================================

sqlite3* db = nullptr;

bool connectDatabase(string databasePath) {

    if (!filesystem::exists(databasePath)) {
        cout << "Database file not found: "
             << databasePath << endl;
        return false;
    }

    int result = sqlite3_open_v2(
        databasePath.c_str(),
        &db,
        SQLITE_OPEN_READWRITE,
        nullptr
    );

    if (result != SQLITE_OK) {
        cout << "Database connection failed: "
             << sqlite3_errmsg(db) << endl;

        sqlite3_close(db);
        db = nullptr;
        return false;
    }

    char* error = nullptr;

    result = sqlite3_exec(
        db,
        "PRAGMA foreign_keys = ON;",
        nullptr,
        nullptr,
        &error
    );

    if (result != SQLITE_OK) {
        cout << "Database setup failed: "
             << (error ? error : "Unknown error")
             << endl;
        sqlite3_free(error);
        sqlite3_close(db);
        db = nullptr;
        return false;
    }

    cout << "Database connected successfully!"
         << endl;

    return true;
}

// ============================================
// DATABASE HELPER FUNCTIONS
// ============================================

string getText(sqlite3_stmt* stmt, int column) {

    const unsigned char* text =
        sqlite3_column_text(stmt, column);

    if (text == nullptr) {
        return "";
    }

    return reinterpret_cast<const char*>(text);
}

bool prepareSQL(const char* sql, sqlite3_stmt** stmt) {

    if (sqlite3_prepare_v2(
        db, sql, -1, stmt, nullptr
    ) == SQLITE_OK) {
        return true;
    }

    cout << "SQL error: " << sqlite3_errmsg(db)
         << endl;

    return false;
}

bool executeSQL(const char* sql) {

    char* error = nullptr;

    int result = sqlite3_exec(
        db, sql, nullptr, nullptr, &error
    );

    if (result != SQLITE_OK) {
        cout << "SQL error: "
             << (error ? error : "Unknown error")
             << endl;
        sqlite3_free(error);
        return false;
    }

    return true;
}

// ============================================
// INPUT FUNCTIONS
// ============================================

string askText(string message) {

    string value;

    cout << message;
    getline(cin, value);

    return value;
}

int askNumber(string message) {

    while (true) {

        string input = askText(message);

        try {
            size_t position = 0;
            int number = stoi(input, &position);

            if (position == input.length()) {
                return number;
            }

        } catch (...) {
        }

        cout << "Please enter a valid number.\n";
    }
}

// ============================================
// READ A BOOK FROM SQLITE
// ============================================

Book readBook(sqlite3_stmt* stmt) {

    Book book;

    book.isbn = getText(stmt, 0);
    book.title = getText(stmt, 1);
    book.category = getText(stmt, 2);

    book.retail = sqlite3_column_double(stmt, 3);
    book.discount = sqlite3_column_double(stmt, 4);

    return book;
}

// ============================================
// LOAD BOOKS INTO A VECTOR
// ============================================

vector<Book> loadBooks() {

    vector<Book> books;

    const char* sql =
        "SELECT ISBN, Title, Category, Retail, "
        "COALESCE(Discount, 0) "
        "FROM Books ORDER BY Title;";

    sqlite3_stmt* stmt = nullptr;

    if (!prepareSQL(sql, &stmt)) {
        return books;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {

        Book book = readBook(stmt);

        books.push_back(book);
    }

    sqlite3_finalize(stmt);

    return books;
}

// ============================================
// DISPLAY BOOKS
// ============================================

void displayBooks(vector<Book> books) {

    if (books.empty()) {
        cout << "No books found.\n";
        return;
    }

    cout << left
         << setw(13) << "ISBN"
         << setw(36) << "TITLE"
         << setw(15) << "CATEGORY"
         << right << setw(10) << "PRICE"
         << endl;

    cout << string(74, '-') << endl;

    for (Book book : books) {

        double price = book.retail - book.discount;

        cout << left
             << setw(13) << book.isbn
             << setw(36) << book.title.substr(0, 35)
             << setw(15) << book.category
             << right << "$"
             << setw(9) << price
             << endl;
    }
}

// ============================================
// SEARCH BOOKS
// ============================================

void searchBooks() {

    string keyword =
        askText("Enter title or ISBN to search: ");

    vector<Book> books = loadBooks();
    vector<Book> results;

    for (Book book : books) {

        if (book.title.find(keyword) != string::npos ||
            book.isbn.find(keyword) != string::npos) {

            results.push_back(book);
        }
    }

    displayBooks(results);
}

// ============================================
// FIND BOOK BY ISBN
// ============================================

bool findBook(string isbn, Book& selectedBook) {

    const char* sql =
        "SELECT ISBN, Title, Category, Retail, "
        "COALESCE(Discount, 0) "
        "FROM Books WHERE ISBN = ?;";

    sqlite3_stmt* stmt = nullptr;

    if (!prepareSQL(sql, &stmt)) {
        return false;
    }

    sqlite3_bind_text(
        stmt, 1, isbn.c_str(),
        -1, SQLITE_TRANSIENT
    );

    bool found = false;

    if (sqlite3_step(stmt) == SQLITE_ROW) {

        selectedBook = readBook(stmt);
        found = true;
    }

    sqlite3_finalize(stmt);

    return found;
}

// ============================================
// VIEW BOOK DETAILS
// ============================================

void viewBookDetails() {

    string isbn = askText("Enter ISBN: ");

    Book book;

    if (!findBook(isbn, book)) {
        cout << "Book not found.\n";
        return;
    }

    cout << "\n===== BOOK DETAILS =====\n";

    cout << "ISBN: " << book.isbn << endl;
    cout << "Title: " << book.title << endl;
    cout << "Category: " << book.category << endl;
    cout << "Retail: $" << book.retail << endl;
    cout << "Discount: $" << book.discount << endl;
    cout << "Final Price: $"
         << book.retail - book.discount << endl;

    const char* sql =
        "SELECT p.Name, b.PubDate "
        "FROM Books b "
        "LEFT JOIN Publisher p ON b.PubID = p.PubID "
        "WHERE b.ISBN = ?;";

    sqlite3_stmt* stmt = nullptr;

    if (prepareSQL(sql, &stmt)) {

        sqlite3_bind_text(
            stmt, 1, isbn.c_str(),
            -1, SQLITE_TRANSIENT
        );

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            cout << "Publisher: " << getText(stmt, 0)
                 << endl;
            cout << "Publication Date: "
                 << getText(stmt, 1) << endl;
        }

        sqlite3_finalize(stmt);
    }

    const char* authorSQL =
        "SELECT a.Fname, a.Lname "
        "FROM BookAuthor ba "
        "JOIN Author a ON ba.AuthorID = a.AuthorID "
        "WHERE ba.ISBN = ?;";

    if (prepareSQL(authorSQL, &stmt)) {

        sqlite3_bind_text(
            stmt, 1, isbn.c_str(),
            -1, SQLITE_TRANSIENT
        );

        cout << "Authors: ";

        bool hasAuthor = false;

        while (sqlite3_step(stmt) == SQLITE_ROW) {

            if (hasAuthor) {
                cout << ", ";
            }

            cout << getText(stmt, 0)
                 << " " << getText(stmt, 1);

            hasAuthor = true;
        }

        if (!hasAuthor) {
            cout << "Not available";
        }

        cout << endl;

        sqlite3_finalize(stmt);
    }
}

// ============================================
// CUSTOMER IDENTIFICATION
// ============================================

bool selectCustomer(Customer& customer) {

    int customerId = askNumber("Enter Customer ID: ");

    const char* sql =
        "SELECT \"Customer#\", FirstName, LastName, "
        "Address, City, State, Zip "
        "FROM Customers WHERE \"Customer#\" = ?;";

    sqlite3_stmt* stmt = nullptr;

    if (!prepareSQL(sql, &stmt)) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, customerId);

    bool found = false;

    if (sqlite3_step(stmt) == SQLITE_ROW) {

        Customer selected;

        selected.id = sqlite3_column_int(stmt, 0);
        selected.firstName = getText(stmt, 1);
        selected.lastName = getText(stmt, 2);
        selected.address = getText(stmt, 3);
        selected.city = getText(stmt, 4);
        selected.state = getText(stmt, 5);
        selected.zip = getText(stmt, 6);

        customer = selected;
        found = true;
    }

    sqlite3_finalize(stmt);

    if (found) {

        cout << "\nWelcome, "
             << customer.firstName << " "
             << customer.lastName << "!\n";

    } else {

        cout << "Customer not found.\n";
    }

    return found;
}

// ============================================
// SHOPPING CART
// ============================================

double calculateTotal(vector<CartItem> cart) {

    double total = 0;

    for (CartItem item : cart) {

        double price =
            item.book.retail - item.book.discount;

        total += price * item.quantity;
    }

    return total;
}

void displayCart(vector<CartItem> cart) {

    if (cart.empty()) {
        cout << "Shopping cart is empty.\n";
        return;
    }

    cout << "\n===== SHOPPING CART =====\n";

    for (CartItem item : cart) {

        double price =
            item.book.retail - item.book.discount;

        cout << item.book.title
             << " | Quantity: " << item.quantity
             << " | Subtotal: $"
             << price * item.quantity << endl;
    }

    cout << "\nTOTAL: $"
         << calculateTotal(cart) << endl;
}

void manageCart(vector<CartItem>& cart) {

    while (true) {

        cout << "\n===== CART MENU =====\n";
        cout << "1. View Cart\n";
        cout << "2. Add Book\n";
        cout << "3. Remove Book\n";
        cout << "4. Return to Main Menu\n";

        int choice = askNumber("Enter choice: ");

        if (choice == 1) {

            displayCart(cart);

        } else if (choice == 2) {

            string isbn = askText("Enter ISBN: ");

            Book book;

            if (!findBook(isbn, book)) {
                cout << "Book not found.\n";
                continue;
            }

            int quantity = askNumber("Enter quantity: ");

            if (quantity <= 0 || quantity > 999) {
                cout << "Quantity must be between 1 and 999.\n";
                continue;
            }

            bool exists = false;

            for (CartItem& item : cart) {

                if (item.book.isbn == isbn) {

                    if (item.quantity + quantity > 999) {
                        cout << "Maximum quantity is 999.\n";
                        exists = true;
                        break;
                    }

                    item.quantity += quantity;
                    exists = true;
                    break;
                }
            }

            if (!exists) {

                CartItem item;
                item.book = book;
                item.quantity = quantity;

                cart.push_back(item);
            }

            cout << "Cart updated.\n";

        } else if (choice == 3) {

            string isbn =
                askText("Enter ISBN to remove: ");

            bool removed = false;

            for (int i = 0; i < (int)cart.size(); i++) {

                if (cart[i].book.isbn == isbn) {

                    cart.erase(cart.begin() + i);
                    removed = true;
                    break;
                }
            }

            if (removed) {
                cout << "Book removed from cart.\n";
            } else {
                cout << "Book not found in cart.\n";
            }

        } else if (choice == 4) {

            break;

        } else {

            cout << "Invalid choice.\n";
        }
    }
}

// ============================================
// PLACE ORDER
// ============================================

void placeOrder(Customer customer, vector<CartItem>& cart) {

    if (customer.id == 0) {
        cout << "Please select a customer first.\n";
        return;
    }

    if (cart.empty()) {
        cout << "Your shopping cart is empty.\n";
        return;
    }

    displayCart(cart);

    string confirmation = askText("Type YES to place order: ");

    if (confirmation != "YES") {
        cout << "Order canceled.\n";
        return;
    }

    if (!executeSQL("BEGIN IMMEDIATE TRANSACTION;")) {
        return;
    }

    bool success = true;
    int orderId = 0;

    sqlite3_stmt* stmt = nullptr;

    // Find next available order number
    const char* maxSQL =
        "SELECT COALESCE(MAX(\"Order#\"), 999) + 1 "
        "FROM Orders;";

    if (prepareSQL(maxSQL, &stmt)) {

        if (sqlite3_step(stmt) == SQLITE_ROW) {

            orderId = sqlite3_column_int(stmt, 0);

        } else {

            success = false;
        }

        sqlite3_finalize(stmt);

    } else {

        success = false;
    }

    // Insert order information
    const char* orderSQL =
        "INSERT INTO Orders "
        "(\"Order#\", \"Customer#\", OrderDate, "
        "ShipDate, ShipStreet, ShipCity, "
        "ShipState, ShipZip, ShipCost) "
        "VALUES (?, ?, DATE('now'), NULL, "
        "?, ?, ?, ?, 0);";

    if (success && prepareSQL(orderSQL, &stmt)) {

        sqlite3_bind_int(stmt, 1, orderId);
        sqlite3_bind_int(stmt, 2, customer.id);

        sqlite3_bind_text(
            stmt, 3, customer.address.c_str(),
            -1, SQLITE_TRANSIENT
        );

        sqlite3_bind_text(
            stmt, 4, customer.city.c_str(),
            -1, SQLITE_TRANSIENT
        );

        sqlite3_bind_text(
            stmt, 5, customer.state.c_str(),
            -1, SQLITE_TRANSIENT
        );

        sqlite3_bind_text(
            stmt, 6, customer.zip.c_str(),
            -1, SQLITE_TRANSIENT
        );

        if (sqlite3_step(stmt) != SQLITE_DONE) {

            cout << sqlite3_errmsg(db) << endl;
            success = false;
        }

        sqlite3_finalize(stmt);

    } else if (success) {

        success = false;
    }

    // Insert all purchased books
    for (int i = 0; success && i < (int)cart.size(); i++) {

        Book currentBook;

        if (!findBook(cart[i].book.isbn, currentBook)) {
            success = false;
            break;
        }

        const char* itemSQL =
            "INSERT INTO OrderItems "
            "(\"Order#\", \"Item#\", ISBN, Quantity, PaidEach) "
            "VALUES (?, ?, ?, ?, ?);";

        if (!prepareSQL(itemSQL, &stmt)) {

            success = false;
            break;
        }

        sqlite3_bind_int(stmt, 1, orderId);
        sqlite3_bind_int(stmt, 2, i + 1);

        sqlite3_bind_text(
            stmt, 3, currentBook.isbn.c_str(),
            -1, SQLITE_TRANSIENT
        );

        sqlite3_bind_int(
            stmt, 4, cart[i].quantity
        );

        double price =
            currentBook.retail - currentBook.discount;

        sqlite3_bind_double(stmt, 5, price);

        if (sqlite3_step(stmt) != SQLITE_DONE) {

            cout << sqlite3_errmsg(db) << endl;
            success = false;
        }

        sqlite3_finalize(stmt);
    }

    if (success) {
        success = executeSQL("COMMIT;");
    }

    if (success) {

        cout << "\nOrder placed successfully!\n";
        cout << "Order Number: " << orderId << endl;
        cout << "Shipping: $0.00 (classroom demo)\n";

        cart.clear();

    } else {

        executeSQL("ROLLBACK;");

        cout << "Order could not be saved.\n";
    }
}

// ============================================
// CUSTOMER ORDER HISTORY
// ============================================

void viewOrderHistory(Customer customer) {

    if (customer.id == 0) {

        cout << "Please select a customer first.\n";
        return;
    }

    const char* sql =
        "SELECT o.\"Order#\", o.OrderDate, "
        "b.Title, oi.Quantity, oi.PaidEach "
        "FROM Orders o "
        "JOIN OrderItems oi "
        "ON o.\"Order#\" = oi.\"Order#\" "
        "JOIN Books b ON oi.ISBN = b.ISBN "
        "WHERE o.\"Customer#\" = ? "
        "ORDER BY o.\"Order#\", oi.\"Item#\";";

    sqlite3_stmt* stmt = nullptr;

    if (!prepareSQL(sql, &stmt)) {
        return;
    }

    sqlite3_bind_int(stmt, 1, customer.id);

    cout << "\n===== ORDER HISTORY =====\n";

    bool found = false;

    while (sqlite3_step(stmt) == SQLITE_ROW) {

        found = true;

        int orderId = sqlite3_column_int(stmt, 0);
        string orderDate = getText(stmt, 1);
        string title = getText(stmt, 2);

        int quantity = sqlite3_column_int(stmt, 3);

        double price = sqlite3_column_double(stmt, 4);

        cout << "Order #: " << orderId
             << " | Date: " << orderDate
             << " | Book: " << title
             << " | Qty: " << quantity
             << " | Total: $" << price * quantity
             << endl;
    }

    if (!found) {
        cout << "No orders found.\n";
    }

    sqlite3_finalize(stmt);
}

// ============================================
// BROWSE BY CATEGORY
// ============================================

void browseCategories() {

    vector<Book> books = loadBooks();

    vector<string> categories;

    // Build list of unique categories
    for (Book book : books) {

        bool exists = false;

        for (string category : categories) {

            if (category == book.category) {
                exists = true;
                break;
            }
        }

        if (!exists) {
            categories.push_back(book.category);
        }
    }

    cout << "\n===== BOOK CATEGORIES =====\n";

    for (int i = 0; i < (int)categories.size(); i++) {

        cout << i + 1 << ". "
             << categories[i] << endl;
    }

    int choice =
        askNumber("Select category number (0 to cancel): ");

    if (choice == 0) {
        return;
    }

    if (choice < 1 || choice > (int)categories.size()) {

        cout << "Invalid category.\n";
        return;
    }

    string selectedCategory = categories[choice - 1];

    vector<Book> matchingBooks;

    for (Book book : books) {

        if (book.category == selectedCategory) {
            matchingBooks.push_back(book);
        }
    }

    displayBooks(matchingBooks);
}

// ============================================
// MAIN FUNCTION
// ============================================

int main(int argc, char* argv[]) {

    cout << fixed << setprecision(2);

    // Update this if your actual database file has a different extension or location.
    string databasePath = "identifier.sqlite";

    // Optional: supply the full database path through CLion program arguments.
    if (argc > 1) {
        databasePath = argv[1];
    }

    cout << "Working directory: "
         << filesystem::current_path() << endl;

    cout << "Opening database: "
         << databasePath << endl;

    // If the database is not in the working
    // directory, also check its parent folder.
    if (!filesystem::exists(databasePath) && argc == 1) {

        if (filesystem::exists("../identifier.sqlite")) {
            databasePath = "../identifier.sqlite";

        } else if (filesystem::exists("Books.db")) {
            databasePath = "Books.db";

        } else if (filesystem::exists("../Books.db")) {
            databasePath = "../Books.db";
        }
    }

    if (!connectDatabase(databasePath)) {

        cout << "\nPlease check your database filename "
             << "and its location in CLion.\n";

        return 1;
    }

    Customer customer;
    vector<CartItem> cart;

    while (true) {

        cout << "\n====================================\n";
        cout << "        JUSTLEE BOOKSTORE\n";
        cout << "====================================\n";

        if (customer.id != 0) {

            cout << "Customer: "
                 << customer.firstName << " "
                 << customer.lastName << endl;

        } else {

            cout << "Customer: Not Selected\n";
        }

        cout << "\n1. Browse All Books\n";
        cout << "2. Search Books\n";
        cout << "3. View Book Details\n";
        cout << "4. Select Customer\n";
        cout << "5. Shopping Cart\n";
        cout << "6. Place an Order\n";
        cout << "7. View Order History\n";
        cout << "8. Browse by Category\n";
        cout << "9. Exit\n";

        cout << "====================================\n";

        int choice = askNumber("Enter choice: ");

        if (choice == (int)Menu::Browse) {

            vector<Book> books = loadBooks();
            displayBooks(books);

        } else if (choice == (int)Menu::Search) {

            searchBooks();

        } else if (choice == (int)Menu::Details) {

            viewBookDetails();

        } else if (choice == (int)Menu::Customer) {

            Customer previous = customer;

            if (selectCustomer(customer) &&
                previous.id != customer.id) {

                // Prevent one customer from purchasing
                // another customer's cart.
                cart.clear();
            }

        } else if (choice == (int)Menu::Cart) {

            manageCart(cart);

        } else if (choice == (int)Menu::PlaceOrder) {

            placeOrder(customer, cart);

        } else if (choice == (int)Menu::History) {

            viewOrderHistory(customer);

        } else if (choice == (int)Menu::Categories) {

            browseCategories();

        } else if (choice == (int)Menu::Exit) {

            break;

        } else {

            cout << "Invalid choice.\n";
        }
    }

    sqlite3_close(db);

    cout << "\nThank you for visiting JustLee Books!\n";

    return 0;
}
