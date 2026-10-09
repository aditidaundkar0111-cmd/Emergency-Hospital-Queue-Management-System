#pragma once

#include "PatientStatus.h"
#include "Person.h"
#include <string>

class Patient : public Person {
public:
    Patient(int id, const std::string& name, int age, const std::string& condition,
            int severity, int waitingMinutes = 0);
    ~Patient() override = default;

    int getAge() const;
    const std::string& getCondition() const;
    int getSeverity() const;
    int getWaitingTime() const;
    PatientStatus getStatus() const;
    virtual double calculatePriority() const;
    void display() const override;

private:
    friend class Hospital;
    void setStatus(PatientStatus status);

    int age_;
    std::string condition_;
    int severity_;
    int waitingMinutes_;
    PatientStatus status_;
};
