#include "EmergencyPatient.h"
#include "DoctorManager.h"
#include "Exceptions.h"
#include "FileManager.h"
#include "Hospital.h"
#include "PatientQueue.h"
#include "RegularPatient.h"
#include "UserManager.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#if defined(_WIN32)
#include <direct.h>
#else
#include <unistd.h>
#endif

namespace {
std::string readWholeFile(const std::string& path) {
    std::ifstream input(path.c_str(), std::ios::binary);
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

void removeTestDirectory(const std::string& directory) {
    std::remove((directory + "/patients.txt").c_str());
    std::remove((directory + "/doctors.txt").c_str());
    std::remove((directory + "/treatments.txt").c_str());
#if defined(_WIN32)
    _rmdir(directory.c_str());
#else
    rmdir(directory.c_str());
#endif
}
}

class FixedPriorityPatient : public Patient {
public:
    FixedPriorityPatient(int id, double priority)
        : Patient(id, "Test patient", 25, "Test condition", 1), priority_(priority) {}
    double calculatePriority() const override { return priority_; }

private:
    double priority_;
};

int main() {
    const std::string missingAuthDirectory = "data/auth_missing_directory";
    const std::string missingAuthFile = missingAuthDirectory + "/users.txt";
    std::remove(missingAuthFile.c_str());
#if defined(_WIN32)
    _rmdir(missingAuthDirectory.c_str());
#else
    rmdir(missingAuthDirectory.c_str());
#endif
    {
        UserManager users(missingAuthFile);
        const AuthUser created = users.registerUser("First Account", "first_missing_test",
                                                    "First-secure-passphrase");
        assert(created.username == "first_missing_test");
        assert(readWholeFile(missingAuthFile).find("First-secure-passphrase") == std::string::npos);
        UserManager reloaded(missingAuthFile);
        assert(reloaded.login("first_missing_test", "First-secure-passphrase").user.fullName ==
               "First Account");
    }
    std::remove(missingAuthFile.c_str());
#if defined(_WIN32)
    _rmdir(missingAuthDirectory.c_str());
#else
    rmdir(missingAuthDirectory.c_str());
#endif

    const std::string authTestFile = "data/auth_test_users.txt";
    std::remove(authTestFile.c_str());
    {
        UserManager users(authTestFile);
        const AuthUser created = users.registerUser("Test Account", "phase6_test", "A-secure-passphrase");
        assert(created.fullName == "Test Account");
        assert(readWholeFile(authTestFile).find("A-secure-passphrase") == std::string::npos);
        bool duplicateRejected = false;
        try { users.registerUser("Duplicate", "PHASE6_TEST", "A-secure-passphrase"); }
        catch (const std::logic_error&) { duplicateRejected = true; }
        assert(duplicateRejected);
        const AuthSession session = users.login("PHASE6_TEST", "A-secure-passphrase");
        assert(session.user.username == "phase6_test" && !session.token.empty());
        AuthUser authenticated;
        assert(users.findSession(session.token, authenticated));
        assert(authenticated.fullName == "Test Account");
        assert(users.logout(session.token));
        assert(!users.findSession(session.token, authenticated));
    }
    {
        UserManager users(authTestFile);
        users.registerUser("Second Account", "phase6_second", "Another-secure-passphrase");
        const AuthSession session = users.login("phase6_test", "A-secure-passphrase");
        assert(session.user.fullName == "Test Account");
        assert(users.login("phase6_second", "Another-secure-passphrase").user.fullName ==
               "Second Account");
        std::ifstream accountRows(authTestFile.c_str());
        std::size_t accountCount = 0;
        std::string accountRow;
        while (std::getline(accountRows, accountRow)) ++accountCount;
        assert(accountCount == 2);
        bool invalidPasswordRejected = false;
        try { users.login("phase6_test", "wrong-password"); }
        catch (const std::invalid_argument&) { invalidPasswordRejected = true; }
        assert(invalidPasswordRejected);
    }
    std::remove(authTestFile.c_str());

    RegularPatient regular(101, "Asha", 30, "Fever", 2, 10);
    assert(regular.getStatus() == PatientStatus::WAITING);
    assert(regular.calculatePriority() == 45.0);

    EmergencyPatient emergency(102, "Ravi", 70, "Chest pain", 5, true, 0);
    assert(emergency.calculatePriority() == 120.0);
    assert(emergency.calculatePriority() > regular.calculatePriority());

    using SystemClock = std::chrono::system_clock;
    const SystemClock::time_point simulatedRegistration = SystemClock::now();
    Patient clockedPatient(106, "Clocked", 30, "Test", 1, 0, simulatedRegistration);
    assert(clockedPatient.getWaitingTimeAt(simulatedRegistration) == 0);
    Hospital registrationClock;
    Patient& newlyRegistered = registrationClock.registerRegularPatient(1050, "New arrival", 30,
        "Check", 1);
    assert(newlyRegistered.getWaitingTime() == 0);
    assert(newlyRegistered.getWaitingTimeAt(simulatedRegistration + std::chrono::minutes(2)) >= 2);
    assert(clockedPatient.getWaitingTimeAt(simulatedRegistration + std::chrono::minutes(5)) == 5);
    assert(clockedPatient.getWaitingTimeAt(simulatedRegistration + std::chrono::minutes(15)) == 15);

    Hospital agedQueue;
    agedQueue.registerPatient(std::make_unique<Patient>(107, "Long wait", 25, "A", 1, 0,
        simulatedRegistration - std::chrono::minutes(45)));
    agedQueue.registerRegularPatient(108, "Higher severity", 25, "B", 2, 0);
    const std::vector<Patient*> agedOrder = agedQueue.getWaitingPatients();
    assert(agedOrder.size() == 2 && agedOrder.front()->getId() == 107);

    Hospital freezeQueue;
    Patient& waitingToCall = freezeQueue.registerPatient(std::make_unique<Patient>(109,
        "Freeze wait", 25, "A", 1, 0, simulatedRegistration - std::chrono::minutes(12)));
    freezeQueue.callNextPatient(simulatedRegistration);
    assert(waitingToCall.getWaitingTimeAt(simulatedRegistration + std::chrono::hours(1)) == 12);

    Hospital assignedClock;
    const SystemClock::time_point twoMinutesAgo = simulatedRegistration - std::chrono::minutes(2);
    Patient& clockLifecyclePatient = assignedClock.registerPatient(std::make_unique<Patient>(110,
        "Lifecycle clock", 25, "A", 2, 0, twoMinutesAgo));
    assignedClock.addDoctor(Doctor(111, "Dr Clock", "General Physician"));
    assert(clockLifecyclePatient.getWaitingTimeAt(simulatedRegistration) >= 2);
    assert(assignedClock.callNextPatient(simulatedRegistration) == &clockLifecyclePatient);
    const int frozenAtCall = clockLifecyclePatient.getWaitingTime();
    assignedClock.assignDoctor(110, 111);
    assert(clockLifecyclePatient.getStatus() == PatientStatus::ASSIGNED);
    assert(!assignedClock.findDoctor(111)->isAvailable());
    assert(clockLifecyclePatient.getWaitingTimeAt(simulatedRegistration + std::chrono::hours(1)) == frozenAtCall);
    HospitalStatistics assignedStats = assignedClock.getStatistics();
    assert(assignedStats.totalPatients == 1 && assignedStats.waitingPatients == 0 &&
           assignedStats.assignedPatients == 1 && assignedStats.patientsUnderTreatment == 0 &&
           assignedStats.completedPatients == 0);

    const std::string assignedClockData = "data/test_assigned_clock";
    assignedClock.saveData(assignedClockData);
    Hospital assignedClockReloaded;
    assignedClockReloaded.loadData(assignedClockData);
    assert(assignedClockReloaded.findPatient(110)->getStatus() == PatientStatus::ASSIGNED);
    assert(assignedClockReloaded.findPatient(110)->getWaitingTime() == frozenAtCall);
    assert(!assignedClockReloaded.findDoctor(111)->isAvailable());
    assignedClockReloaded.startTreatment(110);
    assignedStats = assignedClockReloaded.getStatistics();
    assert(assignedStats.assignedPatients == 0 && assignedStats.patientsUnderTreatment == 1);
    assignedClockReloaded.completeTreatment(110, 111, "Stable", "Observation");
    assignedStats = assignedClockReloaded.getStatistics();
    assert(assignedStats.completedPatients == 1 && assignedStats.assignedPatients == 0 &&
           assignedStats.patientsUnderTreatment == 0);
    assert(assignedClockReloaded.findDoctor(111)->isAvailable());
    assert(assignedClockReloaded.findPatient(110)->getWaitingTimeAt(
        simulatedRegistration + std::chrono::hours(1)) == frozenAtCall);

    const std::string legacyWaitingData = "data/test_legacy_waiting";
    FileManager(legacyWaitingData).saveAll(Hospital());
    {
        std::ofstream legacyPatients(legacyWaitingData + "/patients.txt");
        legacyPatients << "P 112 REGULAR 30 2 0 0 WAITING \"Legacy patient\" \"Check\"\n";
    }
    Hospital legacyWaiting;
    const SystemClock::time_point beforeLegacyLoad = SystemClock::now();
    legacyWaiting.loadData(legacyWaitingData);
    const SystemClock::time_point afterLegacyLoad = SystemClock::now();
    assert(legacyWaiting.findPatient(112) != nullptr);
    assert(legacyWaiting.findPatient(112)->getWaitingTimeAt(beforeLegacyLoad) == 0);
    std::string migratedPatientRecord;
    {
        std::ifstream migratedPatients(legacyWaitingData + "/patients.txt");
        std::getline(migratedPatients, migratedPatientRecord); // Header
        std::getline(migratedPatients, migratedPatientRecord);
    }
    std::istringstream migratedRecord(migratedPatientRecord);
    char patientTag = '\0'; int legacyId = 0, legacyAge = 0, legacySeverity = 0;
    int legacyWait = 0, legacyCritical = 0; std::string legacyType, legacyStatus, legacyName, legacyCondition;
    long long migratedEpoch = -1;
    assert(migratedRecord >> patientTag >> legacyId >> legacyType >> legacyAge >> legacySeverity >>
           legacyWait >> legacyCritical >> legacyStatus >> std::quoted(legacyName) >>
           std::quoted(legacyCondition) >> migratedEpoch);
    const long long beforeEpoch = std::chrono::duration_cast<std::chrono::seconds>(
        beforeLegacyLoad.time_since_epoch()).count();
    const long long afterEpoch = std::chrono::duration_cast<std::chrono::seconds>(
        afterLegacyLoad.time_since_epoch()).count();
    assert(migratedEpoch >= beforeEpoch && migratedEpoch <= afterEpoch);
    Hospital legacyReloaded;
    legacyReloaded.loadData(legacyWaitingData);
    std::string reloadedPatientRecord;
    {
        std::ifstream migratedPatients(legacyWaitingData + "/patients.txt");
        std::getline(migratedPatients, reloadedPatientRecord);
        std::getline(migratedPatients, reloadedPatientRecord);
    }
    assert(reloadedPatientRecord == migratedPatientRecord);
    assert(legacyReloaded.findPatient(112)->getWaitingTimeAt(
        SystemClock::time_point(std::chrono::seconds(migratedEpoch)) + std::chrono::minutes(2)) == 2);

    bool rejectedInvalidAge = false;
    try {
        RegularPatient invalid(103, "Sam", 150, "Injury", 1);
    } catch (const InvalidPatientDataException&) {
        rejectedInvalidAge = true;
    }
    assert(rejectedInvalidAge);

    EmergencyPatient critical(104, "Mira", 35, "Critical", 5, true, 100000);
    RegularPatient minor(105, "Noah", 35, "Minor", 1, 0);
    assert(critical.calculatePriority() > minor.calculatePriority());

    FixedPriorityPatient p101(101, 70.0);
    FixedPriorityPatient p102(102, 155.0);
    FixedPriorityPatient p103(103, 80.0);
    FixedPriorityPatient p104(104, 160.0);
    PatientQueue queue;
    queue.addPatient(&p101);
    queue.addPatient(&p102);
    queue.addPatient(&p103);
    queue.addPatient(&p104);
    assert(queue.size() == 4);
    assert(queue.findPatient(103) == &p103);
    assert(queue.findPatient(999) == nullptr);
    const int expectedOrder[] = {104, 102, 103, 101};
    for (int expectedId : expectedOrder) {
        Patient* next = queue.getNextPatient();
        assert(next != nullptr && next->getId() == expectedId);
    }
    assert(queue.isEmpty());
    assert(queue.findPatient(104) == &p104); // Search includes patients already called.

    DoctorManager doctors;
    doctors.addDoctor(Doctor(1, "Dr Lee", "Cardiologist"));
    doctors.addDoctor(Doctor(2, "Dr Rao", "General Physician"));
    assert(doctors.findDoctorById(1) != nullptr);
    assert(doctors.findDoctorById(99) == nullptr);
    assert(doctors.isDoctorAvailable(1));
    assert(doctors.findAvailableDoctorBySpecialization("Cardiologist") != nullptr);
    Doctor* doctor = doctors.findDoctorById(1);
    doctor->assignPatient(104);
    assert(!doctor->isAvailable());
    assert(!doctors.isDoctorAvailable(1));
    bool busyDoctorRejected = false;
    try {
        doctor->assignPatient(102);
    } catch (const std::logic_error&) {
        busyDoctorRejected = true;
    }
    assert(busyDoctorRejected);
    assert(doctor->getCurrentPatientId() == 104);
    doctor->completeTreatment();
    assert(doctor->isAvailable());

    Hospital hospital;
    auto& h101 = hospital.registerPatient(std::make_unique<FixedPriorityPatient>(101, 70.0));
    auto& h102 = hospital.registerPatient(std::make_unique<FixedPriorityPatient>(102, 155.0));
    auto& h103 = hospital.registerPatient(std::make_unique<FixedPriorityPatient>(103, 80.0));
    auto& h104 = hospital.registerPatient(std::make_unique<FixedPriorityPatient>(104, 160.0));
    assert(h101.getStatus() == PatientStatus::WAITING);
    hospital.addDoctor(Doctor(201, "Dr Heart", "Cardiologist"));
    hospital.addDoctor(Doctor(202, "Dr General", "General Physician"));
    hospital.addDoctor(Doctor(203, "Dr Bone", "Orthopedic"));

    Patient* called = hospital.callNextPatient();
    assert(called == &h104 && called->getStatus() == PatientStatus::CALLED);
    assert(hospital.findPatient(104) == &h104);
    assert(hospital.findPatient(999) == nullptr);

    bool waitingAssignmentRejected = false;
    try { hospital.assignDoctor(101, 201); }
    catch (const std::logic_error&) { waitingAssignmentRejected = true; }
    assert(waitingAssignmentRejected);
    bool missingPatientRejected = false;
    try { hospital.assignDoctor(999, 201); }
    catch (const std::invalid_argument&) { missingPatientRejected = true; }
    assert(missingPatientRejected);
    bool missingDoctorRejected = false;
    try { hospital.assignDoctor(104, 999); }
    catch (const std::invalid_argument&) { missingDoctorRejected = true; }
    assert(missingDoctorRejected);

    hospital.assignDoctor(104, 201);
    assert(h104.getStatus() == PatientStatus::ASSIGNED);
    assert(!hospital.findDoctor(201)->isAvailable());
    bool assignedCompletionRejected = false;
    try { hospital.completeTreatment(104, 201, "N/A", "N/A"); }
    catch (const std::logic_error&) { assignedCompletionRejected = true; }
    assert(assignedCompletionRejected && h104.getStatus() == PatientStatus::ASSIGNED);
    hospital.startTreatment(104);
    assert(h104.getStatus() == PatientStatus::UNDER_TREATMENT);
    Patient* secondCalled = hospital.callNextPatient();
    assert(secondCalled == &h102 && h102.getStatus() == PatientStatus::CALLED);
    bool busyDoctorRejectedByHospital = false;
    try { hospital.assignDoctor(102, 201); }
    catch (const std::logic_error&) { busyDoctorRejectedByHospital = true; }
    assert(busyDoctorRejectedByHospital);
    bool waitingCompletionRejected = false;
    try { hospital.completeTreatment(101, 202, "N/A", "N/A"); }
    catch (const std::logic_error&) { waitingCompletionRejected = true; }
    assert(waitingCompletionRejected);
    bool calledCompletionRejected = false;
    try { hospital.completeTreatment(102, 202, "N/A", "N/A"); }
    catch (const std::logic_error&) { calledCompletionRejected = true; }
    assert(calledCompletionRejected);


    hospital.completeTreatment(104, 201, "Stable", "Observation");
    assert(h104.getStatus() == PatientStatus::COMPLETED);
    assert(hospital.findDoctor(201)->isAvailable());
    assert(hospital.findTreatment(104) != nullptr);
    assert(hospital.findTreatment(104)->isCompleted());
    bool completedAssignmentRejected = false;
    try { hospital.assignDoctor(104, 201); }
    catch (const std::logic_error&) { completedAssignmentRejected = true; }
    assert(completedAssignmentRejected);
    bool completedRepeatRejected = false;
    try { hospital.completeTreatment(104, 201, "Stable", "Observation"); }
    catch (const std::logic_error&) { completedRepeatRejected = true; }
    assert(completedRepeatRejected);

    const int remainingOrder[] = {103, 101};
    for (int expectedId : remainingOrder) {
        Patient* next = hospital.callNextPatient();
        assert(next != nullptr && next->getId() == expectedId);
    }
    const HospitalStatistics stats = hospital.getStatistics();
    assert(stats.totalPatients == 4);
    assert(stats.waitingPatients == 0 && stats.calledPatients == 3);
    assert(stats.completedPatients == 1 && stats.patientsUnderTreatment == 0);
    assert(stats.availableDoctors == 3 && stats.busyDoctors == 0);
    assert(stats.totalTreatments == 1);
    assert(h103.getStatus() == PatientStatus::CALLED);

    const HospitalStatistics emptyStats = Hospital().getStatistics();
    assert(emptyStats.totalPatients == 0 && emptyStats.waitingPatients == 0);
    assert(emptyStats.averageWaitingTime == 0.0 && emptyStats.averagePriority == 0.0);

    const std::string queueData = "data/test_queue_roundtrip";
    const std::string secondQueueData = "data/test_queue_resave";
    Hospital queueBeforeSave;
    queueBeforeSave.registerRegularPatient(501, "Queue one", 25, "A", 1, 0);
    queueBeforeSave.registerRegularPatient(502, "Queue two", 40, "B", 2, 10);
    queueBeforeSave.registerRegularPatient(503, "Queue three", 70, "C", 3, 20);
    queueBeforeSave.registerEmergencyPatient(504, "Queue four", 30, "D", 5, true, 30);
    queueBeforeSave.saveData(queueData);
    Hospital queueAfterLoad;
    queueAfterLoad.loadData(queueData);
    queueAfterLoad.saveData(secondQueueData);
    assert(readWholeFile(queueData + "/patients.txt") ==
           readWholeFile(secondQueueData + "/patients.txt"));
    assert(readWholeFile(queueData + "/doctors.txt") ==
           readWholeFile(secondQueueData + "/doctors.txt"));
    assert(readWholeFile(queueData + "/treatments.txt") ==
           readWholeFile(secondQueueData + "/treatments.txt"));
    const int persistedQueueOrder[] = {504, 503, 502, 501};
    for (int expectedId : persistedQueueOrder) {
        Patient* next = queueAfterLoad.callNextPatient();
        assert(next != nullptr && next->getId() == expectedId);
    }
    const HospitalStatistics queueStats = queueAfterLoad.getStatistics();
    assert(queueStats.totalPatients == 4 && queueStats.calledPatients == 4);
    assert(queueStats.criticalPatients == 1);
    assert(queueStats.averageWaitingTime == 15.0);
    assert(queueStats.averagePriority == 67.5);

    const std::string lifecycleData = "data/test_lifecycle_roundtrip";
    const std::string lifecycleResaveData = "data/test_lifecycle_resave";
    Hospital lifecycleBeforeSave;
    lifecycleBeforeSave.registerRegularPatient(601, "Waiting", 30, "A", 3, 5);
    lifecycleBeforeSave.registerEmergencyPatient(602, "Completed", 45, "B", 5, true, 0);
    lifecycleBeforeSave.registerRegularPatient(603, "Called one", 25, "C", 4, 0);
    lifecycleBeforeSave.registerRegularPatient(604, "In treatment", 35, "D", 2, 0);
    lifecycleBeforeSave.registerRegularPatient(605, "Called two", 60, "E", 1, 0);
    lifecycleBeforeSave.addDoctor(Doctor(701, "Dr Completed", "General Physician"));
    lifecycleBeforeSave.addDoctor(Doctor(702, "Dr Busy", "Cardiologist"));
    lifecycleBeforeSave.addDoctor(Doctor(703, "Dr Free", "Orthopedic"));
    assert(lifecycleBeforeSave.callNextPatient()->getId() == 602);
    lifecycleBeforeSave.assignDoctor(602, 701);
    lifecycleBeforeSave.startTreatment(602);
    lifecycleBeforeSave.completeTreatment(602, 701, "Diagnosis X", "Treatment X");
    assert(lifecycleBeforeSave.callNextPatient()->getId() == 603);
    assert(lifecycleBeforeSave.callNextPatient()->getId() == 601);
    assert(lifecycleBeforeSave.callNextPatient()->getId() == 604);
    lifecycleBeforeSave.assignDoctor(604, 702);
    assert(lifecycleBeforeSave.findPatient(605)->getStatus() == PatientStatus::WAITING);
    lifecycleBeforeSave.saveData(lifecycleData);

    Hospital lifecycleAfterLoad;
    lifecycleAfterLoad.loadData(lifecycleData);
    assert(lifecycleAfterLoad.findPatient(602)->getStatus() == PatientStatus::COMPLETED);
    assert(lifecycleAfterLoad.findPatient(603)->getStatus() == PatientStatus::CALLED);
    assert(lifecycleAfterLoad.findPatient(604)->getStatus() == PatientStatus::ASSIGNED);
    assert(lifecycleAfterLoad.findPatient(605)->getStatus() == PatientStatus::WAITING);
    assert(dynamic_cast<const EmergencyPatient*>(lifecycleAfterLoad.findPatient(602)) != nullptr);
    assert(!lifecycleAfterLoad.findDoctor(702)->isAvailable());
    assert(lifecycleAfterLoad.findDoctor(702)->getCurrentPatientId() == 604);
    assert(lifecycleAfterLoad.findDoctor(701)->isAvailable());
    assert(lifecycleAfterLoad.findTreatment(602) != nullptr);
    assert(lifecycleAfterLoad.findTreatment(602)->getDiagnosis() == "Diagnosis X");
    const HospitalStatistics reloadedStats = lifecycleAfterLoad.getStatistics();
    assert(reloadedStats.totalPatients == 5 && reloadedStats.waitingPatients == 1);
    assert(reloadedStats.calledPatients == 2 && reloadedStats.assignedPatients == 1 && reloadedStats.patientsUnderTreatment == 0);
    assert(reloadedStats.completedPatients == 1 && reloadedStats.criticalPatients == 1);
    assert(reloadedStats.averageWaitingTime == 1.0 && reloadedStats.averagePriority == 63.5);
    assert(reloadedStats.availableDoctors == 2 && reloadedStats.busyDoctors == 1);
    assert(reloadedStats.totalTreatments == 1);
    assert(lifecycleAfterLoad.callNextPatient()->getId() == 605);
    lifecycleAfterLoad.loadData(lifecycleData);
    lifecycleAfterLoad.saveData(lifecycleResaveData);
    assert(readWholeFile(lifecycleData + "/patients.txt") ==
           readWholeFile(lifecycleResaveData + "/patients.txt"));
    assert(readWholeFile(lifecycleData + "/doctors.txt") ==
           readWholeFile(lifecycleResaveData + "/doctors.txt"));
    assert(readWholeFile(lifecycleData + "/treatments.txt") ==
           readWholeFile(lifecycleResaveData + "/treatments.txt"));

    const std::string waitingClockData = "data/test_waiting_clock";
    Hospital waitingClockBeforeSave;
    const SystemClock::time_point tenMinutesAgo = SystemClock::now() - std::chrono::minutes(10);
    waitingClockBeforeSave.registerPatient(std::make_unique<Patient>(606, "Persisted wait", 30,
        "Check", 2, 0, tenMinutesAgo));
    waitingClockBeforeSave.saveData(waitingClockData);
    Hospital waitingClockAfterLoad;
    waitingClockAfterLoad.loadData(waitingClockData);
    Patient* persistedWaiting = waitingClockAfterLoad.findPatient(606);
    assert(persistedWaiting != nullptr && persistedWaiting->getWaitingTime() >= 10);
    const int waitingAtCall = persistedWaiting->getWaitingTime();
    waitingClockAfterLoad.callNextPatient();
    assert(persistedWaiting->getWaitingTimeAt(SystemClock::now() + std::chrono::hours(1)) == waitingAtCall);
    waitingClockAfterLoad.saveData(waitingClockData);
    Hospital frozenClockAfterLoad;
    frozenClockAfterLoad.loadData(waitingClockData);
    assert(frozenClockAfterLoad.findPatient(606)->getWaitingTimeAt(
        SystemClock::now() + std::chrono::hours(1)) == waitingAtCall);

    const std::string malformedData = "data/test_malformed";
    FileManager(malformedData).saveAll(Hospital());
    {
        std::ofstream patients(malformedData + "/patients.txt");
        patients << "P 801 REGULAR 25 3 10 0 WAITING \"Good\" \"Condition\"\n"
                    "P 801 REGULAR 25 3 10 0 WAITING \"Duplicate\" \"Condition\"\n"
                    "P 802 REGULAR 150 3 0 0 WAITING \"Bad age\" \"Condition\"\n"
                    "P 803 REGULAR 30 2 0 0 UNDER_TREATMENT \"Valid busy\" \"Condition\"\n"
                    "P 804 REGULAR 30 2 0 0 UNDER_TREATMENT \"Orphan busy\" \"Condition\"\n"
                    "malformed patient line\n";
        std::ofstream doctorsFile(malformedData + "/doctors.txt");
        doctorsFile << "D 811 0 803 \"Valid doctor\" \"General Physician\"\n"
                       "D 811 1 -1 \"Duplicate doctor\" \"General Physician\"\n"
                       "D 812 0 999 \"Missing patient\" \"Surgeon\"\n"
                       "malformed doctor line\n";
        std::ofstream treatmentsFile(malformedData + "/treatments.txt");
        treatmentsFile << "T 803 811 0 \"Valid in progress\" \"Observation\"\n"
                          "T 803 811 0 \"Duplicate treatment\" \"Observation\"\n"
                          "T 999 811 1 \"Missing patient\" \"Procedure\"\n"
                          "malformed treatment line\n";
    }
    Hospital malformedLoaded;
    malformedLoaded.registerRegularPatient(899, "Preserved", 20, "Existing", 1);
    bool malformedRejected = false;
    try { malformedLoaded.loadData(malformedData); }
    catch (const std::runtime_error&) { malformedRejected = true; }
    assert(malformedRejected);
    assert(malformedLoaded.getStatistics().totalPatients == 1);
    assert(malformedLoaded.findPatient(899) != nullptr);

    const std::string incompleteData = "data/test_incomplete";
    FileManager(incompleteData).saveAll(Hospital());
    std::remove((incompleteData + "/doctors.txt").c_str());
    bool incompleteRejected = false;
    try { malformedLoaded.loadData(incompleteData); }
    catch (const std::runtime_error&) { incompleteRejected = true; }
    assert(incompleteRejected && malformedLoaded.findPatient(899) != nullptr);

    const std::string missingData = "data/test_missing_directory";
    Hospital missingLoaded;
    missingLoaded.registerRegularPatient(899, "Preserved", 20, "Temporary", 1);
    bool missingRejected = false;
    try { missingLoaded.loadData(missingData); }
    catch (const std::runtime_error&) { missingRejected = true; }
    assert(missingRejected);
    assert(missingLoaded.getStatistics().totalPatients == 1);
    assert(missingLoaded.findPatient(899) != nullptr);

    removeTestDirectory(queueData);
    removeTestDirectory(secondQueueData);
    removeTestDirectory(lifecycleData);
    removeTestDirectory(lifecycleResaveData);
    removeTestDirectory(waitingClockData);
    removeTestDirectory(assignedClockData);
    removeTestDirectory(legacyWaitingData);
    removeTestDirectory(malformedData);
    removeTestDirectory(incompleteData);

    std::cout << "Phase 1 through Phase 4 checks passed.\n";
}
