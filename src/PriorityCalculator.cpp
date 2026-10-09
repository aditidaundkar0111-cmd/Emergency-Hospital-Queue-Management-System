#include "PriorityCalculator.h"
#include "Exceptions.h"

double PriorityCalculator::calculate(int severity, bool critical, int waitingMinutes, int age) {
    if (severity < 1 || severity > 5 || waitingMinutes < 0 || age < 0 || age > 130)
        throw InvalidPatientDataException("Invalid values supplied to priority calculator.");
    // Capping the wait credit prevents waiting alone from overtaking high-severity cases.
    const int creditedWait = waitingMinutes < MAX_WAITING_TIME_CREDIT_MINUTES
        ? waitingMinutes : MAX_WAITING_TIME_CREDIT_MINUTES;
    return severity * SEVERITY_WEIGHT + (critical ? CRITICAL_BONUS : 0.0)
         + creditedWait * WAITING_TIME_WEIGHT
         + (age >= AGE_BONUS_THRESHOLD ? AGE_BONUS : 0.0);
}
