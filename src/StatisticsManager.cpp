#include "StatisticsManager.h"
#include "EmergencyPatient.h"

HospitalStatistics StatisticsManager::calculate(
    const std::vector<std::unique_ptr<Patient>>& patients,
    const DoctorManager& doctors, const TreatmentManager& treatments) const {
    HospitalStatistics stats;
    stats.totalPatients = patients.size();
    for (const auto& patient : patients) {
        switch (patient->getStatus()) {
            case PatientStatus::WAITING: ++stats.waitingPatients; break;
            case PatientStatus::CALLED: ++stats.calledPatients; break;
            case PatientStatus::UNDER_TREATMENT: ++stats.patientsUnderTreatment; break;
            case PatientStatus::COMPLETED: ++stats.completedPatients; break;
        }
        if (const auto* emergency = dynamic_cast<const EmergencyPatient*>(patient.get()))
            if (emergency->isCritical()) ++stats.criticalPatients;
        stats.averageWaitingTime += patient->getWaitingTime();
        stats.averagePriority += patient->calculatePriority();
    }
    if (!patients.empty()) {
        stats.averageWaitingTime /= patients.size();
        stats.averagePriority /= patients.size();
    }
    stats.availableDoctors = doctors.countAvailableDoctors();
    stats.busyDoctors = doctors.countBusyDoctors();
    stats.totalTreatments = treatments.countTreatments();
    return stats;
}
