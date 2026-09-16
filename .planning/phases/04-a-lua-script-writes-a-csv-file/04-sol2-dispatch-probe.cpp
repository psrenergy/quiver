#include <sol/sol.hpp>
#include <cstdio>
#include <string>
int main() {
    sol::state lua;
    lua.open_libraries(sol::lib::base);

    std::printf("=== CELL dispatch: is<> under SOL_SAFE_NUMERICS=1 ===\n");
    lua.script(R"(cells = { {"0012"}, {"1e3"}, {"12"}, {42}, {3.5}, {true} })");
    sol::table rows = lua["cells"];
    std::printf("%-10s | lua type | is<i64> | is<dbl> | is<str>\n", "cell");
    for (auto& r : rows) {
        sol::object v = r.second.as<sol::table>()[1];
        const char* t = v.get_type() == sol::type::string ? "string"
                      : v.get_type() == sol::type::number ? "number"
                      : v.get_type() == sol::type::boolean ? "boolean" : "other";
        std::string shown = (v.get_type() == sol::type::string) ? v.as<std::string>()
                          : (v.get_type() == sol::type::boolean) ? (v.as<bool>() ? "true":"false")
                          : (v.is<std::int64_t>() ? std::to_string(v.as<std::int64_t>()) : "3.5");
        std::printf("%-10s | %-8s |    %d    |    %d    |    %d\n",
            shown.c_str(), t, (int)v.is<std::int64_t>(), (int)v.is<double>(), (int)v.is<std::string>());
    }

    std::printf("\n=== KEY walk (FMT-08): is a STRING key \"3\" accepted as index 3? ===\n");
    lua.script(R"(holes = { [1]="a", ["3"]="c", [5]="e" })");
    sol::table holes = lua["holes"];
    for (auto& kv : holes) {
        const char* t = kv.first.get_type() == sol::type::string ? "string" : "number";
        std::printf("  key type=%-7s  is<int64>=%d  %s\n", t, (int)kv.first.is<std::int64_t>(),
            kv.first.is<std::int64_t>() ? "-> COUNTS as an index" : "-> rejected (non-integer key)");
    }
    return 0;
}
