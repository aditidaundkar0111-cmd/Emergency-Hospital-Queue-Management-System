#pragma once

#include <stdexcept>
#include <string>

class InvalidPatientDataException : public std::invalid_argument {
public:
    explicit InvalidPatientDataException(const std::string& message)
        : std::invalid_argument(message) {}
};
