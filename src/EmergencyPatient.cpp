#include "EmergencyPatient.h"
#include "PriorityCalculator.h"
#include <iostream>

EmergencyPatient::EmergencyPatient(int id, const std::string& name, int age,
                                   const std::string& condition, int severity,
                                   bool critical, int waitingMinutes)
    : Patient(id, name, age, condition, severity, waitingMinutes), critical_(critical) {}

bool EmergencyPatient::isCritical() const { return critical_; }

double EmergencyPatient::calculatePriority() const {
    return PriorityCalculator::calculate(getSeverity(), critical_, getWaitingTime(), getAge());
}

void EmergencyPatient::display() const {
    std::cout << "Emergency Patient | ";
    Patient::display();
    std::cout << "  Critical: " << (critical_ ? "Yes" : "No") << '\n';
}
