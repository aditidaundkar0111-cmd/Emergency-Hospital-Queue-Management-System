#include "FileManager.h"
#include "Hospital.h"
#include "WebServer.h"
#include <nlohmann/json.hpp>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <atomic>
#if defined(_WIN32)
#include <direct.h>
#else
#include <unistd.h>
#endif

namespace {
class AuthorizedClient {
public:
    explicit AuthorizedClient(httplib::Client& client) : client_(client) {}
    void setToken(const std::string& token) { token_ = token; }
    std::shared_ptr<httplib::Response> Get(const char* path) {
        return client_.Get(path, headers());
    }
    std::shared_ptr<httplib::Response> Post(const char* path, const std::string& body,
                                            const char* contentType) {
        return client_.Post(path, headers(), body, contentType);
    }

private:
    httplib::Client& client_;
    std::string token_;
    httplib::Headers headers() const {
        httplib::Headers result;
        result.insert(std::make_pair("Authorization", "Bearer " + token_));
        return result;
    }
};

std::string currentDirectory() {
    char buffer[4096];
#if defined(_WIN32)
    if (_getcwd(buffer, sizeof(buffer)) == nullptr) throw std::runtime_error("getcwd failed");
#else
    if (getcwd(buffer, sizeof(buffer)) == nullptr) throw std::runtime_error("getcwd failed");
#endif
    return buffer;
}

bool changeDirectory(const std::string& path) {
#if defined(_WIN32)
    return _chdir(path.c_str()) == 0;
#else
    return chdir(path.c_str()) == 0;
#endif
}

void removeDirectory(const std::string& path) {
#if defined(_WIN32)
    _rmdir(path.c_str());
#else
    rmdir(path.c_str());
#endif
}
}

int main() {
    using Json = nlohmann::json;
    const std::string originalDirectory = currentDirectory();
    const std::string isolatedDirectory = "data/api_web_tests";
    std::remove((isolatedDirectory + "/users.txt").c_str());
    FileManager(isolatedDirectory).saveAll(Hospital());

    Hospital hospital;
    WebServer webServer(hospital, "web", originalDirectory + "/" + isolatedDirectory + "/users.txt");
    std::atomic<bool> listenReturned(false);
    std::thread serverThread([&] {
        webServer.listen("127.0.0.1", 18081);
        listenReturned = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    assert(!listenReturned.load());
    assert(changeDirectory(originalDirectory + "/" + isolatedDirectory));

    httplib::Client transport("127.0.0.1", 18081);
    transport.set_connection_timeout(1, 0);
    transport.set_read_timeout(2, 0);

    auto protectedDashboard = transport.Get("/api/dashboard");
    assert(protectedDashboard && protectedDashboard->status == 401);
    auto registered = transport.Post("/api/auth/register",
        R"({"fullName":"API Test User","username":"api_test","password":"Api-test-password"})",
        "application/json");
    assert(registered && registered->status == 201);
    auto duplicateAccount = transport.Post("/api/auth/register",
        R"({"fullName":"Duplicate","username":"API_TEST","password":"Api-test-password"})",
        "application/json");
    assert(duplicateAccount && duplicateAccount->status == 409);
    auto invalidLogin = transport.Post("/api/auth/login",
        R"({"username":"api_test","password":"wrong-password"})", "application/json");
    assert(invalidLogin && invalidLogin->status == 401);
    auto login = transport.Post("/api/auth/login",
        R"({"username":"api_test","password":"Api-test-password"})", "application/json");
    assert(login && login->status == 200);
    const std::string token = Json::parse(login->body).at("token").get<std::string>();
    AuthorizedClient client(transport);
    client.setToken(token);
    auto currentUser = client.Get("/api/auth/me");
    assert(currentUser && currentUser->status == 200);
    assert(Json::parse(currentUser->body).at("fullName") == "API Test User");

    auto dashboard = client.Get("/api/dashboard");
    assert(dashboard && dashboard->status == 200);
    assert(Json::parse(dashboard->body).at("totalPatients") == 0);
    auto emptyPatients = client.Get("/api/patients");
    assert(emptyPatients && emptyPatients->status == 200);
    assert(Json::parse(emptyPatients->body).is_array());
    assert(emptyPatients->get_header_value("Access-Control-Allow-Origin") == "*");

    auto regular = client.Post("/api/patients",
        R"({"id":101,"name":"Regular One","age":30,"condition":"Checkup","severity":1,"type":"regular"})",
        "application/json");
    assert(regular && regular->status == 201);
    auto duplicatePatient = client.Post("/api/patients",
        R"({"id":101,"name":"Duplicate","age":30,"condition":"Checkup","severity":1,"type":"regular"})",
        "application/json");
    assert(duplicatePatient && duplicatePatient->status == 409);

    auto emergency = client.Post("/api/patients",
        R"({"id":102,"name":"Emergency One","age":45,"condition":"Chest pain","severity":5,"type":"emergency","critical":true})",
        "application/json");
    assert(emergency && emergency->status == 201);
    auto patientListResponse = client.Get("/api/patients");
    assert(patientListResponse && Json::parse(patientListResponse->body).size() == 2);
    auto missingPatient = client.Get("/api/patients/999");
    assert(missingPatient && missingPatient->status == 404);

    auto queueResponse = client.Get("/api/queue");
    assert(queueResponse && queueResponse->status == 200);
    Json queue = Json::parse(queueResponse->body);
    assert(queue.size() == 2 && queue[0].at("id") == 102);
    auto noDoctors = client.Get("/api/doctors");
    assert(noDoctors && Json::parse(noDoctors->body).empty());
    auto doctor = client.Post("/api/doctors",
        R"({"id":1,"name":"Dr API","specialization":"Cardiology"})",
        "application/json");
    assert(doctor && doctor->status == 201);
    auto duplicateDoctor = client.Post("/api/doctors",
        R"({"id":1,"name":"Dr Duplicate","specialization":"General Physician"})",
        "application/json");
    assert(duplicateDoctor && duplicateDoctor->status == 409);
    auto doctors = client.Get("/api/doctors");
    assert(doctors && Json::parse(doctors->body).size() == 1);

    auto invalidDoctor = client.Post("/api/patients/102/assign-doctor",
        R"({"doctorId":999})", "application/json");
    assert(invalidDoctor && invalidDoctor->status == 404);
    auto invalidLifecycle = client.Post("/api/patients/101/start-treatment", "{}", "application/json");
    assert(invalidLifecycle && invalidLifecycle->status == 409);

    auto called = client.Post("/api/queue/call-next", "{}", "application/json");
    assert(called && Json::parse(called->body).at("id") == 102);
    auto assigned = client.Post("/api/patients/102/assign-doctor",
        R"({"doctorId":1})", "application/json");
    assert(assigned && assigned->status == 200);
    auto busyDoctor = client.Post("/api/patients/101/assign-doctor",
        R"({"doctorId":1})", "application/json");
    assert(busyDoctor && busyDoctor->status == 409);
    auto started = client.Post("/api/patients/102/start-treatment", "{}", "application/json");
    assert(started && started->status == 200);
    auto completed = client.Post("/api/patients/102/complete-treatment",
        R"({"doctorId":1,"diagnosis":"API diagnosis","description":"API treatment"})",
        "application/json");
    assert(completed && Json::parse(completed->body).at("status") == "COMPLETED");
    auto treatments = client.Get("/api/treatments");
    assert(treatments && Json::parse(treatments->body).size() == 1);
    auto saved = client.Post("/api/data/save", "{}", "application/json");
    assert(saved && saved->status == 200 && Json::parse(saved->body).at("success") == true);

    auto transientPatient = client.Post("/api/patients",
        R"({"id":103,"name":"Transient","age":20,"condition":"Temporary","severity":1,"type":"regular"})",
        "application/json");
    assert(transientPatient && transientPatient->status == 201);
    auto loaded = client.Post("/api/data/load", "{}", "application/json");
    assert(loaded && loaded->status == 200);
    assert(Json::parse(loaded->body).at("dashboard").at("totalPatients") == 2);
    auto afterLoad = client.Get("/api/patients");
    assert(afterLoad && Json::parse(afterLoad->body).size() == 2);
    auto emptyQueue = client.Get("/api/queue");
    assert(emptyQueue && Json::parse(emptyQueue->body).size() == 1);
    auto queueEmptyCall = client.Post("/api/queue/call-next", "{}", "application/json");
    assert(queueEmptyCall && queueEmptyCall->status == 200);
    assert(Json::parse(queueEmptyCall->body).at("id") == 101);

    auto loggedOut = client.Post("/api/auth/logout", "{}", "application/json");
    assert(loggedOut && loggedOut->status == 200);
    auto expiredSession = client.Get("/api/dashboard");
    assert(expiredSession && expiredSession->status == 401);

    webServer.stop();
    serverThread.join();
    assert(listenReturned.load());
    assert(changeDirectory(originalDirectory));
    std::remove("data/api_web_tests/data/patients.txt");
    std::remove("data/api_web_tests/data/doctors.txt");
    std::remove("data/api_web_tests/data/treatments.txt");
    removeDirectory("data/api_web_tests/data");
    std::remove("data/api_web_tests/patients.txt");
    std::remove("data/api_web_tests/doctors.txt");
    std::remove("data/api_web_tests/treatments.txt");
    std::remove("data/api_web_tests/users.txt");
    removeDirectory(isolatedDirectory);
    std::cout << "Web API checks passed.\n";
}
