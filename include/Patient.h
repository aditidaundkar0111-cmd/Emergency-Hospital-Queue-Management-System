#pragma once

#include "PatientStatus.h"
#include "Person.h"
#include <chrono>
#include <string>

class Patient : public Person {
public:
    Patient(int id, const std::string& name, int age, const std::string& condition,
            int severity, int waitingMinutes = 0);
    Patient(int id, const std::string& name, int age, const std::string& condition,
            int severity, int waitingMinutes, std::chrono::system_clock::time_point waitingStartedAt);
    ~Patient() override = default;

    int getAge() const;
    const std::string& getCondition() const;
    int getSeverity() const;
    int getWaitingTime() const;
    int getWaitingTimeAt(std::chrono::system_clock::time_point now) const;
    PatientStatus getStatus() const;
    virtual double calculatePriority() const;
    void display() const override;

private:
    friend class Hospital;
    friend class FileManager;
    void setStatus(PatientStatus status);
    void setStatusAt(PatientStatus status, std::chrono::system_clock::time_point now);
    void setWaitingStartedAtEpochSeconds(long long epochSeconds);

    int age_;
    std::string condition_;
    int severity_;
    int waitingMinutes_;
    std::chrono::system_clock::time_point waitingStartedAt_;
    PatientStatus status_;
};
