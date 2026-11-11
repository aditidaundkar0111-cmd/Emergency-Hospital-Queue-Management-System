#include "WebServer.h"
#include "EmergencyPatient.h"
#include "RegularPatient.h"
#include <nlohmann/json.hpp>
#include <cctype>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace {
using Json = nlohmann::json;
bool allowedOrigin(const httplib::Request& request) {
    const std::string origin = request.get_header_value("Origin");
    if (origin == "http://localhost:8080" || origin == "http://127.0.0.1:8080")
        return true;
    return false;
}

void sendJson(const httplib::Request& request, httplib::Response& response, int status, const Json& body) {
    if (allowedOrigin(request)) response.set_header("Access-Control-Allow-Origin", request.get_header_value("Origin"));
    response.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    response.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    response.set_header("Cache-Control", "no-store");
    response.status = status;
    response.set_content(body.dump(), "application/json; charset=utf-8");
}

void setCorsHeaders(const httplib::Request& request, httplib::Response& response) {
    if (allowedOrigin(request)) response.set_header("Access-Control-Allow-Origin", request.get_header_value("Origin"));
    response.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    response.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
}

std::string sessionToken(const httplib::Request& request) {
    const std::string authorization = request.get_header_value("Authorization");
    const std::string prefix = "Bearer ";
    return authorization.compare(0, prefix.size(), prefix) == 0
        ? authorization.substr(prefix.size()) : std::string();
}

bool requireAuth(const UserManager& users, const httplib::Request& request,
                 httplib::Response& response) {
    AuthUser user;
    if (users.findSession(sessionToken(request), user)) return true;
    sendJson(request, response, 401, Json{{"error", "Please sign in to continue."}});
    return false;
}

Json authUserJson(const AuthUser& user) {
    return Json{{"fullName", user.fullName}, {"username", user.username}};
}

Json errorJson(const std::string& message) {
    return Json{{"error", message}};
}

int errorStatus(const std::exception& error) {
    const std::string message = error.what();
    if (message.find("Invalid username/email or password") != std::string::npos) return 401;
    if (dynamic_cast<const std::logic_error*>(&error) != nullptr) return 409;
    if (message.find("already registered") != std::string::npos ||
        message.find("Duplicate") != std::string::npos ||
        message.find("duplicate") != std::string::npos) return 409;
    if (message.find("not found") != std::string::npos ||
        message.find("was not found") != std::string::npos) return 404;
    if (dynamic_cast<const std::invalid_argument*>(&error) != nullptr) return 400;
    return 500;
}

template <typename Handler>
void guarded(const httplib::Request& request, httplib::Response& response, Handler handler) {
    try {
        handler();
    } catch (const Json::exception& error) {
        sendJson(request, response, 400, errorJson(std::string("Invalid JSON request: ") + error.what()));
    } catch (const std::exception& error) {
        sendJson(request, response, errorStatus(error), errorJson(error.what()));
    } catch (...) {
        sendJson(request, response, 500, errorJson("Unexpected server error."));
    }
}

template <typename T>
T requiredValue(const Json& body, const char* key) {
    if (!body.is_object() || body.find(key) == body.end())
        throw std::invalid_argument(std::string("Missing required field: ") + key);
    const Json& value = body.at(key);
    if (std::is_same<T, int>::value) {
        if (!value.is_number_integer())
            throw std::invalid_argument(std::string("Invalid value for field: ") + key);
    } else if (std::is_same<T, std::string>::value) {
        if (!value.is_string())
            throw std::invalid_argument(std::string("Invalid value for field: ") + key);
    } else if (std::is_same<T, bool>::value) {
        if (!value.is_boolean())
            throw std::invalid_argument(std::string("Invalid value for field: ") + key);
    }
    try {
        return value.get<T>();
    } catch (const Json::exception&) {
        throw std::invalid_argument(std::string("Invalid value for field: ") + key);
    }
}

int parseId(const std::string& text) {
    if (text.empty()) throw std::invalid_argument("ID must be a positive integer.");
    long long value = 0;
    for (char character : text) {
        if (!std::isdigit(static_cast<unsigned char>(character)))
            throw std::invalid_argument("ID must be a positive integer.");
        value = value * 10 + (character - '0');
        if (value > std::numeric_limits<int>::max())
            throw std::invalid_argument("ID is outside the supported range.");
    }
    if (value <= 0) throw std::invalid_argument("ID must be a positive integer.");
    return static_cast<int>(value);
}

std::string patientType(const Patient& patient) {
    if (dynamic_cast<const EmergencyPatient*>(&patient) != nullptr) return "emergency";
    if (dynamic_cast<const RegularPatient*>(&patient) != nullptr) return "regular";
    return "patient";
}

bool isCritical(const Patient& patient) {
    const auto* emergency = dynamic_cast<const EmergencyPatient*>(&patient);
    return emergency != nullptr && emergency->isCritical();
}

Json patientJson(const Patient& patient) {
    return Json{{"id", patient.getId()}, {"name", patient.getName()},
                {"age", patient.getAge()}, {"condition", patient.getCondition()},
                {"severity", patient.getSeverity()}, {"waitingTime", patient.getWaitingTime()},
                {"status", toString(patient.getStatus())}, {"type", patientType(patient)},
                {"critical", isCritical(patient)}, {"priority", patient.calculatePriority()}};
}

Json patientList(const Hospital& hospital) {
    Json patients = Json::array();
    for (const auto& patient : hospital.getPatients()) patients.push_back(patientJson(*patient));
    return patients;
}

Json dashboardJson(const Hospital& hospital) {
    const HospitalStatistics stats = hospital.getStatistics();
    return Json{{"totalPatients", stats.totalPatients},
                {"waitingPatients", stats.waitingPatients},
                {"calledPatients", stats.calledPatients},
                {"assignedPatients", stats.assignedPatients},
                {"underTreatment", stats.patientsUnderTreatment},
                {"completedPatients", stats.completedPatients},
                {"criticalPatients", stats.criticalPatients},
                {"totalDoctors", hospital.getDoctors().size()},
                {"availableDoctors", stats.availableDoctors},
                {"busyDoctors", stats.busyDoctors},
                {"totalTreatments", stats.totalTreatments},
                {"averageWaitingTime", stats.averageWaitingTime},
                {"averagePriority", stats.averagePriority}};
}

Json doctorJson(const Doctor& doctor) {
    return Json{{"id", doctor.getDoctorId()}, {"name", doctor.getName()},
                {"specialization", doctor.getSpecialization()},
                {"availability", doctor.isAvailable() ? "available" : "busy"},
                {"currentPatientId", doctor.isAvailable() ? Json(nullptr) : Json(doctor.getCurrentPatientId())}};
}

Json doctorList(const Hospital& hospital) {
    Json doctors = Json::array();
    for (const Doctor& doctor : hospital.getDoctors()) doctors.push_back(doctorJson(doctor));
    return doctors;
}

Json treatmentJson(const Treatment& treatment) {
    return Json{{"patientId", treatment.getPatientId()}, {"doctorId", treatment.getDoctorId()},
                {"diagnosis", treatment.getDiagnosis()}, {"description", treatment.getDescription()},
                {"completed", treatment.isCompleted()},
                {"status", treatment.isCompleted() ? "completed" : "in_progress"}};
}

Json treatmentList(const Hospital& hospital) {
    Json treatments = Json::array();
    for (const Treatment& treatment : hospital.getTreatments())
        treatments.push_back(treatmentJson(treatment));
    return treatments;
}
}

WebServer::WebServer(Hospital& hospital, const std::string& webDirectory,
                     const std::string& userFile)
    : hospital_(hospital), userManager_(userFile) {
    const bool frontendMounted = server_.set_base_dir(webDirectory.c_str());
    server_.set_file_request_handler(setCorsHeaders);
    registerRoutes();
    if (!frontendMounted) {
        std::cerr << "Frontend directory not available at " << webDirectory
                  << "; API will run without static files.\n";
        server_.Get("/", [](const httplib::Request& request, httplib::Response& response) {
            sendJson(request, response, 503, errorJson("Frontend files are unavailable; place the UI in web/."));
        });
    }
}

bool WebServer::listen(const std::string& host, int port) {
    return server_.listen(host.c_str(), port);
}

void WebServer::stop() { server_.stop(); }

void WebServer::registerRoutes() {
    server_.Options(R"(/api/.*)", [](const httplib::Request& request, httplib::Response& response) {
        setCorsHeaders(request, response);
        response.status = 204;
    });

    server_.Post("/api/auth/register", [this](const httplib::Request& request, httplib::Response& response) {
        guarded(request, response, [&] {
            const Json body = Json::parse(request.body);
            const AuthUser user = userManager_.registerUser(
                requiredValue<std::string>(body, "fullName"),
                requiredValue<std::string>(body, "username"),
                requiredValue<std::string>(body, "password"));
            sendJson(request, response, 201, Json{{"success", true}, {"user", authUserJson(user)}});
        });
    });

    server_.Post("/api/auth/login", [this](const httplib::Request& request, httplib::Response& response) {
        guarded(request, response, [&] {
            const Json body = Json::parse(request.body);
            const AuthSession session = userManager_.login(
                requiredValue<std::string>(body, "username"),
                requiredValue<std::string>(body, "password"));
            sendJson(request, response, 200, Json{{"success", true}, {"token", session.token},
                                         {"user", authUserJson(session.user)}});
        });
    });

    server_.Post("/api/auth/logout", [this](const httplib::Request& request, httplib::Response& response) {
        userManager_.logout(sessionToken(request));
        sendJson(request, response, 200, Json{{"success", true}});
    });

    server_.Get("/api/auth/me", [this](const httplib::Request& request, httplib::Response& response) {
        AuthUser user;
        if (!userManager_.findSession(sessionToken(request), user))
            sendJson(request, response, 401, Json{{"error", "Please sign in to continue."}});
        else sendJson(request, response, 200, authUserJson(user));
    });

    server_.Get("/api/dashboard", [this](const httplib::Request& request, httplib::Response& response) {
        if (!requireAuth(userManager_, request, response)) return;
        guarded(request, response, [&] { sendJson(request, response, 200, dashboardJson(hospital_)); });
    });

    server_.Get("/api/patients", [this](const httplib::Request& request, httplib::Response& response) {
        if (!requireAuth(userManager_, request, response)) return;
        guarded(request, response, [&] { sendJson(request, response, 200, patientList(hospital_)); });
    });

    server_.Get(R"(/api/patients/([0-9]+))",
        [this](const httplib::Request& request, httplib::Response& response) {
            if (!requireAuth(userManager_, request, response)) return;
            guarded(request, response, [&] {
                const int id = parseId(request.matches[1]);
                const Patient* patient = hospital_.findPatient(id);
                if (patient == nullptr) sendJson(request, response, 404, errorJson("Patient not found"));
                else sendJson(request, response, 200, patientJson(*patient));
            });
        });

    server_.Post("/api/patients", [this](const httplib::Request& request, httplib::Response& response) {
        if (!requireAuth(userManager_, request, response)) return;
        guarded(request, response, [&] {
            const Json body = Json::parse(request.body);
            const int id = requiredValue<int>(body, "id");
            const std::string name = requiredValue<std::string>(body, "name");
            const int age = requiredValue<int>(body, "age");
            const std::string condition = requiredValue<std::string>(body, "condition");
            const int severity = requiredValue<int>(body, "severity");
            const std::string type = requiredValue<std::string>(body, "type");
            const int waiting = body.contains("waitingTime") ? requiredValue<int>(body, "waitingTime") : 0;
            Patient* patient = nullptr;
            if (type == "emergency") {
                const bool critical = body.contains("critical")
                    ? requiredValue<bool>(body, "critical") : false;
                patient = &hospital_.registerEmergencyPatient(id, name, age, condition, severity,
                                                               critical, waiting);
            } else if (type == "regular") {
                patient = &hospital_.registerRegularPatient(id, name, age, condition, severity, waiting);
            } else {
                throw std::invalid_argument("Patient type must be 'emergency' or 'regular'.");
            }
            sendJson(request, response, 201, patientJson(*patient));
        });
    });

    server_.Get("/api/queue", [this](const httplib::Request& request, httplib::Response& response) {
        if (!requireAuth(userManager_, request, response)) return;
        guarded(request, response, [&] {
            Json queue = Json::array();
            for (const Patient* patient : hospital_.getWaitingPatients()) {
                queue.push_back(Json{{"id", patient->getId()}, {"name", patient->getName()},
                    {"severity", patient->getSeverity()}, {"priority", patient->calculatePriority()},
                    {"waitingTime", patient->getWaitingTime()}, {"status", toString(patient->getStatus())},
                    {"critical", isCritical(*patient)}});
            }
            sendJson(request, response, 200, queue);
        });
    });

    server_.Post("/api/queue/call-next", [this](const httplib::Request& request, httplib::Response& response) {
        if (!requireAuth(userManager_, request, response)) return;
        guarded(request, response, [&] {
            Patient* patient = hospital_.callNextPatient();
            if (patient == nullptr) sendJson(request, response, 409, errorJson("Waiting queue is empty"));
            else sendJson(request, response, 200, patientJson(*patient));
        });
    });

    server_.Get("/api/doctors", [this](const httplib::Request& request, httplib::Response& response) {
        if (!requireAuth(userManager_, request, response)) return;
        guarded(request, response, [&] { sendJson(request, response, 200, doctorList(hospital_)); });
    });

    server_.Post("/api/doctors", [this](const httplib::Request& request, httplib::Response& response) {
        if (!requireAuth(userManager_, request, response)) return;
        guarded(request, response, [&] {
            const Json body = Json::parse(request.body);
            const int id = requiredValue<int>(body, "id");
            const std::string name = requiredValue<std::string>(body, "name");
            const std::string specialization = requiredValue<std::string>(body, "specialization");
            hospital_.addDoctor(Doctor(id, name, specialization));
            sendJson(request, response, 201, doctorJson(*hospital_.findDoctor(id)));
        });
    });

    server_.Post(R"(/api/patients/([0-9]+)/assign-doctor)",
        [this](const httplib::Request& request, httplib::Response& response) {
            if (!requireAuth(userManager_, request, response)) return;
            guarded(request, response, [&] {
                const int patientId = parseId(request.matches[1]);
                const Json body = Json::parse(request.body);
                const int doctorId = requiredValue<int>(body, "doctorId");
                hospital_.assignDoctor(patientId, doctorId);
                const Patient* patient = hospital_.findPatient(patientId);
                sendJson(request, response, 200, patientJson(*patient));
            });
        });

    server_.Post(R"(/api/patients/([0-9]+)/start-treatment)",
        [this](const httplib::Request& request, httplib::Response& response) {
            if (!requireAuth(userManager_, request, response)) return;
            guarded(request, response, [&] {
                const int patientId = parseId(request.matches[1]);
                hospital_.startTreatment(patientId);
                sendJson(request, response, 200, Json{{"success", true}, {"message", "Treatment is in progress"}});
            });
        });

    server_.Post(R"(/api/patients/([0-9]+)/complete-treatment)",
        [this](const httplib::Request& request, httplib::Response& response) {
            if (!requireAuth(userManager_, request, response)) return;
            guarded(request, response, [&] {
                const int patientId = parseId(request.matches[1]);
                const Json body = Json::parse(request.body);
                const int doctorId = requiredValue<int>(body, "doctorId");
                const std::string diagnosis = requiredValue<std::string>(body, "diagnosis");
                const std::string description = requiredValue<std::string>(body, "description");
                hospital_.completeTreatment(patientId, doctorId, diagnosis, description);
                sendJson(request, response, 200, patientJson(*hospital_.findPatient(patientId)));
            });
        });

    server_.Get("/api/treatments", [this](const httplib::Request& request, httplib::Response& response) {
        if (!requireAuth(userManager_, request, response)) return;
        guarded(request, response, [&] { sendJson(request, response, 200, treatmentList(hospital_)); });
    });

    server_.Post("/api/data/save", [this](const httplib::Request& request, httplib::Response& response) {
        if (!requireAuth(userManager_, request, response)) return;
        guarded(request, response, [&] {
            hospital_.saveData();
            sendJson(request, response, 200, Json{{"success", true}, {"message", "Data saved successfully"}});
        });
    });

    server_.Post("/api/data/load", [this](const httplib::Request& request, httplib::Response& response) {
        if (!requireAuth(userManager_, request, response)) return;
        guarded(request, response, [&] {
            hospital_.loadData();
            sendJson(request, response, 200, Json{{"success", true}, {"message", "Data loaded successfully"},
                                         {"dashboard", dashboardJson(hospital_)},
                                         {"patients", patientList(hospital_)}});
        });
    });

}
