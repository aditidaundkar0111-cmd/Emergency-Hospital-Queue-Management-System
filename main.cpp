#include "Hospital.h"
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
int readInteger(const std::string& prompt, int minimum, int maximum) {
    while (true) {
        std::cout << prompt;
        int value = 0;
        if (std::cin >> value) {
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            if (value >= minimum && value <= maximum) return value;
            std::cout << "Enter a number from " << minimum << " to " << maximum << ".\n";
        } else {
            if (std::cin.eof()) throw std::runtime_error("Input ended.");
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "Invalid number. Please try again.\n";
        }
    }
}

std::string readText(const std::string& prompt) {
    while (true) {
        std::cout << prompt;
        std::string value;
        std::getline(std::cin, value);
        if (std::cin.eof()) throw std::runtime_error("Input ended.");
        if (value.find_first_not_of(" \t\r\n") != std::string::npos) return value;
        std::cout << "This field cannot be empty.\n";
    }
}

void printMenu() {
    std::cout << "\n=============================================\n"
                 "EMERGENCY HOSPITAL QUEUE OPTIMIZER\n"
                 "=============================================\n"
                 "1. Register Patient\n2. Register Emergency Patient\n"
                 "3. Register Regular Patient\n4. View Waiting Queue\n"
                 "5. View All Patients\n6. Search Patient\n7. Add Doctor\n"
                 "8. View Doctors\n9. Call Next Patient\n10. Assign Doctor\n"
                 "11. Start Treatment\n12. Complete Treatment\n"
                 "13. View Treatment History\n14. Hospital Statistics\n"
                 "15. Save Data (also exits; legacy exit choice)\n"
                 "16. Load Data\n0. Exit\n";
}

void registerPatient(Hospital& hospital, int kind) {
    const int id = readInteger("Patient ID: ", 1, std::numeric_limits<int>::max());
    const std::string name = readText("Name: ");
    const int age = readInteger("Age (0-130): ", 0, 130);
    const std::string condition = readText("Condition: ");
    const int severity = readInteger("Severity (1-5): ", 1, 5);
    if (kind == 2) {
        const int critical = readInteger("Critical? (1 yes, 0 no): ", 0, 1);
        hospital.registerEmergencyPatient(id, name, age, condition, severity, critical == 1);
    } else if (kind == 3) {
        hospital.registerRegularPatient(id, name, age, condition, severity);
    } else {
        hospital.registerPatient(std::make_unique<Patient>(id, name, age, condition, severity));
    }
    std::cout << "Patient registered in WAITING status.\n";
}

void showStatistics(const HospitalStatistics& stats) {
    std::cout << "Total patients: " << stats.totalPatients
              << " | Waiting: " << stats.waitingPatients
              << " | Called: " << stats.calledPatients
              << " | Under treatment: " << stats.patientsUnderTreatment
              << " | Completed: " << stats.completedPatients
              << " | Critical: " << stats.criticalPatients
              << " | Average wait: " << stats.averageWaitingTime
              << " min | Average priority: " << stats.averagePriority
              << " | Doctors available: " << stats.availableDoctors
              << " | Busy: " << stats.busyDoctors
              << " | Treatments: " << stats.totalTreatments << '\n';
}
}

int main() {
    Hospital hospital;
    try {
        while (true) {
            printMenu();
            const int choice = readInteger("Choice: ", 0, 16);
            if (choice == 0) break;
            bool exitAfterAction = false;
            try {
                switch (choice) {
                    case 1: case 2: case 3: registerPatient(hospital, choice); break;
                    case 4: hospital.displayWaitingQueue(); break;
                    case 5: hospital.displayAllPatients(); break;
                    case 6: {
                        const int id = readInteger("Patient ID: ", 1, std::numeric_limits<int>::max());
                        Patient* patient = hospital.findPatient(id);
                        if (patient) patient->display(); else std::cout << "Patient not found.\n";
                        break;
                    }
                    case 7: {
                        const int id = readInteger("Doctor ID: ", 1, std::numeric_limits<int>::max());
                        const std::string name = readText("Name: ");
                        const std::string specialization = readText("Specialization: ");
                        hospital.addDoctor(Doctor(id, name, specialization));
                        std::cout << "Doctor added.\n";
                        break;
                    }
                    case 8: hospital.displayDoctors(); break;
                    case 9: {
                        Patient* patient = hospital.callNextPatient();
                        if (patient) { std::cout << "Called patient: "; patient->display(); }
                        else std::cout << "The waiting queue is empty.\n";
                        break;
                    }
                    case 10: {
                        const int patientId = readInteger("Patient ID: ", 1, std::numeric_limits<int>::max());
                        const int doctorId = readInteger("Doctor ID: ", 1, std::numeric_limits<int>::max());
                        hospital.assignDoctor(patientId, doctorId);
                        std::cout << "Doctor assigned; patient is ASSIGNED.\n";
                        break;
                    }
                    case 11:
                        hospital.startTreatment(readInteger("Patient ID: ", 1, std::numeric_limits<int>::max()));
                        std::cout << "Treatment is in progress.\n";
                        break;
                    case 12: {
                        const int patientId = readInteger("Patient ID: ", 1, std::numeric_limits<int>::max());
                        const int doctorId = readInteger("Doctor ID: ", 1, std::numeric_limits<int>::max());
                        const std::string diagnosis = readText("Diagnosis: ");
                        const std::string description = readText("Treatment description: ");
                        hospital.completeTreatment(patientId, doctorId, diagnosis, description);
                        std::cout << "Treatment completed.\n";
                        break;
                    }
                    case 13: hospital.displayTreatmentHistory(); break;
                    case 14: showStatistics(hospital.getStatistics()); break;
                    case 15:
                        hospital.saveData();
                        std::cout << "Data saved. Exiting.\n";
                        exitAfterAction = true;
                        break;
                    case 16:
                        hospital.loadData();
                        std::cout << "Data loaded.\n";
                        break;
                    default: std::cout << "Invalid menu choice.\n"; break;
                }
            } catch (const std::exception& error) {
                std::cout << "Operation failed: " << error.what() << '\n';
            }
            if (exitAfterAction) break;
        }
    } catch (const std::exception& error) {
        if (std::cin.eof()) return 0;
        std::cerr << "Application stopped: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Goodbye.\n";
    return 0;
}
