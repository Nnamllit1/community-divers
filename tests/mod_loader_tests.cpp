#include "community/core.hpp"
#include "community/mod_loader.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace cd;
void require(bool v, const char *message) {
    if (!v)
        throw std::runtime_error(message);
}
template <class F> void rejects(F f) {
    bool bad = false;
    try {
        f();
    } catch (const std::exception &) {
        bad = true;
    }
    require(bad, "Expected invalid input to be rejected");
}
void write(const fs::path &p, const std::string &text) {
    fs::create_directories(p.parent_path());
    std::ofstream(p) << text;
}
int main() {
    auto root = fs::temp_directory_path() /
                ("community-divers-mods-" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int passed = 0;
    auto test = [&](const char *name, auto body) {
        body();
        ++passed;
        std::cout << "PASS " << name << '\n';
    };
    try {
        test("mod metadata and duplicate IDs", [&] {
            auto mods = root / "mods";
            for (auto name : {"a", "b"})
                write(mods / name / "mod.json",
                      R"({"schema":1,"id":"duplicate","name":"A","version":"1"})");
            auto found = discover_mods(mods);
            require(found.size() == 2 && !found[0].error.empty() && !found[1].error.empty(),
                    "Duplicate mod IDs not flagged");
        });
        fs::remove_all(root);
        std::cout << passed << " mod-loader checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << "\nFixture: " << root << '\n';
        return 1;
    }
}
