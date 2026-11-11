# Emergency Hospital Patient Queue Management System

## Overview

An academic C++17 project demonstrating hospital patient queue management through a console application and a local HTTP/JSON web application. The priority score is illustrative and is **not a medically validated triage tool**.

## Technologies

- C++17 (console application and HTTP backend)
- cpp-httplib 0.5.0 and nlohmann/json 3.11.3 (vendored headers in `third_party/`)
- HTML, CSS, and vanilla JavaScript frontend
- Plain text files for patient, doctor, treatment, and local account persistence
- CMake or a direct MinGW/GCC build

## OOP Concepts

- Encapsulation through private patient and doctor state
- Inheritance: `Patient` derives from `Person`; emergency and regular patient classes derive from `Patient`
- Runtime polymorphism for display and priority calculations
- Composition: `Hospital` coordinates queue, doctor, treatment, and statistics managers
- RAII ownership of patients with `std::unique_ptr`; the priority queue holds non-owning pointers

## Features

- Patient registration and emergency-priority queue ordering
- Lifecycle: `WAITING → CALLED → ASSIGNED → UNDER_TREATMENT → COMPLETED`
- Doctor assignment and availability tracking
- Treatment records and operational statistics
- File persistence with malformed/incomplete dataset rejection before replacing active state
- Local account registration/login and bearer-session-protected hospital API routes
- Responsive browser interface served by the C++ server

## Project Structure

```text
.
├── CMakeLists.txt
├── README.md
├── main.cpp                  # Console entry point
├── web_server_main.cpp       # HTTP server entry point
├── include/                  # Class declarations
├── src/                      # Core class implementations
├── web_server/WebServer.cpp  # HTTP API and static frontend hosting
├── web/                      # HTML, CSS, JavaScript, frontend notes
├── third_party/              # Vendored HTTP and JSON headers/licenses
├── tests/                    # C++ tests and console smoke script
└── data/                     # Existing local/sample patient, doctor, treatment files
```

## Data and Git Safety

`data/patients.txt`, `data/doctors.txt`, and `data/treatments.txt` are currently tracked by Git and ignored for future untracked changes. Git history identifies them as the project's sample hospital data; the files are preserved. Runtime saves currently write to those same paths, so later application use can change sample records. `data/users.txt` exists locally, is ignored, and is not tracked. Never add account files, hashes, or credentials to Git.

The application reads and writes under `data/` relative to its current working directory. To separate runtime state from sample data safely, preserve the tracked files as a read-only sample set (for example, copy them to a clearly named `sample-data/` directory) and run the application with a separate writable working/data directory. Do not repurpose or overwrite the existing records when making this separation.

All three hospital files are required together when loading a saved dataset. If none exist and the hospital is empty, startup begins with an empty state; if a populated hospital has no saved files, only some files exist, or any record is malformed/inconsistent, loading reports an error and keeps the current in-memory state. Patient records retain their existing columns and append an optional waiting-start Unix timestamp. Older patient records without that field remain supported. For a `WAITING` patient, elapsed whole minutes are calculated from the timestamp; on leaving `WAITING`, the elapsed minutes are frozen in the existing wait field. Thus waiting time survives browser refreshes and application restarts without requiring a new data file. Account records contain salted password hashes; sessions are in memory and end when the server stops.

## Build

### CMake (recommended)

From the project root:

```sh
cmake -S . -B build
cmake --build build
```

On Windows, use an installed CMake generator compatible with MinGW. The HTTP target links Winsock. The API test target is included only when the compiler supports `std::thread`.

### Direct MinGW/GCC console build

```sh
g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude src/*.cpp main.cpp -o emergency_hospital.exe
```

### Direct MinGW/GCC web build

```sh
g++ -std=c++17 -Wall -Wextra -Wpedantic -D_WIN32_WINNT=0x0A00 -DCPPHTTPLIB_THREAD_POOL_COUNT=-1 -Iinclude -Ithird_party src/*.cpp web_server/WebServer.cpp web_server_main.cpp -lws2_32 -o emergency_hospital_web.exe
```

`CPPHTTPLIB_THREAD_POOL_COUNT=-1` selects synchronous handling for compatibility with the documented MinGW.org GCC 6.3 environment. That server handles one request at a time.

## Run

Run each executable from the project root so the default `data/` paths resolve correctly:

```sh
./emergency_hospital.exe
```

For the web app, start:

```sh
./emergency_hospital_web.exe
```

Then open [http://localhost:8080](http://localhost:8080). Create a local account using **Register**, then sign in with that account. There is no default account. “Remember Me” remembers the username only; it does not persist the login session. “Forgot password?” displays administrator guidance; the application has no password reset mechanism.

## Tests and Checks

Configure and build the test targets, then run them from the build directory:

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

The C++ regression and HTTP API tests use the build directory as their working directory so their generated `data/` records do not touch the repository’s existing sample/runtime files. The API test is omitted when `std::thread` is unavailable. The console smoke script uses the current directory’s `data/` files; run it only from an isolated working copy with a compiled executable:

```powershell
./tests/menu_smoke.ps1 -Executable ./build/emergency_hospital.exe
```

JavaScript syntax can be checked with `node --check web/app.js`. The project does not include automated browser visual tests.

## API and Authentication

The local API supports registration/login/logout/current-user, dashboard, patients, queue, doctors, treatment lifecycle, treatment history, and save/load routes. Hospital data routes require a bearer session. CORS is limited to the local UI origins `http://localhost:8080` and `http://127.0.0.1:8080`; normal same-origin use does not require CORS.

This authentication is for an academic local demonstration, not production identity management. Password hashing is implemented in project code, tokens are held in server memory, sessions have no configured expiry, and the server uses HTTP on loopback. Do not expose it to a network or use real patient data.

## Limitations

- Priority weights are educational and not clinically validated.
- Waiting time is measured in whole elapsed minutes while a patient is `WAITING`; it freezes when the patient is called. The educational priority score is recalculated from that current wait value when the queue is read/rebuilt.
- Persistence is text-file based and has no database transactions or concurrent-write protection.
- The web server processes requests synchronously in the documented compatibility configuration.
- Password reset, account recovery, and persistent login are not implemented.
