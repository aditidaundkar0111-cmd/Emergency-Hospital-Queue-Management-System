#include "Person.h"
#include "Exceptions.h"
#include <iostream>
#include <algorithm>
#include <cctype>

bool isBlankText(const std::string& value) {
    return value.empty() || std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
}

Person::Person(int id, const std::string& name) : id_(id), name_(name) {
    if (id <= 0) throw InvalidPatientDataException("Person ID must be positive.");
    if (isBlankText(name)) throw InvalidPatientDataException("Name cannot be empty.");
}

int Person::getId() const { return id_; }
const std::string& Person::getName() const { return name_; }

void Person::display() const {
    std::cout << "ID: " << id_ << " | Name: " << name_;
}
