#include "FileManager.h"
#include "DoctorManager.h"
#include "EmergencyPatient.h"
#include "Hospital.h"
#include "RegularPatient.h"
#include <cerrno>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <sys/stat.h>
#if defined(_WIN32)
#include <direct.h>
#endif

namespace {
std::string statusToText(PatientStatus status) {
    switch (status) {
        case PatientStatus::WAITING: return "WAITING";
        case PatientStatus::CALLED: return "CALLED";
        case PatientStatus::ASSIGNED: return "ASSIGNED";
        case PatientStatus::UNDER_TREATMENT: return "UNDER_TREATMENT";
        case PatientStatus::COMPLETED: return "COMPLETED";
    }
    throw std::invalid_argument("Unknown patient status.");
}

PatientStatus parseStatus(const std::string& value) {
    if (value == "WAITING") return PatientStatus::WAITING;
    if (value == "CALLED") return PatientStatus::CALLED;
    if (value == "ASSIGNED") return PatientStatus::ASSIGNED;
    if (value == "UNDER_TREATMENT") return PatientStatus::UNDER_TREATMENT;
    if (value == "COMPLETED") return PatientStatus::COMPLETED;
    throw std::invalid_argument("Invalid patient status: " + value);
}

bool isIgnorableLine(const std::string& line) {
    const std::size_t first = line.find_first_not_of(" \t\r");
    return first == std::string::npos || line[first] == '#';
}

bool pathExists(const std::string& path) {
#if defined(_WIN32)
    struct _stat info;
    return _stat(path.c_str(), &info) == 0 && (info.st_mode & _S_IFREG) != 0;
#else
    struct stat info;
    return stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode);
#endif
}

void requireEndOfRecord(std::istringstream& record) {
    std::string extra;
    if (record >> extra) throw std::invalid_argument("Unexpected extra field.");
}

void reportBadRecord(const std::string& path, std::size_t lineNumber,
                     const std::string& reason) {
    throw std::runtime_error("Invalid record in " + path + ':' + std::to_string(lineNumber) +
                             " (" + reason + "). Existing application data was preserved.");
}
}

FileManager::FileManager(const std::string& directory) : directory_(directory) {
    if (directory_.empty()) throw std::invalid_argument("Data directory cannot be empty.");
}

std::string FileManager::filePath(const std::string& fileName) const {
    const char last = directory_[directory_.size() - 1];
    return directory_ + ((last == '/' || last == '\\') ? "" : "/") + fileName;
}

void FileManager::ensureDirectory() const {
    const auto createOne = [](const std::string& path) {
#if defined(_WIN32)
        const int result = _mkdir(path.c_str());
#else
        const int result = mkdir(path.c_str(), 0755);
#endif
        if (result != 0 && errno != EEXIST)
            throw std::runtime_error("Could not create data directory: " + path);
    };

    for (std::size_t index = 0; index < directory_.size(); ++index) {
        if (directory_[index] != '/' && directory_[index] != '\\') continue;
        if (index == 0 || (index == 2 && directory_.size() > 2 && directory_[1] == ':')) continue;
        createOne(directory_.substr(0, index));
    }
    createOne(directory_);
}

void FileManager::savePatients(const Hospital& hospital) const {
    ensureDirectory();
    const std::string path = filePath("patients.txt");
    std::ofstream output(path.c_str(), std::ios::trunc);
    if (!output) throw std::runtime_error("Could not open patient data file for writing: " + path);
    output << "# P id type age severity wait critical status \"name\" \"condition\" [waitingStartedEpochSeconds]\n";
    for (const auto& patient : hospital.patients_) {
        std::string type = "BASE";
        int critical = 0;
        if (const auto* emergency = dynamic_cast<const EmergencyPatient*>(patient.get())) {
            type = "EMERGENCY";
            critical = emergency->isCritical() ? 1 : 0;
        } else if (dynamic_cast<const RegularPatient*>(patient.get()) != nullptr) {
            type = "REGULAR";
        }
        const long long waitingStartedEpochSeconds = patient->getStatus() == PatientStatus::WAITING
            ? std::chrono::duration_cast<std::chrono::seconds>(
                  patient->waitingStartedAt_.time_since_epoch()).count()
            : -1;
        output << "P " << patient->getId() << ' ' << type << ' ' << patient->getAge() << ' '
               << patient->getSeverity() << ' ' << patient->waitingMinutes_ << ' '
               << critical << ' ' << statusToText(patient->getStatus()) << ' '
               << std::quoted(patient->getName()) << ' '
               << std::quoted(patient->getCondition()) << ' '
               << waitingStartedEpochSeconds << '\n';
    }
    if (!output) throw std::runtime_error("Failed while writing patient data: " + path);
}

void FileManager::loadPatients(Hospital& hospital) const {
    bool legacyWaitingTimestampFound = false;
    loadPatients(hospital, legacyWaitingTimestampFound);
}

void FileManager::loadPatients(Hospital& hospital, bool& legacyWaitingTimestampFound) const {
    const std::string path = filePath("patients.txt");
    std::ifstream input(path.c_str());
    if (!input) {
        std::cerr << "Patient data file not found; loading no patient records: " << path << '\n';
        return;
    }
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        if (isIgnorableLine(line)) continue;
        try {
            std::istringstream record(line);
            char tag = '\0';
            int id = 0, age = 0, severity = 0, waiting = 0, critical = 0;
            long long waitingStartedEpochSeconds = -1;
            std::string type, statusText, name, condition;
            if (!(record >> tag >> id >> type >> age >> severity >> waiting >> critical
                         >> statusText >> std::quoted(name) >> std::quoted(condition)) || tag != 'P')
                throw std::invalid_argument("Invalid patient record format.");
            record >> std::ws;
            const bool hasWaitingTimestamp = record.peek() != std::char_traits<char>::eof();
            if (hasWaitingTimestamp && !(record >> waitingStartedEpochSeconds))
                throw std::invalid_argument("Invalid waiting start timestamp.");
            requireEndOfRecord(record);
            if (waitingStartedEpochSeconds < -1)
                throw std::invalid_argument("Invalid waiting start timestamp.");
            if (waitingStartedEpochSeconds >= 0) {
                const long long latestAllowed = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count() + 86400;
                if (waitingStartedEpochSeconds > latestAllowed)
                    throw std::invalid_argument("Waiting start timestamp is in the future.");
            }
            if ((critical != 0 && critical != 1) || (type != "BASE" && type != "REGULAR" && type != "EMERGENCY"))
                throw std::invalid_argument("Invalid patient type or critical flag.");
            if (type != "EMERGENCY" && critical != 0)
                throw std::invalid_argument("Critical flag is only valid for emergency patients.");
            const PatientStatus status = parseStatus(statusText);
            std::unique_ptr<Patient> patient;
            if (type == "EMERGENCY")
                patient = std::make_unique<EmergencyPatient>(id, name, age, condition, severity,
                                                             critical == 1, waiting);
            else if (type == "REGULAR")
                patient = std::make_unique<RegularPatient>(id, name, age, condition, severity, waiting);
            else
                patient = std::make_unique<Patient>(id, name, age, condition, severity, waiting);
            hospital.restorePatient(std::move(patient), status);
            if (status == PatientStatus::WAITING && waitingStartedEpochSeconds >= 0)
                hospital.findPatient(id)->setWaitingStartedAtEpochSeconds(waitingStartedEpochSeconds);
            else if (status == PatientStatus::WAITING && waitingStartedEpochSeconds < 0)
                legacyWaitingTimestampFound = true;
        } catch (const std::exception& error) {
            reportBadRecord(path, lineNumber, error.what());
        }
    }
    hospital.rebuildWaitingQueue();
}

void FileManager::saveDoctors(const Hospital& hospital) const {
    ensureDirectory();
    const std::string path = filePath("doctors.txt");
    std::ofstream output(path.c_str(), std::ios::trunc);
    if (!output) throw std::runtime_error("Could not open doctor data file for writing: " + path);
    output << "# D id available currentPatientId \"name\" \"specialization\"\n";
    for (const Doctor& doctor : hospital.doctorManager_.doctors_)
        output << "D " << doctor.getDoctorId() << ' ' << (doctor.isAvailable() ? 1 : 0) << ' '
               << doctor.getCurrentPatientId() << ' ' << std::quoted(doctor.getName()) << ' '
               << std::quoted(doctor.getSpecialization()) << '\n';
    if (!output) throw std::runtime_error("Failed while writing doctor data: " + path);
}

void FileManager::loadDoctors(Hospital& hospital) const {
    const std::string path = filePath("doctors.txt");
    std::ifstream input(path.c_str());
    if (!input) {
        std::cerr << "Doctor data file not found; loading no doctor records: " << path << '\n';
        return;
    }
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        if (isIgnorableLine(line)) continue;
        try {
            std::istringstream record(line);
            char tag = '\0';
            int id = 0, available = 0, currentPatientId = -1;
            std::string name, specialization;
            if (!(record >> tag >> id >> available >> currentPatientId
                         >> std::quoted(name) >> std::quoted(specialization)) || tag != 'D')
                throw std::invalid_argument("Invalid doctor record format.");
            requireEndOfRecord(record);
            if (available != 0 && available != 1)
                throw std::invalid_argument("Invalid doctor availability value.");
            if (available == 0) {
                const Patient* patient = hospital.findPatient(currentPatientId);
                if (patient == nullptr || (patient->getStatus() != PatientStatus::ASSIGNED &&
                                           patient->getStatus() != PatientStatus::UNDER_TREATMENT))
                    throw std::invalid_argument("Busy doctor references no assigned patient.");
                for (const Doctor& existing : hospital.doctorManager_.doctors_)
                    if (!existing.isAvailable() && existing.getCurrentPatientId() == currentPatientId)
                        throw std::invalid_argument("Patient is assigned to more than one doctor.");
            } else if (currentPatientId != -1) {
                throw std::invalid_argument("Available doctor must not have a current patient.");
            }
            hospital.addDoctor(Doctor(id, name, specialization, available == 1, currentPatientId));
        } catch (const std::exception& error) {
            reportBadRecord(path, lineNumber, error.what());
        }
    }
}

void FileManager::saveTreatments(const Hospital& hospital) const {
    ensureDirectory();
    const std::string path = filePath("treatments.txt");
    std::ofstream output(path.c_str(), std::ios::trunc);
    if (!output) throw std::runtime_error("Could not open treatment data file for writing: " + path);
    output << "# T patientId doctorId completed \"diagnosis\" \"description\"\n";
    for (const Treatment& treatment : hospital.treatmentManager_.treatments_)
        output << "T " << treatment.getPatientId() << ' ' << treatment.getDoctorId() << ' '
               << (treatment.isCompleted() ? 1 : 0) << ' ' << std::quoted(treatment.getDiagnosis())
               << ' ' << std::quoted(treatment.getDescription()) << '\n';
    if (!output) throw std::runtime_error("Failed while writing treatment data: " + path);
}

void FileManager::loadTreatments(Hospital& hospital) const {
    const std::string path = filePath("treatments.txt");
    std::ifstream input(path.c_str());
    if (!input) {
        std::cerr << "Treatment data file not found; loading no treatment records: " << path << '\n';
        return;
    }
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        if (isIgnorableLine(line)) continue;
        try {
            std::istringstream record(line);
            char tag = '\0';
            int patientId = 0, doctorId = 0, completed = 0;
            std::string diagnosis, description;
            if (!(record >> tag >> patientId >> doctorId >> completed
                         >> std::quoted(diagnosis) >> std::quoted(description)) || tag != 'T')
                throw std::invalid_argument("Invalid treatment record format.");
            requireEndOfRecord(record);
            if (completed != 0 && completed != 1)
                throw std::invalid_argument("Invalid treatment completion value.");
            if (hospital.findTreatment(patientId) != nullptr)
                throw std::invalid_argument("Duplicate treatment record for patient.");
            hospital.restoreTreatment(Treatment(patientId, doctorId, diagnosis, description,
                                                 completed == 1));
        } catch (const std::exception& error) {
            reportBadRecord(path, lineNumber, error.what());
        }
    }
}

void FileManager::saveAll(const Hospital& hospital) const {
    ensureDirectory();
    savePatients(hospital);
    saveDoctors(hospital);
    saveTreatments(hospital);
}

void FileManager::loadAll(Hospital& hospital) const {
    const std::string paths[] = {filePath("patients.txt"), filePath("doctors.txt"), filePath("treatments.txt")};
    int existingFiles = 0;
    for (const std::string& path : paths) if (pathExists(path)) ++existingFiles;
    if (existingFiles != 0 && existingFiles != 3)
        throw std::runtime_error("Hospital data set is incomplete; all three data files are required. Existing application data was preserved.");
    if (existingFiles == 0 && (hospital.getStatistics().totalPatients != 0 ||
        !hospital.getDoctors().empty() || !hospital.getTreatments().empty()))
        throw std::runtime_error("No saved hospital data files were found. Existing application data was preserved.");
    Hospital loaded;
    bool legacyWaitingTimestampFound = false;
    loadPatients(loaded, legacyWaitingTimestampFound);
    loadDoctors(loaded);

    for (const auto& patient : loaded.patients_) {
        if (patient->getStatus() != PatientStatus::UNDER_TREATMENT &&
            patient->getStatus() != PatientStatus::ASSIGNED) continue;
        bool hasAssignedDoctor = false;
        for (const Doctor& doctor : loaded.doctorManager_.doctors_)
            if (!doctor.isAvailable() && doctor.getCurrentPatientId() == patient->getId())
                hasAssignedDoctor = true;
        if (!hasAssignedDoctor) {
            throw std::runtime_error("Patient " + std::to_string(patient->getId()) +
                                     " has no valid busy doctor assignment. Existing application data was preserved.");
        }
    }

    loadTreatments(loaded);
    // Persist the one-time fallback clock only after every file and relationship validates.
    if (legacyWaitingTimestampFound) savePatients(loaded);
    hospital.replaceWith(std::move(loaded));
}
