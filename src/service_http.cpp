#include "community/service.hpp"
#include <httplib.h>
namespace cd::service {
namespace {
std::string bearer(const httplib::Request &request) {
    auto value = request.get_header_value("Authorization");
    if (!value.starts_with("Bearer ") || value.size() > 128)
        return {};
    return value.substr(7);
}
json body(const httplib::Request &request) {
    auto type = request.get_header_value("Content-Type");
    if (type != "application/json" && !type.starts_with("application/json;"))
        throw Error(415, "content_type", "Use application/json");
    try {
        return json::parse(request.body, [](int depth, json::parse_event_t, json &) {
            if (depth > 32)
                throw Error(400, "invalid_json", "JSON nesting exceeds 32 levels");
            return true;
        });
    } catch (const json::exception &) {
        throw Error(400, "invalid_json", "Request is not valid JSON");
    }
}
void reply(httplib::Response &response, const json &data, int status = 200) {
    response.status = status;
    response.set_header("Cache-Control", "no-store");
    response.set_content(data.dump(), "application/json");
}
} // namespace
struct Server::Impl {
    Store &store;
    httplib::Server http;
    explicit Impl(Store &s) : store(s) {
        http.set_payload_max_length(65536);
        http.set_read_timeout(5, 0);
        http.set_write_timeout(5, 0);
        http.set_keep_alive_max_count(16);
        http.new_task_queue = [] { return new httplib::ThreadPool(4, 4, 64); };
        auto route = [&](auto operation) {
            return [operation](const httplib::Request &r, httplib::Response &p) {
                try {
                    reply(p, operation(r));
                } catch (const Error &e) {
                    reply(p, {{"error", e.code}, {"message", e.what()}}, e.status);
                } catch (const json::exception &) {
                    reply(p,
                          {{"error", "invalid_body"},
                           {"message", "Request fields have invalid types"}},
                          422);
                } catch (const std::exception &) {
                    reply(p,
                          {{"error", "service_error"},
                           {"message", "The service could not complete this request"}},
                          500);
                }
            };
        };
        http.Get("/healthz",
                 route([](const auto &) { return json{{"ok", true}, {"protocol", 1}}; }));
        http.Get("/v1/status", route([this](const auto &) { return store.status(); }));
        http.Get("/v1/campaign", route([this](const auto &) { return store.campaign(); }));
        http.Get("/v1/me", route([this](const auto &r) { return store.profile(bearer(r)); }));
        http.Put("/v1/me/loadout",
                 route([this](const auto &r) { return store.save_loadout(bearer(r), body(r)); }));
        http.Post("/v1/admin/players", route([this](const auto &r) {
                      store.require_admin(bearer(r));
                      auto j = body(r);
                      if (!j.is_object() || j.size() != 1 || !j.contains("name") ||
                          !j["name"].is_string())
                          throw Error(422, "invalid_name", "Provide a player name");
                      return store.issue_player(j["name"].template get<std::string>());
                  }));
        http.Post("/v1/admin/planets",
                  route([this](const auto &r) { return store.put_planet(bearer(r), body(r)); }));
        http.Post("/v1/admin/templates",
                  route([this](const auto &r) { return store.put_template(bearer(r), body(r)); }));
        http.Post("/v1/missions", route([this](const auto &r) {
                      return store.start_mission(bearer(r), body(r),
                                                 r.get_header_value("X-Community-Launcher"),
                                                 r.get_header_value("X-Community-Release-Set"));
                  }));
        http.Post(R"(/v1/missions/([a-f0-9]{32})/result)", route([this](const auto &r) {
                      return store.finish_mission(bearer(r), r.matches[1].str(), body(r));
                  }));
        http.set_error_handler([](const auto &, auto &p) {
            if (p.body.empty())
                reply(p, {{"error", "http_error"}, {"message", "Request could not be handled"}},
                      p.status);
        });
    }
};
Server::Server(Store &store) : impl(std::make_unique<Impl>(store)) {
}
Server::~Server() {
    stop();
}
int Server::bind(int port) {
    if (port < 0)
        port = impl->store.config().port;
    if (!port)
        return impl->http.bind_to_any_port(impl->store.config().host);
    return impl->http.bind_to_port(impl->store.config().host, port) ? port : -1;
}
bool Server::listen() {
    return impl->http.listen_after_bind();
}
void Server::stop() {
    impl->http.stop();
}
bool Server::running() const {
    return impl->http.is_running();
}
} // namespace cd::service
