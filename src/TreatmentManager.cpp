#include "TreatmentManager.h"
#include <iostream>

void TreatmentManager::addTreatment(const Treatment& treatment) {
    treatments_.push_back(treatment);
}

const Treatment* TreatmentManager::findTreatment(int patientId) const {
    for (const Treatment& treatment : treatments_)
        if (treatment.getPatientId() == patientId) return &treatment;
    return nullptr;
}

void TreatmentManager::displayTreatmentHistory() const {
    if (treatments_.empty()) {
        std::cout << "No treatment records.\n";
        return;
    }
    for (const Treatment& treatment : treatments_) treatment.display();
}

std::size_t TreatmentManager::countTreatments() const { return treatments_.size(); }

void TreatmentManager::clear() { treatments_.clear(); }

const std::vector<Treatment>& TreatmentManager::getTreatments() const { return treatments_; }
