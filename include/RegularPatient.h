#pragma once

#include "Patient.h"

class RegularPatient : public Patient {
public:
    using Patient::Patient;
    void display() const override;
};
