#pragma once

#include "Doctor.h"
#include <string>
#include <vector>

class DoctorManager {
public:
    void addDoctor(const Doctor& doctor);
    void displayAllDoctors() const;
    Doctor* findDoctorById(int doctorId);
    const Doctor* findDoctorById(int doctorId) const;
    Doctor* findAvailableDoctor();
    Doctor* findAvailableDoctorBySpecialization(const std::string& specialization);
    bool isDoctorAvailable(int doctorId) const;
    std::size_t size() const;
    std::size_t countAvailableDoctors() const;
    std::size_t countBusyDoctors() const;
    void clear();
    const std::vector<Doctor>& getDoctors() const;

private:
    friend class FileManager;
    std::vector<Doctor> doctors_;
};
