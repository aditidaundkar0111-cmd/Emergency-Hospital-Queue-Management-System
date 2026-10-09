#pragma once

#include "Patient.h"

class EmergencyPatient : public Patient {
public:
    EmergencyPatient(int id, const std::string& name, int age,
                     const std::string& condition, int severity,
                     bool critical, int waitingMinutes = 0);
    bool isCritical() const;
    double calculatePriority() const override;
    void display() const override;

private:
    bool critical_;
};
