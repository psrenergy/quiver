// Probe: what does std::to_chars actually DO with a non-finite double?
// CONTEXT.md flags this as the single MEDIUM-confidence claim in the research body:
// "verified against standards text, never executed."
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <system_error>

static void probe(const char* name, double v) {
    char buf[32];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), v);
    std::printf("%-28s isfinite=%d  ec=%s  len=%zd  text=[%.*s]\n",
                name,
                std::isfinite(v) ? 1 : 0,
                ec == std::errc{} ? "OK" : (ec == std::errc::value_too_large ? "value_too_large" : "OTHER"),
                (ec == std::errc{}) ? (ptr - buf) : (ptrdiff_t)-1,
                (int)((ec == std::errc{}) ? (ptr - buf) : 0), buf);
}

int main() {
    std::printf("=== compiler ===\n");
#ifdef _MSC_VER
    std::printf("MSVC _MSC_VER=%d\n", _MSC_VER);
#endif
    std::printf("_cplusplus=%ld\n", (long)__cplusplus);

    std::printf("\n=== non-finite doubles through std::to_chars ===\n");
    double zero = 0.0, one = 1.0;
    probe("0.0/0.0  (NaN)",        zero / zero);
    probe("1.0/0.0  (+inf)",       one / zero);
    probe("-1.0/0.0 (-inf)",       -one / zero);
    probe("-(0.0/0.0) (-NaN)",     -(zero / zero));
    probe("nan(\"\")",             std::nan(""));

    std::printf("\n=== the finite values the phase actually cares about ===\n");
    probe("2014.0",   2014.0);
    probe("0.1",      0.1);
    probe("1e300",    1e300);
    probe("1e-300",   1e-300);

    std::printf("\n=== int64 path (TEST-07: the digit that disappears via double) ===\n");
    {
        char b[32];
        long long n = 9007199254740993LL;
        auto [p, e] = std::to_chars(b, b + sizeof(b), n);
        std::printf("int64  9007199254740993 -> [%.*s]\n", (int)(p - b), b);
        double d = (double)n;
        auto [p2, e2] = std::to_chars(b, b + sizeof(b), d);
        std::printf("double 9007199254740993 -> [%.*s]   <-- the corruption FMT-04 prevents\n", (int)(p2 - b), b);
    }

    std::printf("\n=== 32-byte buffer adequacy (append_number's stack buffer) ===\n");
    {
        // Worst case shortest-round-trip double: 17 sig digits + sign + '.' + 'e' + sign + 3 exp digits
        char b[32];
        double worst = -1.7976931348623157e308;
        auto [p, e] = std::to_chars(b, b + sizeof(b), worst);
        std::printf("-DBL_MAX len=%zd (buffer=32) ec=%s -> [%.*s]\n",
                    p - b, e == std::errc{} ? "OK" : "FAIL", (int)(p - b), b);
        double denorm = 5e-324;
        auto [p2, e2] = std::to_chars(b, b + sizeof(b), denorm);
        std::printf("min denormal len=%zd ec=%s -> [%.*s]\n",
                    p2 - b, e2 == std::errc{} ? "OK" : "FAIL", (int)(p2 - b), b);
    }
    return 0;
}
