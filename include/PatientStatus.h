#pragma once

// Academic simulation states; these do not represent a clinical workflow.
enum class PatientStatus {
    WAITING,
    CALLED,
    UNDER_TREATMENT,
    COMPLETED
};

const char* toString(PatientStatus status);
