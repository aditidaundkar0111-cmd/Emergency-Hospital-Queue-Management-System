#pragma once

#include <string>

class Hospital;

class FileManager {
public:
    explicit FileManager(const std::string& directory = "data");

    void savePatients(const Hospital& hospital) const;
    void loadPatients(Hospital& hospital) const;
    void saveDoctors(const Hospital& hospital) const;
    void loadDoctors(Hospital& hospital) const;
    void saveTreatments(const Hospital& hospital) const;
    void loadTreatments(Hospital& hospital) const;
    void saveAll(const Hospital& hospital) const;
    void loadAll(Hospital& hospital) const;

private:
    std::string directory_;
    std::string filePath(const std::string& fileName) const;
    void ensureDirectory() const;
    void loadPatients(Hospital& hospital, bool& legacyWaitingTimestampFound) const;
};
