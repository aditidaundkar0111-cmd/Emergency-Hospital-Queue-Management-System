#include "Patient.h"
#include "Exceptions.h"
#include "PriorityCalculator.h"
#include <iostream>

const char* toString(PatientStatus status) {
    switch (status) {
        case PatientStatus::WAITING: return "WAITING";
        case PatientStatus::CALLED: return "CALLED";
        case PatientStatus::UNDER_TREATMENT: return "UNDER_TREATMENT";
        case PatientStatus::COMPLETED: return "COMPLETED";
    }
    return "UNKNOWN";
}

Patient::Patient(int id, const std::string& name, int age,
                 const std::string& condition, int severity, int waitingMinutes)
    : Person(id, name), age_(age), condition_(condition), severity_(severity),
      waitingMinutes_(waitingMinutes), status_(PatientStatus::WAITING) {
    if (age < 0 || age > 130) throw InvalidPatientDataException("Age must be between 0 and 130.");
    if (isBlankText(condition)) throw InvalidPatientDataException("Condition cannot be empty.");
    if (severity < 1 || severity > 5) throw InvalidPatientDataException("Severity must be between 1 and 5.");
    if (waitingMinutes < 0) throw InvalidPatientDataException("Waiting time cannot be negative.");
}

int Patient::getAge() const { return age_; }
const std::string& Patient::getCondition() const { return condition_; }
int Patient::getSeverity() const { return severity_; }
int Patient::getWaitingTime() const { return waitingMinutes_; }
PatientStatus Patient::getStatus() const { return status_; }

void Patient::setStatus(PatientStatus status) { status_ = status; }

double Patient::calculatePriority() const {
    return PriorityCalculator::calculate(severity_, false, waitingMinutes_, age_);
}

void Patient::display() const {
    Person::display();
    std::cout << " | Age: " << age_ << " | Condition: " << condition_
              << " | Severity: " << severity_ << " | Wait: " << waitingMinutes_
              << " min | Status: " << toString(status_)
              << " | Priority: " << calculatePriority() << '\n';
}
