#pragma once

#include "Hospital.h"
#include "UserManager.h"
#include "httplib.h"
#include <string>

class WebServer {
public:
    explicit WebServer(Hospital& hospital, const std::string& webDirectory = "web",
                       const std::string& userFile = "data/users.txt");
    bool listen(const std::string& host = "127.0.0.1", int port = 8080);
    void stop();

private:
    Hospital& hospital_;
    UserManager userManager_;
    httplib::Server server_;

    void registerRoutes();
};
