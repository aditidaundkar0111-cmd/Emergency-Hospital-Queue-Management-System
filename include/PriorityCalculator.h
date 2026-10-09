#pragma once

class PriorityCalculator {
public:
    // Transparent educational weights. Severity outweighs a long wait.
    static constexpr double SEVERITY_WEIGHT = 20.0;
    static constexpr double CRITICAL_BONUS = 15.0;
    static constexpr double WAITING_TIME_WEIGHT = 0.5;
    static constexpr int MAX_WAITING_TIME_CREDIT_MINUTES = 60;
    static constexpr double AGE_BONUS_THRESHOLD = 65.0;
    static constexpr double AGE_BONUS = 5.0;

    static double calculate(int severity, bool critical, int waitingMinutes, int age);
};
