#pragma once

#include <initializer_list>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace TestSupport {

struct Scenario {
    const char* name;
    void (*run)();
};

inline void fail(const char* expression, const char* file, int line, const std::string& detail = {}) {
    std::ostringstream message;
    message << file << ':' << line << ": assertion failed: " << expression;
    if (!detail.empty()) {
        message << " (" << detail << ')';
    }
    throw std::runtime_error(message.str());
}

template <typename Actual, typename Expected>
void expectEqual(const Actual& actual,
                 const Expected& expected,
                 const char* actualExpression,
                 const char* expectedExpression,
                 const char* file,
                 int line) {
    if (!(actual == expected)) {
        std::ostringstream detail;
        detail << actualExpression << '=' << actual << ", " << expectedExpression << '=' << expected;
        fail("values are equal", file, line, detail.str());
    }
}

inline int runSelected(int argc, char** argv, const std::initializer_list<Scenario> scenarios) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " SCENARIO\n";
        return 2;
    }
    for (const Scenario& scenario : scenarios) {
        if (scenario.name == std::string(argv[1])) {
            try {
                scenario.run();
                std::cout << "PASS " << scenario.name << '\n';
                return 0;
            } catch (const std::exception& error) {
                std::cerr << "FAIL " << scenario.name << ": " << error.what() << '\n';
                return 1;
            }
        }
    }
    std::cerr << "unknown scenario: " << argv[1] << '\n';
    return 2;
}

}  // namespace TestSupport

#define EXPECT_TRUE(value)                                                                                             \
    do {                                                                                                               \
        if (!(value)) {                                                                                                \
            TestSupport::fail(#value, __FILE__, __LINE__);                                                            \
        }                                                                                                              \
    } while (false)

#define EXPECT_FALSE(value) EXPECT_TRUE(!(value))

#define EXPECT_EQ(actual, expected)                                                                                    \
    TestSupport::expectEqual((actual), (expected), #actual, #expected, __FILE__, __LINE__)
