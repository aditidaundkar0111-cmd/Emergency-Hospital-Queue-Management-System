#include "PatientQueue.h"
#include <stdexcept>
#include <utility>

bool PatientPriorityComparator::operator()(const Patient* left, const Patient* right) const {
    if (left == nullptr || right == nullptr)
        throw std::invalid_argument("Patient queue cannot compare a null patient.");
    const double leftPriority = left->calculatePriority();
    const double rightPriority = right->calculatePriority();
    if (leftPriority != rightPriority) return leftPriority < rightPriority;
    // Lower ID wins ties, making ordering reproducible for demonstrations and tests.
    return left->getId() > right->getId();
}

void PatientQueue::addPatient(Patient* patient) {
    if (patient == nullptr) throw std::invalid_argument("Cannot add a null patient.");
    if (findPatient(patient->getId()) != nullptr)
        throw std::invalid_argument("Patient ID is already registered in this queue.");
    if (patient->getStatus() != PatientStatus::WAITING)
        throw std::invalid_argument("Only waiting patients can be added to the queue.");
    registeredPatients_.push_back(patient);
    try {
        waitingQueue_.push(patient);
    } catch (...) {
        registeredPatients_.pop_back();
        throw;
    }
}

void PatientQueue::trackPatient(Patient* patient) {
    if (patient == nullptr) throw std::invalid_argument("Cannot track a null patient.");
    if (findPatient(patient->getId()) != nullptr)
        throw std::invalid_argument("Patient ID is already registered in this queue.");
    registeredPatients_.push_back(patient);
}

void PatientQueue::rebuildWaitingQueue() {
    decltype(waitingQueue_) rebuilt;
    for (Patient* patient : registeredPatients_)
        if (patient->getStatus() == PatientStatus::WAITING) rebuilt.push(patient);
    waitingQueue_ = std::move(rebuilt);
}

void PatientQueue::clear() {
    waitingQueue_ = decltype(waitingQueue_){};
    registeredPatients_.clear();
}

Patient* PatientQueue::getNextPatient() {
    if (waitingQueue_.empty()) return nullptr;
    Patient* patient = waitingQueue_.top();
    waitingQueue_.pop();
    return patient;
}

Patient* PatientQueue::findPatient(int patientId) const {
    for (Patient* patient : registeredPatients_)
        if (patient != nullptr && patient->getId() == patientId) return patient;
    return nullptr;
}

void PatientQueue::displayQueue() const {
    auto copy = waitingQueue_;
    while (!copy.empty()) {
        copy.top()->display();
        copy.pop();
    }
}

bool PatientQueue::isEmpty() const { return waitingQueue_.empty(); }
std::size_t PatientQueue::size() const { return waitingQueue_.size(); }

std::vector<Patient*> PatientQueue::getWaitingPatients() const {
    auto copy = waitingQueue_;
    std::vector<Patient*> patients;
    patients.reserve(copy.size());
    while (!copy.empty()) {
        patients.push_back(copy.top());
        copy.pop();
    }
    return patients;
}
