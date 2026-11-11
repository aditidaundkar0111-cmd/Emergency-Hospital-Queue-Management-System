#pragma once

#include "DoctorManager.h"
#include "EmergencyPatient.h"
#include "PatientQueue.h"
#include "RegularPatient.h"
#include "StatisticsManager.h"
#include "TreatmentManager.h"
#include <chrono>
#include <memory>
#include <string>
#include <vector>

class Hospital {
public:
    Patient& registerPatient(std::unique_ptr<Patient> patient);
    Patient& registerEmergencyPatient(int id, const std::string& name, int age,
                                      const std::string& condition, int severity,
                                      bool critical, int waitingMinutes = 0);
    Patient& registerRegularPatient(int id, const std::string& name, int age,
                                    const std::string& condition, int severity,
                                    int waitingMinutes = 0);
    void addDoctor(const Doctor& doctor);
    void displayWaitingQueue() const;
    void displayAllPatients() const;
    Patient* findPatient(int patientId);
    const Patient* findPatient(int patientId) const;
    void displayDoctors() const;
    Patient* callNextPatient(std::chrono::system_clock::time_point now =
                             std::chrono::system_clock::now());
    void assignDoctor(int patientId, int doctorId);
    void startTreatment(int patientId);
    void completeTreatment(int patientId, int doctorId, const std::string& diagnosis,
                           const std::string& description);
    const Treatment* findTreatment(int patientId) const;
    void displayTreatmentHistory() const;
    HospitalStatistics getStatistics() const;
    Doctor* findDoctor(int doctorId);
    const std::vector<std::unique_ptr<Patient>>& getPatients() const;
    std::vector<Patient*> getWaitingPatients() const;
    const std::vector<Doctor>& getDoctors() const;
    const std::vector<Treatment>& getTreatments() const;
    void saveData(const std::string& directory = "data") const;
    void loadData(const std::string& directory = "data");

private:
    friend class FileManager;
    void restorePatient(std::unique_ptr<Patient> patient, PatientStatus status);
    void restoreTreatment(const Treatment& treatment);
    void rebuildWaitingQueue();
    void discardPatient(int patientId);
    void replaceWith(Hospital&& other);

    // Declared first so it is destroyed last, after the non-owning queue pointers.
    std::vector<std::unique_ptr<Patient>> patients_;
    PatientQueue patientQueue_;
    DoctorManager doctorManager_;
    TreatmentManager treatmentManager_;
    StatisticsManager statisticsManager_;
};
