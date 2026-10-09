#pragma once

#include "Treatment.h"
#include <cstddef>
#include <vector>

class TreatmentManager {
public:
    void addTreatment(const Treatment& treatment);
    const Treatment* findTreatment(int patientId) const;
    void displayTreatmentHistory() const;
    std::size_t countTreatments() const;
    void clear();
    const std::vector<Treatment>& getTreatments() const;

private:
    friend class FileManager;
    std::vector<Treatment> treatments_;
};
