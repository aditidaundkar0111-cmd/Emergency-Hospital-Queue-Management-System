# Emergency Hospital Patient Queue Optimizer

## 1. Project Overview

This is a C++17 console application and HTTP API that demonstrates object-oriented programming by managing a simulated hospital waiting queue, doctors, treatment records, and saved data. The existing console executable remains available.

**This project is an academic simulation and is not a medically validated triage system.** Its score is an educational example and must not be used to make real medical decisions.

## 2. Problem Statement

A first-come-first-served queue does not demonstrate how differing urgency can affect service order. This project models a transparent, configurable priority queue while tracking patient and doctor states through a simple lifecycle.

## 3. Objectives

- Register regular, emergency, and base patient records.
- Order waiting patients by an explainable score.
- Demonstrate inheritance, virtual functions, encapsulation, and composition.
- Manage doctor availability and treatment completion.
- Calculate hospital statistics from current records.
- Save and reload patients, doctors, and treatments using text files.

## 4. Features

- Priority queue ordering with deterministic ID tie-breaking.
- Patient lifecycle: `WAITING → CALLED → UNDER_TREATMENT → COMPLETED`.
- Doctor lookup, specialization lookup, assignment, and release.
- Treatment history and current-state statistics.
- Text persistence in `data/patients.txt`, `data/doctors.txt`, and `data/treatments.txt`.
- Menu input validation and graceful handling of missing or malformed records.
- Local staff registration and login for the web interface, with salted PBKDF2-HMAC-SHA-256 password hashes and in-memory bearer sessions.

## 5. OOP Concepts Used

- **Encapsulation:** patient and doctor state is stored in private fields.
- **Inheritance:** `Patient` derives from `Person`; `EmergencyPatient` and `RegularPatient` derive from `Patient`.
- **Runtime polymorphism:** patient display and priority calculation are virtual.
- **Composition:** `Hospital` coordinates the patient queue, doctor manager, treatment manager, and statistics manager.
- **RAII:** `Hospital` owns patients with `std::unique_ptr`.
- **Exceptions:** invalid records, IDs, state transitions, and file errors produce useful errors.

## 6. Data Structures Used

- `std::priority_queue<Patient*, ...>` for the waiting queue. Its pointers are non-owning; the `Hospital` owns each patient.
- `std::vector<Doctor>` in `DoctorManager`.
- `std::vector<Treatment>` in `TreatmentManager`.
- `std::vector<std::unique_ptr<Patient>>` in `Hospital` for stable patient object addresses.

## 7. Priority Calculation

The educational score is:

```text
severity × 20
+ 15 for a critical EmergencyPatient
+ min(waiting minutes, 60) × 0.5
+ 5 for age 65 or older
```

Severity has a large contribution. Waiting credit is capped at 60 minutes, so unlimited waiting cannot accumulate unlimited points. The critical bonus and age bonus are explicit. These weights are illustrative; they have not been medically validated.

## 8. Class Architecture

- `Person`: shared ID and name.
- `Patient`: age, condition, severity, waiting time, status, and base priority behavior.
- `EmergencyPatient`: critical flag and emergency priority bonus.
- `RegularPatient`: regular patient display behavior.
- `PriorityCalculator`: shared score weights and calculation.
- `PatientQueue`: priority ordering and patient lookup, including patients already called.
- `Doctor` / `DoctorManager`: doctor details, availability, assignments, and specialization lookup.
- `Treatment` / `TreatmentManager`: treatment details and history.
- `StatisticsManager`: derives totals and averages from current records.
- `Hospital`: owns patients and coordinates the workflow.
- `FileManager`: serializes and validates patient, doctor, and treatment records.

## 9. Project Structure

```text
EmergencyHospitalQueueOptimizer/
├── CMakeLists.txt
├── README.md
├── main.cpp
├── web_server_main.cpp       # HTTP server entry point
├── web_server/               # API routes and JSON serialization
├── third_party/              # Vendored header-only HTTP/JSON dependencies
├── web/                      # Frontend files (index.html, styles.css, app.js)
├── include/                 # Class declarations
├── src/                     # Class implementations
├── data/
│   ├── patients.txt
│   ├── doctors.txt
│   └── treatments.txt
└── tests/
    ├── menu_smoke.ps1
    └── test_main.cpp
```

## 10. How to Compile

Direct GCC build from the project directory:

```sh
g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude src/*.cpp main.cpp -o emergency_hospital
```

CMake build:

```sh
cmake -S . -B build
cmake --build build
```

The HTTP server target is `emergency_hospital_web`; it listens on `http://localhost:8080` and serves the `web/` directory from the project working directory. Its compatibility build uses cpp-httplib 0.5.0 with synchronous request handling and nlohmann/json 3.11.3. It binds to loopback and handles one request at a time. On Windows the web target links Winsock. The console target does not compile or link the HTTP server.

Direct web build with the existing MinGW.org GCC 6.3 toolchain:

```sh
g++ -std=c++17 -Wall -Wextra -Wpedantic -D_WIN32_WINNT=0x0A00 -DCPPHTTPLIB_THREAD_POOL_COUNT=-1 -Iinclude -Ithird_party src/*.cpp web_server/WebServer.cpp web_server_main.cpp -lws2_32 -o emergency_hospital_web.exe
```

The `-1` thread-pool setting selects cpp-httplib's synchronous task queue for the GCC 6 MinGW runtime, which does not provide `std::thread`. The C++ web API test harness uses `std::thread`, so CMake omits that harness when the compiler cannot compile `std::thread`; the Phase 1–4 regression suite remains enabled.

The API routes are `POST /api/auth/register`, `POST /api/auth/login`, `POST /api/auth/logout`, `GET /api/auth/me`, `GET /api/dashboard`, `GET|POST /api/patients`, `GET /api/patients/{id}`, `GET /api/queue`, `POST /api/queue/call-next`, `GET|POST /api/doctors`, `POST /api/patients/{id}/assign-doctor`, `POST /api/patients/{id}/start-treatment`, `POST /api/patients/{id}/complete-treatment`, `GET /api/treatments`, and `POST /api/data/save` or `/api/data/load`. Every hospital data route requires a valid bearer token; auth routes remain available before sign-in. Mutations use the existing `Hospital` methods, and responses use JSON. CORS is enabled for browser clients. If the `web/` directory is absent, the API remains available and `/` returns a descriptive 503 response.

New local accounts are stored in `data/users.txt`. The file contains hex-encoded account names, a per-account random salt, and PBKDF2-HMAC-SHA-256 output (12,000 iterations); it does not contain plaintext passwords. Sessions are held in server memory and browser `sessionStorage`, so users sign in again after the server restarts or browser session ends. This simple local authentication is for the academic demonstration and is not a production identity system.

## 11. How to Run

Run `./emergency_hospital` (Windows: `emergency_hospital.exe`) from the project directory so the default `data/` folder is used. Menu option 0 exits; option 15 saves and exits (preserving its earlier exit shortcut); option 16 reloads.

## 12. How Persistence Works

Records are one per line. Each starts with a record tag (`P`, `D`, or `T`); text values use C++ quoted-string escaping, so spaces and quote characters are supported. The format is whitespace-separated, and raw pointers are never stored.

- Patients store ID, type, age, severity, waiting time, critical flag, status, name, and condition.
- Doctors store ID, availability, current patient ID, name, and specialization.
- Treatments store patient ID, doctor ID, completion flag, diagnosis, and description.

Saving creates the data directory if needed. Missing files are treated as empty records. Malformed records and invalid references are reported and skipped. Loading reconstructs patients once under `Hospital` ownership, rebuilds the queue from `WAITING` statuses using the existing priority calculation, restores valid busy-doctor assignments, and then loads valid treatment records. A complete load is assembled separately before replacing the current hospital state.

## 13. Testing

The C++ regression suite covers model validation, priority ordering, lifecycle and doctor rules, treatment records, save/load round trips, queue rebuilding, statistics, duplicate IDs, malformed records, missing files, and local credential hashing/session behavior.

```sh
g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude src/*.cpp tests/test_main.cpp -o hospital_tests
./hospital_tests
```

With CMake, run `ctest --test-dir build --output-on-failure`. On Windows PowerShell, after building the app, run `./tests/menu_smoke.ps1 -Executable ./build/emergency_hospital.exe` (adjust the path for the selected generator) to exercise menu input, save, reload, and exit.

The HTTP API suite is `emergency_hospital_web_tests`; it exercises JSON routes, queue ordering, errors, CORS, doctor assignment, treatment lifecycle, and save/load. Run both suites with CTest after configuring and building with a supported toolchain.

## 14. Limitations

- Patient waiting time is a static value supplied at registration; there are no real-time arrival timestamps.
- The priority score is an academic demonstration, not a clinical tool.
- Persistence is simple local text storage without concurrent access protection or database transactions.
- Returned `Doctor*` pointers are transient because `DoctorManager` stores doctors in a vector; do not retain them across doctor additions.
- The local authentication/session implementation is an academic demonstration. Sessions are in-memory, and passwords use a self-contained implementation to avoid adding crypto dependencies to the GCC 6.3 build. Do not expose the HTTP service beyond the local machine.

## 15. Future Enhancements

Possible future work includes a GUI, a database, real-time timestamps, maps or location support, hospital networking, and analytics. These are not implemented here.
