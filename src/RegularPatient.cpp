#include "RegularPatient.h"
#include <iostream>

void RegularPatient::display() const {
    std::cout << "Regular Patient | ";
    Patient::display();
}
