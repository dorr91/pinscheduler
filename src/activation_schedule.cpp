#include "activation_schedule.h"

#include <ArduinoJson.h>

#include "CronAlarms.h"

ActivationSchedule PinScheduler;

// CronAlarms callback to activate the pin for the active alarm
void ActivatePin() {
    CronId current_alarm = Cron.getTriggeredCronId();
    Serial.print("Triggered CronId: ");
    Serial.println(current_alarm);

    PinSchedule pin_sched = PinScheduler.pin_schedules_[current_alarm];
    Serial.print("Activating pin #");
    Serial.println(pin_sched.pin);

    ActivationSchedule::Activate(pin_sched);
}

void ActivationSchedule::Activate(const int pin, const int total_on_sec,
        const int on_sec, const int off_sec) {
    if (total_on_sec < 0 || on_sec < 0 || off_sec < 0) {
        Serial.println("Invalid argument to ActivateInterval: ");
        char err_buf[128];
        snprintf(err_buf, sizeof(err_buf), "total_on_sec: %d; on_sec: %d; "
                "off_sec: %d", total_on_sec, on_sec, off_sec);
        Serial.println(err_buf);
        return;
    }

    char plan_buf[128];
    snprintf(plan_buf, sizeof(plan_buf), "Plan: Power pin %d for %ds total in cycles of "
           "%ds on / %ds off", pin, total_on_sec, on_sec, off_sec);
    Serial.println(plan_buf);
    int remaining_on_sec = total_on_sec;
    while (remaining_on_sec > 0) {
        int cycle_on_sec = std::min(remaining_on_sec, on_sec);
        digitalWrite(pin, HIGH);
        delay(cycle_on_sec * 1000);
        digitalWrite(pin, LOW);
        remaining_on_sec -= cycle_on_sec;
        if (remaining_on_sec > 0) {
            delay(off_sec * 1000);
        }
    }
}

void ActivationSchedule::Activate(const PinSchedule& schedule) {
    ActivationSchedule::Activate(schedule.pin, schedule.total_on_sec,
            schedule.on_sec, schedule.off_sec);
}

void ActivationSchedule::Schedule(PinSchedule schedule) {
    // TODO validate schedule
    CronId id = Cron.create(schedule.cron_str, ActivatePin, /*isOneShot=*/false);
    // TODO hypothetical race condition where the run is triggered but the map doesn't?
    // I don't think so because Cron only fires during Cron.delay
    pin_schedules_[id] = schedule;
}

bool ActivationSchedule::ParseConfig(StaticJsonDocument<1024> config) {
    int num_pin_configs = config["pin_configs"].size();
    PinSchedule pin_schedules[num_pin_configs];

    int i = 0;
    JsonArray pin_configs = config["pin_configs"];
    for (const auto& pin_config : pin_configs) {
        // TODO avoid dynamic allocation since it can lead to memory
        // fragmentation over time
        const char *src_cron_str = pin_config["cron_str"];
        if (src_cron_str == nullptr) {
            Serial.print("Found a null cron str in pin config #");
            Serial.println(i);
            continue;
        }
        char *cron_str = strdup(src_cron_str);

        PinSchedule sched = {
            .pin = pin_config["pin"], .total_on_sec = pin_config["total_on_sec"],
            .on_sec = pin_config["on_sec"], .off_sec = pin_config["off_sec"],
            .cron_str = cron_str,
        };
        if (sched.pin < 0 || sched.total_on_sec < 0 || sched.on_sec < 0
                || sched.off_sec < 0 || strlen(sched.cron_str) == 0) {
            Serial.print("Couldn't parse JSON config: invalid pin schedule at #");
            Serial.println(i);
            return false;
        }
        pin_schedules[i++] = sched;
    }
    for (const auto& schedule : pin_schedules) {
        Schedule(schedule);
    }
    return true;
}
