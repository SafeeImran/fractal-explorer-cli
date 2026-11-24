// Part 2 tests: ASCII rendering, colour mapping and PPM export.
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

#include "fractal.hpp"
#include "output.hpp"

using namespace fractal;

static int failures = 0;
#define CHECK(cond)                                             \
    do {                                                        \
        if (!(cond)) {                                          \
            std::printf("FAIL line %d: %s\n", __LINE__, #cond); \
            ++failures;                                         \
        }                                                       \
    } while (0)

static int countChar(const std::string& s, char c) {
    int n = 0;
    for (char x : s) n += x == c;
    return n;
}

int main() {
    // Points inside the set are black; escaped points are not.
    CHECK(output::colourFor(100, 100).r == 0 && output::colourFor(100, 100).g == 0 && output::colourFor(100, 100).b == 0);
    auto c = output::colourFor(50, 100);
    CHECK(c.r + c.g + c.b > 0);

    // ASCII: 5 lines of 20 characters plus newline; set interior drawn as spaces.
    {
        Settings s;
        Grid g(20, 5);
        renderSerial(g, View{}, s, 2.0);
        std::string a = output::toAscii(g, s.maxIter, false);
        CHECK(countChar(a, '\n') == 5);
        CHECK(a.size() == 5 * 21);
        CHECK(a.find(' ') != std::string::npos); // some pixels are inside the set
    }
    // Colour mode adds escape codes; plain mode has none.
    {
        Settings s;
        Grid g(10, 4);
        renderSerial(g, View{}, s, 2.0);
        CHECK(output::toAscii(g, s.maxIter, false).find('\x1b') == std::string::npos);
        CHECK(output::toAscii(g, s.maxIter, true).find('\x1b') != std::string::npos);
    }
    // Banded output has the same shape as the plain one.
    {
        Settings s;
        Grid g(30, 6);
        renderSerial(g, View{}, s, 2.0);
        std::string b = output::toAsciiBands(g, s.maxIter, 32, false);
        CHECK(countChar(b, '\n') == 6 && b.size() == 6 * 31);
    }
    // PPM: correct header and exactly width*height*3 bytes of pixel data.
    {
        Settings s;
        Grid g(12, 7);
        renderSerial(g, View{}, s, 1.0);
        std::string err;
        const char* path = "test_output_tmp.ppm";
        CHECK(output::writePPM(path, g, s.maxIter, err));
        std::ifstream f(path, std::ios::binary);
        std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        f.close();
        std::remove(path);
        std::string header = "P6\n12 7\n255\n";
        CHECK(data.compare(0, header.size(), header) == 0);
        CHECK(data.size() == header.size() + 12 * 7 * 3);
    }
    // Writing to an impossible path fails cleanly with a message.
    {
        Grid g(2, 2);
        std::string err;
        CHECK(!output::writePPM("no_such_folder_xyz/out.ppm", g, 10, err));
        CHECK(!err.empty());
    }

    if (failures) std::printf("Part 2: %d test(s) FAILED\n", failures);
    else std::printf("Part 2: all tests passed\n");
    return failures ? 1 : 0;
}
