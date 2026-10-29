#include "community/preparation.hpp"
#include <iostream>
#include <stdexcept>
using namespace cd;
namespace {
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
template <typename F> void rejects(F action) {
    try {
        action();
    } catch (const std::exception &) {
        return;
    }
    throw std::runtime_error("Invalid prepared identity was accepted");
}
} // namespace
int main() {
    try {
        PreparationPlan plan;
        plan.build = "12345";
        plan.bytes = 12;
        plan.depots = json::array({{{"depot", "99"}, {"manifest", "88"}}});
        ClientIdentity identity;
        identity.installation.valid = true;
        for (auto path : client_binary_paths) {
            plan.files.push_back({std::string(path), std::string(40, 'a'), 3});
            identity.files.push_back({std::string(path), std::string(64, 'a'), {}, 3});
        }
        auto receipt = preparation_report(plan);
        receipt["state"] = "copied";
        receipt["binary_fingerprints"] = json::object();
        for (const auto &f : identity.files)
            receipt["binary_fingerprints"][f.path] = {{"sha256", f.sha256}, {"bytes", f.bytes}};
        require(prepared_identity_build(receipt.dump(), identity.files) == "12345",
                "Prepared build not identified without Steam metadata");
        auto changed = identity.files;
        changed[0].sha256 = std::string(64, 'b');
        rejects([&] { prepared_identity_build(receipt.dump(), changed); });
        changed = identity.files;
        changed[0].bytes++;
        rejects([&] { prepared_identity_build(receipt.dump(), changed); });
        changed = identity.files;
        changed[0].error = "unreadable";
        rejects([&] { prepared_identity_build(receipt.dump(), changed); });
        changed = identity.files;
        changed[1] = changed[0];
        rejects([&] { prepared_identity_build(receipt.dump(), changed); });
        changed.pop_back();
        rejects([&] { prepared_identity_build(receipt.dump(), changed); });
        std::cout
            << "PASS preparation identity binds build to all four current binary fingerprints\n";
        for (const char *field : {"integration_available", "profile_isolation_verified",
                                  "official_network_isolation_verified"}) {
            auto invalid = receipt;
            invalid[field] = true;
            rejects([&] { prepared_identity_build(invalid.dump(), identity.files); });
        }
        for (const char *state : {"planned", "verified_copy", "unknown"}) {
            auto invalid = receipt;
            invalid["state"] = state;
            rejects([&] { prepared_identity_build(invalid.dump(), identity.files); });
        }
        for (const auto &build : {"", "12345text", "../12345", "123456789012345678901"}) {
            auto invalid = receipt;
            invalid["steam_build"] = build;
            rejects([&] { prepared_identity_build(invalid.dump(), identity.files); });
        }
        auto invalid = receipt;
        invalid["binary_fingerprints"]["bin/helldivers2.exe"]["bytes"] = -1;
        rejects([&] { prepared_identity_build(invalid.dump(), identity.files); });
        invalid = receipt;
        invalid["binary_fingerprints"].erase("bin/helldivers2.exe");
        rejects([&] { prepared_identity_build(invalid.dump(), identity.files); });
        auto duplicate = receipt.dump();
        duplicate.insert(1, "\"steam_build\":\"54321\",");
        rejects([&] { prepared_identity_build(duplicate, identity.files); });
        auto nested = "{\"ignored\":" + std::string(40, '[') + "0" + std::string(40, ']') + "}";
        rejects([&] { prepared_identity_build(nested, identity.files); });
        rejects([&] {
            prepared_identity_build(std::string(32 * 1024 * 1024 + 1, ' '), identity.files);
        });
        std::cout << "PASS receipt schema, duplicate keys, depth and size limits\n";
        identity.installation.build = prepared_identity_build(receipt.dump(), identity.files);
        identity.build_source = "preparation_receipt";
        json startup_files = json::object();
        for (auto path : client_startup_paths) {
            identity.startup_files.push_back({std::string(path), std::string(64, 'c'), {}, 7});
            startup_files[std::string(path)] = {{"sha256", std::string(64, 'c')}, {"bytes", 7}};
        }
        json catalog{{"schema", 1},
                     {"profiles", json::array({{{"id", "fixture"},
                                                {"steam_build", plan.build},
                                                {"files", receipt["binary_fingerprints"]},
                                                {"startup_files", startup_files}}})}};
        auto report = identity_report(identity, catalog);
        require(report["identity"]["known"] == true &&
                    report["build_source"] == "preparation_receipt" &&
                    report["integration"]["available"] == false,
                "Prepared identity granted integration or lost provenance");
        require(report["files"].size() == 4 && report["startup_files"].size() == 4 &&
                    receipt["binary_fingerprints"].size() == 4 &&
                    !receipt.contains("startup_files") &&
                    prepared_identity_build(receipt.dump(), identity.files) == "12345",
                "Startup inspection changed the four-file preparation receipt contract");
        const auto unknown = [&](const ClientIdentity &candidate, const json &profiles) {
            const auto value = identity_report(candidate, profiles);
            return value["identity"]["known"] == false &&
                   value["integration"]["available"] == false &&
                   value["steam_build"] == "12345" &&
                   value["build_source"] == "preparation_receipt";
        };
        for (size_t i = 0; i < client_startup_paths.size(); ++i) {
            auto altered = identity;
            altered.startup_files[i].sha256 = std::string(64, 'd');
            require(unknown(altered, catalog), "Changed startup content matched an observed build");
            altered = identity;
            altered.startup_files[i].bytes++;
            require(unknown(altered, catalog), "Changed startup size matched an observed build");
            altered = identity;
            altered.startup_files[i].error = "unreadable";
            auto error_report = identity_report(altered, catalog);
            require(unknown(altered, catalog) &&
                        error_report["startup_files"][i]["error"] == "unreadable",
                    "Unreadable startup module was hidden or accepted");
            altered = identity;
            altered.startup_files.erase(altered.startup_files.begin() + i);
            require(unknown(altered, catalog), "Missing startup module matched an observed build");
        }
        auto altered = identity;
        altered.startup_files[1] = altered.startup_files[0];
        require(unknown(altered, catalog), "Duplicate startup paths matched an observed build");
        altered = identity;
        altered.startup_files[0].path = "bin/unrelated.dll";
        require(unknown(altered, catalog), "An unrelated startup path matched an observed build");
        altered = identity;
        altered.startup_files[0].sha256 = std::string(64, 'C');
        require(unknown(altered, catalog), "Malformed startup digest matched an observed build");
        auto legacy_catalog = catalog;
        legacy_catalog["profiles"][0].erase("startup_files");
        require(unknown(identity, legacy_catalog), "Four-file catalog established startup identity");
        auto partial_catalog = catalog;
        partial_catalog["profiles"][0]["startup_files"].erase(std::string(client_startup_paths[0]));
        require(unknown(identity, partial_catalog), "Partial startup catalog matched an observed build");
        std::cout << "PASS startup dependency coverage and failures never grant readiness\n";
        identity.metadata_error = "Conflicting metadata";
        report = identity_report(identity, catalog);
        require(report["identity"]["known"] == false && report.contains("metadata_error"),
                "Metadata errors still matched the catalog");
        std::cout << "PASS report provenance and conflict rejection never grant integration\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
