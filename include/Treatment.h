#pragma once

#include <string>

class Treatment {
public:
    Treatment(int patientId, int doctorId, const std::string& diagnosis,
              const std::string& description, bool completed = false);

    int getPatientId() const;
    int getDoctorId() const;
    const std::string& getDiagnosis() const;
    const std::string& getDescription() const;
    bool isCompleted() const;
    void display() const;

private:
    int patientId_;
    int doctorId_;
    std::string diagnosis_;
    std::string description_;
    bool completed_;
};
