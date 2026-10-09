#pragma once

#include <string>

class Doctor {
public:
    Doctor(int doctorId, const std::string& name, const std::string& specialization,
           bool available = true, int currentPatientId = -1);

    int getDoctorId() const;
    const std::string& getName() const;
    const std::string& getSpecialization() const;
    int getCurrentPatientId() const;
    bool isAvailable() const;
    void assignPatient(int patientId);
    void completeTreatment();
    void display() const;

private:
    int doctorId_;
    std::string name_;
    std::string specialization_;
    bool available_;
    int currentPatientId_;
};
