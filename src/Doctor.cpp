#include "Doctor.h"
#include "Person.h"
#include <iostream>
#include <stdexcept>

Doctor::Doctor(int doctorId, const std::string& name, const std::string& specialization,
               bool available, int currentPatientId)
    : doctorId_(doctorId), name_(name), specialization_(specialization),
      available_(available), currentPatientId_(currentPatientId) {
    if (doctorId <= 0) throw std::invalid_argument("Doctor ID must be positive.");
    if (isBlankText(name)) throw std::invalid_argument("Doctor name cannot be empty.");
    if (isBlankText(specialization)) throw std::invalid_argument("Specialization cannot be empty.");
    if ((available && currentPatientId != -1) || (!available && currentPatientId <= 0))
        throw std::invalid_argument("Doctor availability and current patient ID are inconsistent.");
}

int Doctor::getDoctorId() const { return doctorId_; }
const std::string& Doctor::getName() const { return name_; }
const std::string& Doctor::getSpecialization() const { return specialization_; }
int Doctor::getCurrentPatientId() const { return currentPatientId_; }
bool Doctor::isAvailable() const { return available_; }

void Doctor::assignPatient(int patientId) {
    if (patientId <= 0) throw std::invalid_argument("Patient ID must be positive.");
    if (!available_) throw std::logic_error("Doctor is already assigned to a patient.");
    currentPatientId_ = patientId;
    available_ = false;
}

void Doctor::completeTreatment() {
    if (available_) throw std::logic_error("Doctor has no active patient.");
    currentPatientId_ = -1;
    available_ = true;
}

void Doctor::display() const {
    std::cout << "Doctor " << doctorId_ << " | " << name_ << " | " << specialization_
              << " | " << (available_ ? "Available" : "Busy")
              << (available_ ? "" : " | Patient " + std::to_string(currentPatientId_)) << '\n';
}
