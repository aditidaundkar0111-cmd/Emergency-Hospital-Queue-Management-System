#pragma once

#include "DoctorManager.h"
#include "Patient.h"
#include "TreatmentManager.h"
#include <memory>
#include <vector>

struct HospitalStatistics {
    std::size_t totalPatients = 0;
    std::size_t waitingPatients = 0;
    std::size_t calledPatients = 0;
    std::size_t assignedPatients = 0;
    std::size_t patientsUnderTreatment = 0;
    std::size_t completedPatients = 0;
    std::size_t criticalPatients = 0;
    double averageWaitingTime = 0.0;
    double averagePriority = 0.0;
    std::size_t availableDoctors = 0;
    std::size_t busyDoctors = 0;
    std::size_t totalTreatments = 0;
};

class StatisticsManager {
public:
    HospitalStatistics calculate(const std::vector<std::unique_ptr<Patient>>& patients,
                                 const DoctorManager& doctors,
                                 const TreatmentManager& treatments) const;
};
