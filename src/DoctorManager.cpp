#include "DoctorManager.h"
#include <iostream>
#include <stdexcept>

void DoctorManager::addDoctor(const Doctor& doctor) {
    if (findDoctorById(doctor.getDoctorId()) != nullptr)
        throw std::invalid_argument("Doctor ID is already registered.");
    doctors_.push_back(doctor);
}

void DoctorManager::displayAllDoctors() const {
    for (const Doctor& doctor : doctors_) doctor.display();
}

Doctor* DoctorManager::findDoctorById(int doctorId) {
    for (Doctor& doctor : doctors_)
        if (doctor.getDoctorId() == doctorId) return &doctor;
    return nullptr;
}

const Doctor* DoctorManager::findDoctorById(int doctorId) const {
    for (const Doctor& doctor : doctors_)
        if (doctor.getDoctorId() == doctorId) return &doctor;
    return nullptr;
}

Doctor* DoctorManager::findAvailableDoctor() {
    for (Doctor& doctor : doctors_)
        if (doctor.isAvailable()) return &doctor;
    return nullptr;
}

Doctor* DoctorManager::findAvailableDoctorBySpecialization(const std::string& specialization) {
    for (Doctor& doctor : doctors_)
        if (doctor.isAvailable() && doctor.getSpecialization() == specialization) return &doctor;
    return nullptr;
}

bool DoctorManager::isDoctorAvailable(int doctorId) const {
    const Doctor* doctor = findDoctorById(doctorId);
    return doctor != nullptr && doctor->isAvailable();
}

std::size_t DoctorManager::size() const { return doctors_.size(); }

std::size_t DoctorManager::countAvailableDoctors() const {
    std::size_t count = 0;
    for (const Doctor& doctor : doctors_) if (doctor.isAvailable()) ++count;
    return count;
}

std::size_t DoctorManager::countBusyDoctors() const {
    return doctors_.size() - countAvailableDoctors();
}

void DoctorManager::clear() { doctors_.clear(); }

const std::vector<Doctor>& DoctorManager::getDoctors() const { return doctors_; }
