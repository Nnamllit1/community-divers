#include "community/service.hpp"
#include "community/version.hpp"
#include <sqlite3.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <mutex>
#include <set>
namespace cd::service {
namespace {
constexpr int application_id = 0x43445652;
std::string text(const json &j, const char *key, size_t max = 128) {
    if (!j.contains(key) || !j[key].is_string())
        throw Error(422, "invalid_field", std::string("Missing text field: ") + key);
    auto s = j[key].get<std::string>();
    if (s.empty() || s.size() > max ||
        std::any_of(s.begin(), s.end(), [](unsigned char c) { return c < 32; }))
        throw Error(422, "invalid_field", std::string("Invalid text field: ") + key);
    return s;
}
std::string id(const json &j, const char *key) {
    auto s = text(j, key, 96);
    if (!std::all_of(s.begin(), s.end(), [](unsigned char c) {
            return std::isalnum(c) || c == '-' || c == '_' || c == '.';
        }))
        throw Error(422, "invalid_id", "Invalid content identifier");
    return s;
}
int64_t integer(const json &j, const char *key, int64_t max) {
    if (!j.contains(key) || !j[key].is_number_integer())
        throw Error(422, "invalid_field", std::string("Missing integer: ") + key);
    auto n = j[key].get<int64_t>();
    if (n < 0 || n > max)
        throw Error(422, "invalid_field", std::string("Integer out of range: ") + key);
    return n;
}
void fields(const json &j, std::initializer_list<const char *> allowed) {
    if (!j.is_object())
        throw Error(422, "invalid_body", "Expected a JSON object");
    for (auto it = j.begin(); it != j.end(); ++it)
        if (std::none_of(allowed.begin(), allowed.end(),
                         [&](const char *s) { return it.key() == s; }))
            throw Error(422, "invalid_field", "Unexpected request field");
}
int64_t now() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}
void sql(sqlite3 *db, const char *statement) {
    if (sqlite3_exec(db, statement, nullptr, nullptr, nullptr) != SQLITE_OK)
        throw std::runtime_error("Service database operation failed");
}
struct Query {
    sqlite3_stmt *s = nullptr;
    Query(sqlite3 *db, const char *statement) {
        if (sqlite3_prepare_v2(db, statement, -1, &s, nullptr) != SQLITE_OK)
            throw std::runtime_error("Service database query failed");
    }
    ~Query() { sqlite3_finalize(s); }
    void bind(int i, const std::string &v) {
        if (sqlite3_bind_text(s, i, v.data(), static_cast<int>(v.size()), SQLITE_TRANSIENT) !=
            SQLITE_OK)
            throw std::runtime_error("Database binding failed");
    }
    void bind(int i, int64_t v) {
        if (sqlite3_bind_int64(s, i, v) != SQLITE_OK)
            throw std::runtime_error("Database binding failed");
    }
    bool row() {
        int r = sqlite3_step(s);
        if (r == SQLITE_ROW)
            return true;
        if (r == SQLITE_DONE)
            return false;
        throw std::runtime_error("Service database statement failed");
    }
    void done() {
        if (row())
            throw std::runtime_error("Unexpected database row");
    }
    std::string str(int i) {
        auto p = sqlite3_column_text(s, i);
        auto n = sqlite3_column_bytes(s, i);
        return p ? std::string(reinterpret_cast<const char *>(p), n) : std::string{};
    }
    int64_t num(int i) { return sqlite3_column_int64(s, i); }
};
struct Transaction {
    sqlite3 *db;
    bool committed = false;
    explicit Transaction(sqlite3 *p) : db(p) { sql(db, "BEGIN IMMEDIATE"); }
    ~Transaction() {
        if (!committed)
            sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
    }
    void commit() {
        sql(db, "COMMIT");
        committed = true;
    }
};
bool equal_secret(const std::string &a, const std::string &b) {
    unsigned diff = static_cast<unsigned>(a.size() ^ b.size());
    for (size_t i = 0; i < std::max(a.size(), b.size()); ++i)
        diff |= static_cast<unsigned>((i < a.size() ? a[i] : 0) ^ (i < b.size() ? b[i] : 0));
    return diff == 0;
}
void copy_private(const fs::path &from, const fs::path &to) {
    private_write_new(to, {});
    std::ifstream in(from, std::ios::binary);
    std::ofstream out(to, std::ios::binary | std::ios::trunc);
    if (!in || !out)
        throw std::runtime_error("Cannot copy service backup");
    out << in.rdbuf();
    out.flush();
    if (!in.eof() && in.fail())
        throw std::runtime_error("Cannot read service backup");
    if (!out)
        throw std::runtime_error("Cannot write service backup");
}
} // namespace
Config parse_config(const json &j) {
    if (j.at("schema") != 1)
        throw std::runtime_error("Unsupported service configuration schema");
    Config c;
    c.id = id(j, "id");
    c.name = text(j, "name");
    c.campaign = text(j, "campaign");
    if (j.contains("listen")) {
        c.host = text(j["listen"], "host");
        c.port = static_cast<int>(integer(j["listen"], "port", 65535));
    }
    if (c.host != "127.0.0.1" && c.host != "::1" && c.host != "localhost")
        throw std::runtime_error(
            "The service HTTP listener must use loopback behind an HTTPS proxy");
    if (!c.port)
        throw std::runtime_error("Service port must be between 1 and 65535");
    c.game_builds = j.at("game_builds").get<std::vector<std::string>>();
    c.required_release_set = j.value("required_release_set", "");
    c.required_launcher_release = j.value("required_launcher_release", "");
    json status{{"protocol", 1}, {"id", c.id}, {"name", c.name}, {"game_builds", c.game_builds}};
    if (!c.required_release_set.empty())
        status["required_release_set"] = c.required_release_set;
    if (!c.required_launcher_release.empty())
        status["required_launcher_release"] = c.required_launcher_release;
    parse_status(status.dump());
    return c;
}
json encode_config(const Config &c) {
    json j{{"schema", 1},
           {"id", c.id},
           {"name", c.name},
           {"campaign", c.campaign},
           {"game_builds", c.game_builds},
           {"listen", {{"host", c.host}, {"port", c.port}}}};
    if (!c.required_release_set.empty())
        j["required_release_set"] = c.required_release_set;
    if (!c.required_launcher_release.empty())
        j["required_launcher_release"] = c.required_launcher_release;
    return j;
}
struct Store::Impl {
    sqlite3 *db = nullptr;
    Config config;
    fs::path directory;
    std::string admin_hash;
    std::mutex mutex;
    ~Impl() {
        if (db)
            sqlite3_close(db);
    }
    std::string player(const std::string &token) {
        if (token.size() != 68 || !token.starts_with("cdp_"))
            throw Error(401, "unauthorized", "A valid player credential is required");
        Query q(db, "SELECT id FROM players WHERE token_hash=?");
        q.bind(1, sha256_text(token));
        if (!q.row())
            throw Error(401, "unauthorized", "A valid player credential is required");
        return q.str(0);
    }
    json profile_id(const std::string &pid) {
        Query q(db, "SELECT id,name,xp,missions_completed,loadout FROM players WHERE id=?");
        q.bind(1, pid);
        if (!q.row())
            throw Error(404, "not_found", "Player not found");
        return {{"id", q.str(0)},
                {"name", q.str(1)},
                {"xp", q.num(2)},
                {"missions_completed", q.num(3)},
                {"loadout", json::parse(q.str(4))}};
    }
    void event(const std::string &kind, const std::string &actor, const json &body) {
        Query q(db, "INSERT INTO events(created,kind,actor,body) VALUES(?,?,?,?)");
        q.bind(1, now());
        q.bind(2, kind);
        q.bind(3, actor);
        q.bind(4, body.dump());
        q.done();
        sql(db, "UPDATE community SET revision=revision+1 WHERE singleton=1");
    }
};
void initialize(const fs::path &dir, const Config &config) {
    parse_config(encode_config(config));
    private_directory(dir);
    private_write_new(dir / "config.json", encode_config(config).dump(2) + "\n");
    private_write_new(dir / "operator.token", random_hex(32));
    private_write_new(dir / "state.sqlite", {});
    Store store(dir, config, true);
}
Store::Store(const fs::path &dir, Config config, bool create) : impl(std::make_unique<Impl>()) {
    impl->directory = fs::absolute(dir);
    impl->config = parse_config(encode_config(config));
    auto token = read_text(dir / "operator.token", 128);
    if (token.size() != 64 ||
        !std::all_of(token.begin(), token.end(), [](unsigned char c) { return std::isxdigit(c); }))
        throw std::runtime_error("Invalid operator token file");
    impl->admin_hash = sha256_text(token);
    if (!fs::exists(dir / "state.sqlite"))
        throw std::runtime_error(
            "Service database is missing; initialize or restore it explicitly");
    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX;
    if (sqlite3_open_v2(to_utf8(dir / "state.sqlite").c_str(), &impl->db, flags, nullptr) !=
        SQLITE_OK)
        throw std::runtime_error("Cannot open service database");
    auto scalar = [&](const char *statement) {
        Query q(impl->db, statement);
        q.row();
        return q.num(0);
    };
    auto appid = scalar("PRAGMA application_id"), schema = scalar("PRAGMA user_version"),
         count = scalar("SELECT count(*) FROM sqlite_master WHERE type='table'");
    bool fresh = create && schema == 0 && appid == 0 && count == 0;
    if (!fresh && (appid != application_id || schema != 1))
        throw std::runtime_error("Database is not a supported Community Divers schema");
    sqlite3_busy_timeout(impl->db, 5000);
    sql(impl->db, "PRAGMA foreign_keys=ON");
    if (fresh) {
        Transaction t(impl->db);
        sql(impl->db, R"(
PRAGMA application_id=1128552018;
CREATE TABLE community(singleton INTEGER PRIMARY KEY CHECK(singleton=1),id TEXT NOT NULL,name TEXT NOT NULL,campaign TEXT NOT NULL,revision INTEGER NOT NULL DEFAULT 0,epoch INTEGER NOT NULL DEFAULT 1);
CREATE TABLE planets(id TEXT PRIMARY KEY,name TEXT NOT NULL,biome TEXT NOT NULL,faction TEXT NOT NULL,liberation INTEGER NOT NULL CHECK(liberation BETWEEN 0 AND 1000000));
CREATE TABLE templates(id TEXT PRIMARY KEY,name TEXT NOT NULL,reward_xp INTEGER NOT NULL CHECK(reward_xp BETWEEN 0 AND 1000000),war_points INTEGER NOT NULL CHECK(war_points BETWEEN 0 AND 1000000));
CREATE TABLE players(id TEXT PRIMARY KEY,name TEXT NOT NULL,token_hash TEXT UNIQUE NOT NULL,xp INTEGER NOT NULL DEFAULT 0 CHECK(xp>=0),missions_completed INTEGER NOT NULL DEFAULT 0,loadout TEXT NOT NULL DEFAULT '{}');
CREATE TABLE missions(id TEXT PRIMARY KEY,player TEXT NOT NULL REFERENCES players(id),planet TEXT NOT NULL REFERENCES planets(id),template TEXT NOT NULL REFERENCES templates(id),epoch INTEGER NOT NULL,state TEXT NOT NULL DEFAULT 'running',reward_xp INTEGER NOT NULL,war_points INTEGER NOT NULL,created INTEGER NOT NULL,expires INTEGER NOT NULL,receipt TEXT);
CREATE TABLE events(id INTEGER PRIMARY KEY AUTOINCREMENT,created INTEGER NOT NULL,kind TEXT NOT NULL,actor TEXT NOT NULL,body TEXT NOT NULL);
CREATE INDEX missions_player_created ON missions(player,created);
PRAGMA user_version=1;
)");
        Query q(impl->db, "INSERT INTO community(singleton,id,name,campaign)VALUES(1,?,?,?)");
        q.bind(1, config.id);
        q.bind(2, config.name);
        q.bind(3, config.campaign);
        q.done();
        t.commit();
    }
    {
        Query owner(impl->db, "SELECT id FROM community WHERE singleton=1");
        if (!owner.row() || owner.str(0) != config.id)
            throw std::runtime_error("Configuration belongs to a different community database");
    }
    sql(impl->db, "PRAGMA journal_mode=WAL");
    sql(impl->db, "PRAGMA synchronous=FULL");
}
Store::~Store() = default;
const Config &Store::config() const {
    return impl->config;
}
void Store::require_admin(const std::string &token) {
    if (token.size() != 64 || !equal_secret(sha256_text(token), impl->admin_hash))
        throw Error(403, "forbidden", "An operator credential is required");
}
json Store::status() {
    std::lock_guard lock(impl->mutex);
    Query q(impl->db, "SELECT name,campaign FROM community WHERE singleton=1");
    q.row();
    json j{{"protocol", 1},
           {"id", impl->config.id},
           {"name", q.str(0)},
           {"game_builds", impl->config.game_builds},
           {"campaign", {{"name", q.str(1)}}},
           {"capabilities", {"campaign.read", "profiles", "mission-ledger"}},
           {"service_version", version::release_id}};
    if (!impl->config.required_release_set.empty())
        j["required_release_set"] = impl->config.required_release_set;
    if (!impl->config.required_launcher_release.empty())
        j["required_launcher_release"] = impl->config.required_launcher_release;
    return j;
}
json Store::campaign() {
    std::lock_guard lock(impl->mutex);
    Query c(impl->db, "SELECT id,campaign,revision,epoch FROM community WHERE singleton=1");
    c.row();
    json j{{"schema", 1},
           {"community_id", c.str(0)},
           {"name", c.str(1)},
           {"revision", c.num(2)},
           {"epoch", c.num(3)},
           {"planets", json::array()},
           {"templates", json::array()}};
    Query p(impl->db, "SELECT id,name,biome,faction,liberation FROM planets ORDER BY id");
    while (p.row())
        j["planets"].push_back({{"id", p.str(0)},
                                {"name", p.str(1)},
                                {"biome_id", p.str(2)},
                                {"owner_faction", p.str(3)},
                                {"liberation", p.num(4)}});
    Query t(impl->db, "SELECT id,name,reward_xp,war_points FROM templates ORDER BY id");
    while (t.row())
        j["templates"].push_back({{"id", t.str(0)},
                                  {"name", t.str(1)},
                                  {"reward_xp", t.num(2)},
                                  {"war_points", t.num(3)}});
    return j;
}
json Store::issue_player(const std::string &name) {
    auto checked = text(json{{"name", name}}, "name");
    std::lock_guard lock(impl->mutex);
    auto pid = random_hex(16), token = "cdp_" + random_hex(32);
    Query q(impl->db, "INSERT INTO players(id,name,token_hash)VALUES(?,?,?)");
    q.bind(1, pid);
    q.bind(2, checked);
    q.bind(3, sha256_text(token));
    q.done();
    return {{"schema", 1},
            {"community_id", impl->config.id},
            {"player_id", pid},
            {"name", name},
            {"token", token}};
}
json Store::profile(const std::string &token) {
    std::lock_guard lock(impl->mutex);
    return impl->profile_id(impl->player(token));
}
json Store::save_loadout(const std::string &token, const json &body) {
    if (!body.is_object() || body.size() > 32 || body.dump().size() > 8192)
        throw Error(422, "invalid_loadout", "Loadout must be an object of at most 8 KiB");
    std::lock_guard lock(impl->mutex);
    auto pid = impl->player(token);
    Query q(impl->db, "UPDATE players SET loadout=? WHERE id=?");
    q.bind(1, body.dump());
    q.bind(2, pid);
    q.done();
    return impl->profile_id(pid);
}
json Store::put_planet(const std::string &admin, const json &body) {
    require_admin(admin);
    fields(body, {"id", "name", "biome_id", "owner_faction", "liberation"});
    auto key = id(body, "id"), name = text(body, "name"), biome = id(body, "biome_id"),
         faction = id(body, "owner_faction");
    auto progress = integer(body, "liberation", 1000000);
    std::lock_guard lock(impl->mutex);
    Transaction t(impl->db);
    Query q(impl->db, "INSERT INTO planets VALUES(?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET "
                      "name=excluded.name,biome=excluded.biome,faction=excluded.faction,liberation="
                      "excluded.liberation");
    q.bind(1, key);
    q.bind(2, name);
    q.bind(3, biome);
    q.bind(4, faction);
    q.bind(5, progress);
    q.done();
    impl->event("planet_configured", "operator", body);
    t.commit();
    return body;
}
json Store::put_template(const std::string &admin, const json &body) {
    require_admin(admin);
    fields(body, {"id", "name", "reward_xp", "war_points"});
    auto key = id(body, "id"), name = text(body, "name");
    auto xp = integer(body, "reward_xp", 1000000), points = integer(body, "war_points", 1000000);
    std::lock_guard lock(impl->mutex);
    Transaction t(impl->db);
    Query q(impl->db,
            "INSERT INTO templates VALUES(?,?,?,?) ON CONFLICT(id) DO UPDATE SET "
            "name=excluded.name,reward_xp=excluded.reward_xp,war_points=excluded.war_points");
    q.bind(1, key);
    q.bind(2, name);
    q.bind(3, xp);
    q.bind(4, points);
    q.done();
    impl->event("template_configured", "operator", body);
    t.commit();
    return body;
}
json Store::start_mission(const std::string &token, const json &body, const std::string &launcher,
                          const std::string &release_set) {
    fields(body, {"planet_id", "template_id"});
    auto planet = id(body, "planet_id"), templ = id(body, "template_id");
    if ((!impl->config.required_launcher_release.empty() &&
         launcher != impl->config.required_launcher_release) ||
        (!impl->config.required_release_set.empty() &&
         release_set != impl->config.required_release_set))
        throw Error(409, "version_mismatch",
                    "Use this community's required launcher and release set");
    std::lock_guard lock(impl->mutex);
    auto pid = impl->player(token);
    Transaction t(impl->db);
    Query q(impl->db, "SELECT t.reward_xp,t.war_points,c.epoch FROM templates t,community c WHERE "
                      "t.id=? AND EXISTS(SELECT 1 FROM planets WHERE id=?)");
    q.bind(1, templ);
    q.bind(2, planet);
    if (!q.row())
        throw Error(404, "content_not_found", "Planet or mission template is not configured");
    auto mid = random_hex(16);
    auto expires = now() + 86400;
    Query add(impl->db, "INSERT INTO "
                        "missions(id,player,planet,template,epoch,reward_xp,war_points,created,"
                        "expires)VALUES(?,?,?,?,?,?,?,?,?)");
    add.bind(1, mid);
    add.bind(2, pid);
    add.bind(3, planet);
    add.bind(4, templ);
    add.bind(5, q.num(2));
    add.bind(6, q.num(0));
    add.bind(7, q.num(1));
    add.bind(8, now());
    add.bind(9, expires);
    add.done();
    t.commit();
    return {{"id", mid},           {"player_id", pid},
            {"planet_id", planet}, {"template_id", templ},
            {"expires", expires},  {"state", "running"}};
}
json Store::finish_mission(const std::string &token, const std::string &mission, const json &body) {
    fields(body, {"outcome"});
    auto outcome = text(body, "outcome", 16);
    if (outcome != "success" && outcome != "failure" && outcome != "aborted")
        throw Error(422, "invalid_outcome", "Invalid mission outcome");
    std::lock_guard lock(impl->mutex);
    auto pid = impl->player(token);
    Transaction t(impl->db);
    Query q(impl->db, "SELECT player,planet,epoch,state,reward_xp,war_points,expires,receipt FROM "
                      "missions WHERE id=?");
    q.bind(1, mission);
    if (!q.row())
        throw Error(404, "not_found", "Mission not found");
    if (q.str(0) != pid)
        throw Error(403, "forbidden", "Mission belongs to another player");
    if (q.str(3) != "running") {
        if (q.str(3) != outcome)
            throw Error(409, "result_conflict", "A different result was already committed");
        auto receipt = json::parse(q.str(7));
        receipt["replayed"] = true;
        t.commit();
        return receipt;
    }
    Query campaign(impl->db, "SELECT epoch FROM community WHERE singleton=1");
    campaign.row();
    if (campaign.num(0) != q.num(2) || q.num(6) < now())
        throw Error(409, "stale_mission", "Mission expired or belongs to another campaign epoch");
    int64_t xp = outcome == "success" ? q.num(4) : 0, points = outcome == "success" ? q.num(5) : 0;
    Query reward(impl->db, "UPDATE players SET xp=xp+?,missions_completed=missions_completed+? "
                           "WHERE id=? AND xp<=9223372036853775807");
    reward.bind(1, xp);
    reward.bind(2, static_cast<int64_t>(outcome == "success" ? 1 : 0));
    reward.bind(3, pid);
    reward.done();
    if (sqlite3_changes(impl->db) != 1)
        throw Error(409, "progression_limit", "Profile progression limit reached");
    Query progress(impl->db, "SELECT liberation FROM planets WHERE id=?");
    progress.bind(1, q.str(1));
    if (!progress.row())
        throw Error(409, "content_not_found", "Mission planet is unavailable");
    points = std::min(points, 1000000 - progress.num(0));
    if (points > 0) {
        Query war(impl->db, "UPDATE planets SET liberation=liberation+?,faction=CASE WHEN "
                            "liberation+?>=1000000 THEN 'human' ELSE faction END WHERE id=?");
        war.bind(1, points);
        war.bind(2, points);
        war.bind(3, q.str(1));
        war.done();
    }
    impl->event("mission_result", pid,
                {{"mission_id", mission}, {"outcome", outcome}, {"war_points", points}});
    Query rev(impl->db, "SELECT revision FROM community WHERE singleton=1");
    rev.row();
    json receipt{{"mission_id", mission},
                 {"outcome", outcome},
                 {"xp_awarded", xp},
                 {"war_points", points},
                 {"campaign_revision", rev.num(0)},
                 {"replayed", false},
                 {"profile", impl->profile_id(pid)}};
    Query done(impl->db, "UPDATE missions SET state=?,receipt=? WHERE id=?");
    done.bind(1, outcome);
    done.bind(2, receipt.dump());
    done.bind(3, mission);
    done.done();
    t.commit();
    return receipt;
}
void Store::backup(const fs::path &destination) {
    std::lock_guard lock(impl->mutex);
    private_directory(destination);
    private_write_new(destination / "state.sqlite", {});
    sqlite3 *copy = nullptr;
    if (sqlite3_open_v2(to_utf8(destination / "state.sqlite").c_str(), &copy, SQLITE_OPEN_READWRITE,
                        nullptr) != SQLITE_OK) {
        if (copy)
            sqlite3_close(copy);
        throw std::runtime_error("Cannot open backup database");
    }
    auto job = sqlite3_backup_init(copy, "main", impl->db, "main");
    int result = job ? sqlite3_backup_step(job, -1) : SQLITE_ERROR;
    if (job)
        sqlite3_backup_finish(job);
    if (result != SQLITE_DONE) {
        sqlite3_close(copy);
        throw std::runtime_error("Service backup failed");
    }
    try {
        Query check(copy, "PRAGMA integrity_check");
        if (!check.row() || check.str(0) != "ok")
            throw std::runtime_error("Backup integrity check failed");
    } catch (...) {
        sqlite3_close(copy);
        throw;
    }
    sqlite3_close(copy);
    copy_private(impl->directory / "operator.token", destination / "operator.token");
    private_write_new(destination / "config.json", encode_config(impl->config).dump(2) + "\n");
    private_write_new(destination / "backup.json",
                      json{{"schema", 1},
                           {"community_id", impl->config.id},
                           {"database_sha256", sha256_file(destination / "state.sqlite")},
                           {"operator_sha256", sha256_file(destination / "operator.token")},
                           {"config_sha256", sha256_file(destination / "config.json")}}
                              .dump(2) +
                          "\n");
}
void restore(const fs::path &backup, const fs::path &destination) {
    auto meta = json::parse(read_text(backup / "backup.json"));
    if (meta.at("schema") != 1 ||
        meta.at("database_sha256").get<std::string>() != sha256_file(backup / "state.sqlite") ||
        meta.at("operator_sha256").get<std::string>() != sha256_file(backup / "operator.token") ||
        meta.at("config_sha256").get<std::string>() != sha256_file(backup / "config.json"))
        throw std::runtime_error("Backup checksum verification failed");
    auto config = parse_config(json::parse(read_text(backup / "config.json")));
    if (config.id != meta.at("community_id").get<std::string>())
        throw std::runtime_error("Backup community identity mismatch");
    private_directory(destination);
    for (const char *file : {"state.sqlite", "operator.token", "config.json"})
        copy_private(backup / file, destination / file);
    Store verify(destination, config);
}
} // namespace cd::service
