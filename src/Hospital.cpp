#include "Hospital.h"
#include "FileManager.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <utility>

Patient& Hospital::registerPatient(std::unique_ptr<Patient> patient) {
    if (!patient) throw std::invalid_argument("Cannot register a null patient.");
    if (findPatient(patient->getId()) != nullptr)
        throw std::invalid_argument("Patient ID is already registered.");
    if (patient->getStatus() != PatientStatus::WAITING)
        throw std::invalid_argument("A newly registered patient must be WAITING.");
    Patient* rawPatient = patient.get();
    patients_.push_back(std::move(patient));
    try {
        patientQueue_.addPatient(rawPatient);
    } catch (...) {
        patients_.pop_back();
        throw;
    }
    return *rawPatient;
}

Patient& Hospital::registerEmergencyPatient(int id, const std::string& name, int age,
                                            const std::string& condition, int severity,
                                            bool critical, int waitingMinutes) {
    return registerPatient(std::make_unique<EmergencyPatient>(
        id, name, age, condition, severity, critical, waitingMinutes));
}

Patient& Hospital::registerRegularPatient(int id, const std::string& name, int age,
                                          const std::string& condition, int severity,
                                          int waitingMinutes) {
    return registerPatient(std::make_unique<RegularPatient>(
        id, name, age, condition, severity, waitingMinutes));
}

void Hospital::addDoctor(const Doctor& doctor) { doctorManager_.addDoctor(doctor); }
void Hospital::displayWaitingQueue() const { patientQueue_.displayQueue(); }

void Hospital::displayAllPatients() const {
    if (patients_.empty()) {
        std::cout << "No registered patients.\n";
        return;
    }
    for (const auto& patient : patients_) patient->display();
}

Patient* Hospital::findPatient(int patientId) { return patientQueue_.findPatient(patientId); }
const Patient* Hospital::findPatient(int patientId) const { return patientQueue_.findPatient(patientId); }
void Hospital::displayDoctors() const { doctorManager_.displayAllDoctors(); }

Patient* Hospital::callNextPatient(std::chrono::system_clock::time_point now) {
    Patient* patient = patientQueue_.getNextPatient();
    if (patient == nullptr) return nullptr;
    if (patient->getStatus() != PatientStatus::WAITING)
        throw std::logic_error("Only a WAITING patient can be called.");
    patient->setStatusAt(PatientStatus::CALLED, now);
    return patient;
}

void Hospital::assignDoctor(int patientId, int doctorId) {
    Doctor* doctor = doctorManager_.findDoctorById(doctorId);
    if (doctor == nullptr) throw std::invalid_argument("Doctor ID was not found.");
    if (!doctor->isAvailable()) throw std::logic_error("Doctor is not available.");
    Patient* patient = findPatient(patientId);
    if (patient == nullptr) throw std::invalid_argument("Patient ID was not found.");
    if (patient->getStatus() != PatientStatus::CALLED)
        throw std::logic_error("A doctor can only be assigned to a CALLED patient.");
    doctor->assignPatient(patientId);
    patient->setStatus(PatientStatus::ASSIGNED);
}

void Hospital::startTreatment(int patientId) {
    Patient* patient = findPatient(patientId);
    if (patient == nullptr) throw std::invalid_argument("Patient ID was not found.");
    if (patient->getStatus() != PatientStatus::ASSIGNED)
        throw std::logic_error("Treatment can start only after a doctor is assigned.");
    const Doctor* assignedDoctor = nullptr;
    for (const Doctor& doctor : doctorManager_.getDoctors())
        if (!doctor.isAvailable() && doctor.getCurrentPatientId() == patientId) {
            assignedDoctor = &doctor;
            break;
        }
    if (assignedDoctor == nullptr) throw std::logic_error("Assigned doctor was not found.");
    patient->setStatus(PatientStatus::UNDER_TREATMENT);
}

void Hospital::completeTreatment(int patientId, int doctorId,
                                 const std::string& diagnosis,
                                 const std::string& description) {
    Doctor* doctor = doctorManager_.findDoctorById(doctorId);
    if (doctor == nullptr) throw std::invalid_argument("Doctor ID was not found.");
    Patient* patient = findPatient(patientId);
    if (patient == nullptr) throw std::invalid_argument("Patient ID was not found.");
    if (patient->getStatus() != PatientStatus::UNDER_TREATMENT)
        throw std::logic_error("Only an UNDER_TREATMENT patient can be completed.");
    if (doctor->isAvailable() || doctor->getCurrentPatientId() != patientId)
        throw std::logic_error("The specified doctor is not assigned to this patient.");

    Treatment treatment(patientId, doctorId, diagnosis, description, true);
    treatmentManager_.addTreatment(treatment);
    doctor->completeTreatment();
    patient->setStatus(PatientStatus::COMPLETED);
}

const Treatment* Hospital::findTreatment(int patientId) const {
    return treatmentManager_.findTreatment(patientId);
}

void Hospital::displayTreatmentHistory() const { treatmentManager_.displayTreatmentHistory(); }

HospitalStatistics Hospital::getStatistics() const {
    return statisticsManager_.calculate(patients_, doctorManager_, treatmentManager_);
}

Doctor* Hospital::findDoctor(int doctorId) { return doctorManager_.findDoctorById(doctorId); }

const std::vector<std::unique_ptr<Patient>>& Hospital::getPatients() const { return patients_; }
std::vector<Patient*> Hospital::getWaitingPatients() const { return patientQueue_.getWaitingPatients(); }
const std::vector<Doctor>& Hospital::getDoctors() const { return doctorManager_.getDoctors(); }
const std::vector<Treatment>& Hospital::getTreatments() const { return treatmentManager_.getTreatments(); }

void Hospital::restorePatient(std::unique_ptr<Patient> patient, PatientStatus status) {
    if (!patient) throw std::invalid_argument("Cannot restore a null patient.");
    if (findPatient(patient->getId()) != nullptr)
        throw std::invalid_argument("Patient ID is already registered.");
    if (status == PatientStatus::CALLED || status == PatientStatus::ASSIGNED || status == PatientStatus::UNDER_TREATMENT ||
        status == PatientStatus::COMPLETED) patient->setStatus(PatientStatus::CALLED);
    if (status == PatientStatus::ASSIGNED || status == PatientStatus::UNDER_TREATMENT || status == PatientStatus::COMPLETED)
        patient->setStatus(PatientStatus::ASSIGNED);
    if (status == PatientStatus::UNDER_TREATMENT || status == PatientStatus::COMPLETED)
        patient->setStatus(PatientStatus::UNDER_TREATMENT);
    if (status == PatientStatus::COMPLETED) patient->setStatus(PatientStatus::COMPLETED);

    Patient* rawPatient = patient.get();
    patients_.push_back(std::move(patient));
    try {
        patientQueue_.trackPatient(rawPatient);
    } catch (...) {
        patients_.pop_back();
        throw;
    }
}

void Hospital::restoreTreatment(const Treatment& treatment) {
    const Patient* patient = findPatient(treatment.getPatientId());
    const Doctor* doctor = doctorManager_.findDoctorById(treatment.getDoctorId());
    if (patient == nullptr || doctor == nullptr)
        throw std::invalid_argument("Treatment references an unknown patient or doctor.");
    if (treatment.isCompleted() && patient->getStatus() != PatientStatus::COMPLETED)
        throw std::invalid_argument("Completed treatment requires a completed patient.");
    if (!treatment.isCompleted() &&
        (patient->getStatus() != PatientStatus::UNDER_TREATMENT || doctor->isAvailable() ||
         doctor->getCurrentPatientId() != patient->getId()))
        throw std::invalid_argument("In-progress treatment must match the doctor's assignment.");
    treatmentManager_.addTreatment(treatment);
}

void Hospital::rebuildWaitingQueue() { patientQueue_.rebuildWaitingQueue(); }

void Hospital::discardPatient(int patientId) {
    const Patient* patient = findPatient(patientId);
    if (patient == nullptr) return;
    if (patient->getStatus() == PatientStatus::WAITING)
        throw std::logic_error("Cannot discard a patient still in the waiting queue.");
    patientQueue_.clear();
    for (const auto& ownedPatient : patients_)
        if (ownedPatient->getId() != patientId) patientQueue_.trackPatient(ownedPatient.get());
    patients_.erase(std::remove_if(patients_.begin(), patients_.end(),
        [patientId](const std::unique_ptr<Patient>& item) { return item->getId() == patientId; }),
        patients_.end());
    patientQueue_.rebuildWaitingQueue();
}

void Hospital::replaceWith(Hospital&& other) {
    patientQueue_.clear();
    patients_.clear();
    doctorManager_.clear();
    treatmentManager_.clear();
    patients_ = std::move(other.patients_);
    patientQueue_ = std::move(other.patientQueue_);
    doctorManager_ = std::move(other.doctorManager_);
    treatmentManager_ = std::move(other.treatmentManager_);
}

void Hospital::saveData(const std::string& directory) const {
    FileManager(directory).saveAll(*this);
}

void Hospital::loadData(const std::string& directory) {
    FileManager(directory).loadAll(*this);
}
