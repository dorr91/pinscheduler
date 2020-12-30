// activation_schedule.h
// Activate a particular pin on a given Cron schedule
// Read config from disk or from a specified URL
#include <map>

#include <ArduinoJson.h>

#include "CronAlarms.h"

struct PinSchedule {
    int pin;
    // Duty cycle for this pin. Power on for total_on_sec in intervals of
    // on_sec on / off_sec off.
    int total_on_sec;
    int on_sec;
    int off_sec;

    // Schedule on which to run the activation cycle.
    char *cron_str;
};

class ActivationSchedule {
    public:
        // Activate `pin` now with the specified duty cycle, e.g. power on for
        // `on_sec` seconds at a time, waiting `off_sec` between activations,
        // until it has been active for `total_on_sec`.
        static void Activate(const int pin, const int total_on_sec,
                const int on_sec, const int off_sec);
        static void Activate(const PinSchedule& schedule);

        // Schedule the activation using Cron
        void Schedule(PinSchedule schedule);

        bool ParseConfig(StaticJsonDocument<1024> config);
        // Read config from a file. Replaces all currently scheduled
        // PinSchedules.
        bool ReadConfig(char* filename);

        // map of CronId to pin schedules which are being managed by this object
        std::map<CronId, PinSchedule> pin_schedules_;
};

extern ActivationSchedule PinScheduler;
// CronAlarms callback that activates the pin for the current alarm
void ActivatePin();

