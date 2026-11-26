// Part 3 tests: input validation helpers.
#include <cmath>
#include <cstdio>

#include "input.hpp"

using namespace input;

static int failures = 0;
#define CHECK(cond)                                             \
    do {                                                        \
        if (!(cond)) {                                          \
            std::printf("FAIL line %d: %s\n", __LINE__, #cond); \
            ++failures;                                         \
        }                                                       \
    } while (0)

int main() {
    // Integers: valid, boundaries, and everything a user could type wrong.
    CHECK(parseInt("42", 1, 100) == 42);
    CHECK(parseInt("1", 1, 100) == 1);      // lower bound allowed
    CHECK(parseInt("100", 1, 100) == 100);  // upper bound allowed
    CHECK(!parseInt("0", 1, 100));          // below range
    CHECK(!parseInt("101", 1, 100));        // above range
    CHECK(!parseInt("", 1, 100));           // empty
    CHECK(!parseInt("12abc", 1, 100));      // trailing junk
    CHECK(!parseInt("abc", 1, 100));        // not a number
    CHECK(!parseInt("3.5", 1, 100));        // not an integer
    CHECK(!parseInt("99999999999999999999", 1, 100)); // overflow
    CHECK(parseInt("-5", -10, 10) == -5);   // negatives

    // Doubles.
    CHECK(parseDouble("2.5", 0, 10) == 2.5);
    CHECK(parseDouble("-0.75", -2, 2) == -0.75);
    CHECK(parseDouble("1e-3", 0, 1) == 0.001);
    CHECK(!parseDouble("11", 0, 10));
    CHECK(!parseDouble("1.5x", 0, 10));
    CHECK(!parseDouble("", 0, 10));
    CHECK(!parseDouble("nan", -1e9, 1e9));  // NaN must be rejected
    CHECK(!parseDouble("inf", -1e9, 1e9));  // infinity is out of range
    CHECK(!parseDouble("1e999", -1e9, 1e9)); // overflow

    // Tokeniser.
    auto t = split("  zoom   3  ");
    CHECK(t.size() == 2 && t[0] == "zoom" && t[1] == "3");
    CHECK(split("").empty());
    CHECK(split("   ").empty());
    CHECK(split("q").size() == 1);

    if (failures) std::printf("Part 3: %d test(s) FAILED\n", failures);
    else std::printf("Part 3: all tests passed\n");
    return failures ? 1 : 0;
}
