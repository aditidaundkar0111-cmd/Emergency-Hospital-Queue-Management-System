#include "Patient.h"
#include "Exceptions.h"
#include "PriorityCalculator.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>

const char* toString(PatientStatus status) {
    switch (status) {
        case PatientStatus::WAITING: return "WAITING";
        case PatientStatus::CALLED: return "CALLED";
        case PatientStatus::ASSIGNED: return "ASSIGNED";
        case PatientStatus::UNDER_TREATMENT: return "UNDER_TREATMENT";
        case PatientStatus::COMPLETED: return "COMPLETED";
    }
    return "UNKNOWN";
}

Patient::Patient(int id, const std::string& name, int age,
                 const std::string& condition, int severity, int waitingMinutes)
    : Patient(id, name, age, condition, severity, waitingMinutes,
              std::chrono::system_clock::now()) {}

Patient::Patient(int id, const std::string& name, int age,
                 const std::string& condition, int severity, int waitingMinutes,
                 std::chrono::system_clock::time_point waitingStartedAt)
    : Person(id, name), age_(age), condition_(condition), severity_(severity),
      waitingMinutes_(waitingMinutes), waitingStartedAt_(waitingStartedAt),
      status_(PatientStatus::WAITING) {
    if (age < 0 || age > 130) throw InvalidPatientDataException("Age must be between 0 and 130.");
    if (isBlankText(condition)) throw InvalidPatientDataException("Condition cannot be empty.");
    if (severity < 1 || severity > 5) throw InvalidPatientDataException("Severity must be between 1 and 5.");
    if (waitingMinutes < 0) throw InvalidPatientDataException("Waiting time cannot be negative.");
}

int Patient::getAge() const { return age_; }
const std::string& Patient::getCondition() const { return condition_; }
int Patient::getSeverity() const { return severity_; }
int Patient::getWaitingTime() const { return getWaitingTimeAt(std::chrono::system_clock::now()); }
int Patient::getWaitingTimeAt(std::chrono::system_clock::time_point now) const {
    long long minutes = waitingMinutes_;
    if (status_ == PatientStatus::WAITING) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - waitingStartedAt_).count();
        if (elapsed > 0) minutes += elapsed / 60;
    }
    return static_cast<int>(std::min(minutes, static_cast<long long>(std::numeric_limits<int>::max())));
}
PatientStatus Patient::getStatus() const { return status_; }

void Patient::setStatus(PatientStatus status) {
    setStatusAt(status, std::chrono::system_clock::now());
}

void Patient::setStatusAt(PatientStatus status, std::chrono::system_clock::time_point now) {
    if (status_ == PatientStatus::WAITING && status != PatientStatus::WAITING)
        waitingMinutes_ = getWaitingTimeAt(now);
    else if (status_ != PatientStatus::WAITING && status == PatientStatus::WAITING)
        waitingStartedAt_ = now;
    status_ = status;
}

void Patient::setWaitingStartedAtEpochSeconds(long long epochSeconds) {
    waitingStartedAt_ = std::chrono::system_clock::time_point(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            std::chrono::seconds(epochSeconds)));
}

double Patient::calculatePriority() const {
    return PriorityCalculator::calculate(severity_, false, getWaitingTime(), age_);
}

void Patient::display() const {
    Person::display();
    std::cout << " | Age: " << age_ << " | Condition: " << condition_
              << " | Severity: " << severity_ << " | Wait: " << getWaitingTime()
              << " min | Status: " << toString(status_)
              << " | Priority: " << calculatePriority() << '\n';
}
