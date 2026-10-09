#include "Treatment.h"
#include "Person.h"
#include <iostream>
#include <stdexcept>

Treatment::Treatment(int patientId, int doctorId, const std::string& diagnosis,
                     const std::string& description, bool completed)
    : patientId_(patientId), doctorId_(doctorId), diagnosis_(diagnosis),
      description_(description), completed_(completed) {
    if (patientId <= 0 || doctorId <= 0)
        throw std::invalid_argument("Treatment patient and doctor IDs must be positive.");
    if (isBlankText(diagnosis) || isBlankText(description))
        throw std::invalid_argument("Diagnosis and treatment description cannot be empty.");
}

int Treatment::getPatientId() const { return patientId_; }
int Treatment::getDoctorId() const { return doctorId_; }
const std::string& Treatment::getDiagnosis() const { return diagnosis_; }
const std::string& Treatment::getDescription() const { return description_; }
bool Treatment::isCompleted() const { return completed_; }

void Treatment::display() const {
    std::cout << "Patient " << patientId_ << " | Doctor " << doctorId_
              << " | Diagnosis: " << diagnosis_ << " | Treatment: " << description_
              << " | " << (completed_ ? "Completed" : "In progress") << '\n';
}
