#include "Hospital.h"
#include "WebServer.h"
#include <iostream>

int main() {
    Hospital hospital;
    hospital.loadData();
    WebServer server(hospital);
    std::cout << "Emergency Hospital API listening on http://localhost:8080\n";
    std::cout << "Server uses single-threaded request handling on the local machine.\n";
    std::cout << "Press Ctrl+C to stop the server.\n";
    if (!server.listen("127.0.0.1", 8080)) {
        std::cerr << "Could not start the web server on port 8080.\n";
        return 1;
    }
    return 0;
}
