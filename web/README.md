# Web Frontend — Emergency Patient Queue Management System

The existing HTML/CSS interface is served by the C++ HTTP server. From the project root, start `emergency_hospital_web`; then open `http://localhost:8080`. The frontend calls the server's `/api` routes on the same origin. Opening `index.html` directly or serving it from a separate static server will not connect to the C++ API.

No JavaScript framework or separate frontend build step is required. The C++ `Hospital` classes remain the source of truth for patient, queue, doctor, treatment, statistics, and persistence data.

Staff accounts are created from the login screen. Account records are stored by the C++ `UserManager` in `data/users.txt` using salted PBKDF2-HMAC-SHA-256 password hashes (12,000 iterations); bearer sessions are held in memory by the local HTTP server. All `/api` hospital-data endpoints require a logged-in session. Restarting the server ends active sessions but preserves accounts.
