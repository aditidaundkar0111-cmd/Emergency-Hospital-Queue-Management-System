#include "FileManager.h"
#include "Hospital.h"
#include "WebServer.h"
#include <nlohmann/json.hpp>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <atomic>
#include <vector>
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
    transport.set_timeout_sec(2);

    auto protectedDashboard = transport.Get("/api/dashboard");
    assert(protectedDashboard && protectedDashboard->status == 401);
    auto registered = transport.Post("/api/auth/register",
        R"({"fullName":"API Test User","username":"api_test","password":"Api-test-password"})",
        "application/json");
    assert(registered && registered->status == 201);
    const Json registrationBody = Json::parse(registered->body);
    assert(registrationBody.at("user").size() == 2);
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
    const Json loginBody = Json::parse(login->body);
    assert(loginBody.at("user").size() == 2);
    const std::string token = loginBody.at("token").get<std::string>();
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
    assert(emptyPatients->get_header_value("Access-Control-Allow-Origin").empty());
    httplib::Headers localOrigin;
    localOrigin.insert(std::make_pair("Origin", "http://localhost:8080"));
    auto localCors = transport.Get("/api/patients", localOrigin);
    assert(localCors && localCors->get_header_value("Access-Control-Allow-Origin") == "http://localhost:8080");
    httplib::Headers foreignOrigin;
    foreignOrigin.insert(std::make_pair("Origin", "http://example.invalid"));
    auto foreignCors = transport.Get("/api/patients", foreignOrigin);
    assert(foreignCors && foreignCors->get_header_value("Access-Control-Allow-Origin").empty());

    auto regular = client.Post("/api/patients",
        R"({"id":101,"name":"Regular One","age":30,"condition":"Checkup","severity":1,"type":"regular"})",
        "application/json");
    assert(regular && regular->status == 201);
    assert(Json::parse(regular->body).at("waitingTime") == 0);
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
    auto populatedDashboard = client.Get("/api/dashboard");
    assert(populatedDashboard && Json::parse(populatedDashboard->body).at("averageWaitingTime") == 0.0);
    auto missingPatient = client.Get("/api/patients/999");
    assert(missingPatient && missingPatient->status == 404);

    auto queueResponse = client.Get("/api/queue");
    assert(queueResponse && queueResponse->status == 200);
    Json queue = Json::parse(queueResponse->body);
    assert(queue.size() == 2 && queue[0].at("id") == 102);
    assert(queue[0].at("waitingTime") == 0);
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
    auto dashboardWithDoctor = client.Get("/api/dashboard");
    assert(dashboardWithDoctor && Json::parse(dashboardWithDoctor->body).at("totalDoctors") == 1);

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
    assert(Json::parse(assigned->body).at("status") == "ASSIGNED");
    auto dashboardAssigned = client.Get("/api/dashboard");
    Json assignedStats = Json::parse(dashboardAssigned->body);
    assert(assignedStats.at("totalPatients") == 2 && assignedStats.at("waitingPatients") == 1);
    assert(assignedStats.at("assignedPatients") == 1 && assignedStats.at("underTreatment") == 0);
    auto assignedPatients = client.Get("/api/patients");
    bool assignedInPatientList = false;
    for (const Json& patient : Json::parse(assignedPatients->body))
        if (patient.at("id") == 102 && patient.at("status") == "ASSIGNED") assignedInPatientList = true;
    assert(assignedInPatientList);
    auto queueAfterAssignment = client.Get("/api/queue");
    Json waitingAfterAssignment = Json::parse(queueAfterAssignment->body);
    assert(waitingAfterAssignment.size() == 1 && waitingAfterAssignment[0].at("id") == 101);
    assert(Json::parse(client.Get("/api/doctors")->body)[0].at("availability") == "busy");
    auto busyDoctor = client.Post("/api/patients/101/assign-doctor",
        R"({"doctorId":1})", "application/json");
    assert(busyDoctor && busyDoctor->status == 409);
    auto started = client.Post("/api/patients/102/start-treatment", "{}", "application/json");
    assert(started && started->status == 200);
    auto afterStart = client.Get("/api/patients/102");
    assert(afterStart && Json::parse(afterStart->body).at("status") == "UNDER_TREATMENT");
    Json treatmentStats = Json::parse(client.Get("/api/dashboard")->body);
    assert(treatmentStats.at("assignedPatients") == 0 && treatmentStats.at("underTreatment") == 1);
    auto completed = client.Post("/api/patients/102/complete-treatment",
        R"({"doctorId":1,"diagnosis":"API diagnosis","description":"API treatment"})",
        "application/json");
    assert(completed && Json::parse(completed->body).at("status") == "COMPLETED");
    Json completedStats = Json::parse(client.Get("/api/dashboard")->body);
    assert(completedStats.at("assignedPatients") == 0 && completedStats.at("underTreatment") == 0);
    assert(completedStats.at("completedPatients") == 1);
    assert(Json::parse(client.Get("/api/doctors")->body)[0].at("availability") == "available");
    auto treatments = client.Get("/api/treatments");
    assert(treatments && Json::parse(treatments->body).size() == 1);
    auto saved = client.Post("/api/data/save", "{}", "application/json");
    assert(saved && saved->status == 200 && Json::parse(saved->body).at("success") == true);

    auto transientPatient = client.Post("/api/patients",
        R"({"id":103,"name":"Transient","age":20,"condition":"Temporary","severity":1,"type":"regular"})",
        "application/json");
    assert(transientPatient && transientPatient->status == 201);
    {
        std::ifstream savedPatients("data/patients.txt");
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(savedPatients, line)) {
            if (line.compare(0, 6, "P 101 ") == 0) {
                const std::size_t timestampSeparator = line.find_last_of(" \t");
                assert(timestampSeparator != std::string::npos);
                const long long simulatedArrival = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count() - 180;
                line = line.substr(0, timestampSeparator + 1) + std::to_string(simulatedArrival);
            }
            lines.push_back(line);
        }
        assert(!lines.empty());
        std::ofstream savedPatientsOut("data/patients.txt", std::ios::trunc);
        for (const std::string& savedLine : lines) savedPatientsOut << savedLine << '\n';
        assert(savedPatientsOut.good());
    }
    auto loaded = client.Post("/api/data/load", "{}", "application/json");
    assert(loaded && loaded->status == 200);
    assert(Json::parse(loaded->body).at("dashboard").at("totalPatients") == 2);
    auto afterLoad = client.Get("/api/patients");
    assert(afterLoad && Json::parse(afterLoad->body).size() == 2);
    auto agedPatient = client.Get("/api/patients/101");
    assert(agedPatient && Json::parse(agedPatient->body).at("waitingTime") >= 3);
    auto emptyQueue = client.Get("/api/queue");
    assert(emptyQueue && Json::parse(emptyQueue->body).size() == 1);
    auto queueEmptyCall = client.Post("/api/queue/call-next", "{}", "application/json");
    assert(queueEmptyCall && queueEmptyCall->status == 200);
    assert(Json::parse(queueEmptyCall->body).at("id") == 101);
    const int frozenApiWait = Json::parse(queueEmptyCall->body).at("waitingTime").get<int>();
    assert(frozenApiWait >= 3 && Json::parse(queueEmptyCall->body).at("status") == "CALLED");
    assert(Json::parse(client.Get("/api/patients/101")->body).at("waitingTime") == frozenApiWait);

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
