#pragma once

#include <string>

bool isBlankText(const std::string& value);

class Person {
public:
    Person(int id, const std::string& name);
    virtual ~Person() = default;

    int getId() const;
    const std::string& getName() const;
    virtual void display() const;

private:
    int id_;
    std::string name_;
};
