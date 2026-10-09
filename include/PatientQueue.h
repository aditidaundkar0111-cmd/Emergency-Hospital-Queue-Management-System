#pragma once

#include "Patient.h"
#include <queue>
#include <vector>

struct PatientPriorityComparator {
    bool operator()(const Patient* left, const Patient* right) const;
};

class PatientQueue {
public:
    void addPatient(Patient* patient);
    void trackPatient(Patient* patient);
    void rebuildWaitingQueue();
    void clear();
    Patient* getNextPatient();
    Patient* findPatient(int patientId) const;
    void displayQueue() const;
    bool isEmpty() const;
    std::size_t size() const;
    std::vector<Patient*> getWaitingPatients() const;

private:
    std::priority_queue<Patient*, std::vector<Patient*>, PatientPriorityComparator> waitingQueue_;
    std::vector<Patient*> registeredPatients_;
};
