#include "serial.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/watchdog.h"

static char serial_buffer[256];
static char last_command[256];

#define PULSE_DELAY_CYCLES_DEFAULT 0
#define PULSE_TIME_CYCLES_DEFAULT 625 // 5us in 8ns cycles
#define PULSE_TIME_US_DEFAULT 5 // 5us
#define PULSE_POWER_DEFAULT 0.0122
static uint32_t pulse_time;
static uint32_t pulse_delay_cycles;
static uint32_t pulse_time_cycles;
static union float_union {float f; uint32_t ui32;} pulse_power;

void read_line() {
    memset(serial_buffer, 0, sizeof(serial_buffer));
    while(1) {
        int c = getchar();
        if(c == EOF) {
            return;
        }

        putchar(c);

        if(c == '\r') {
            return;
        }
        if(c == '\n') {
            continue;
        }

        // buffer full, just return.
        if(strlen(serial_buffer) >= 255) {
            return;
        }

        serial_buffer[strlen(serial_buffer)] = (char)c;
    }
}

void print_status(uint32_t status) {
    bool armed = (status >> 0) & 1;
    bool charged = (status >> 1) & 1;
    bool timeout_active = (status >> 2) & 1;
    bool hvp_mode = (status >> 3) & 1;
    printf("inter: Status:\n");
    if(armed) {
        printf("inter: - Armed\n");
    } else {
        printf("inter: - Disarmed\n");
    }
    if(charged) {
        printf("inter: - Charged\n");
    } else {
        printf("inter: - Not charged\n");
    }
    if(timeout_active) {
        printf("inter: - Timeout active\n");
    } else {
        printf("inter: - Timeout disabled\n");
    }
    if(hvp_mode) {
        printf("inter: - HVP internal\n");
    } else {
        printf("inter: - HVP external\n");
    }
    printf("result: End of status\n");
}

bool handle_command(char *command) {
    if (command[0] == 0 && last_command[0] != 0) {
        printf("inter: Repeat previous command (%s)\n", last_command);
        return handle_command(last_command);
    } else {
        strcpy(last_command, command);
    }

    if(strcmp(command, "h") == 0 || strcmp(command, "help") == 0)
        return false;

    if(strcmp(command, "a") == 0 || strcmp(command, "arm") == 0) {
        multicore_fifo_push_blocking(cmd_arm);
        uint32_t result = multicore_fifo_pop_blocking();
        if(result == return_ok) {
            printf("result: Device armed!\n");
        } else {
            printf("error: Arming failed!\n");
        }
        return true;
    }
    if(strcmp(command, "d") == 0 || strcmp(command, "disarm") == 0) {
        multicore_fifo_push_blocking(cmd_disarm);
        uint32_t result = multicore_fifo_pop_blocking();
        if(result == return_ok) {
            printf("result: Device disarmed!\n");
        } else {
            printf("error: Disarming failed!\n");
        }
        return true;
    }
    if(strcmp(command, "p") == 0 || strcmp(command, "pulse") == 0) {
        multicore_fifo_push_blocking(cmd_pulse);
        uint32_t result = multicore_fifo_pop_blocking();
        if(result == return_ok) {
            printf("result: Pulsed!\n");
        } else {
            printf("error: Pulse failed!\n");
        }
        return true;
    }
    if(strcmp(command, "ch") == 0 || strcmp(command, "charge") == 0) {
        multicore_fifo_push_blocking(cmd_status);
        uint32_t result = multicore_fifo_pop_blocking();
        if(result == return_ok) {
            bool charged = (multicore_fifo_pop_blocking() >> 1) & 1;
            if(charged) {
                printf("result: charged\n");
            } else {
                printf("result: not charged\n");
            }
        } else {
            printf("error: Getting charge status failed!\n");
        }
        return true;
    }
    if(strcmp(command, "s") == 0 || strcmp(command, "status") == 0) {
        multicore_fifo_push_blocking(cmd_status);
        uint32_t result = multicore_fifo_pop_blocking();
        if(result == return_ok) {
            print_status(multicore_fifo_pop_blocking());
        } else {
            printf("error: Getting status failed!\n");
        }
        return true;
    }
    if(strcmp(command, "en") == 0 || strcmp(command, "enable_timeout") == 0) {
        multicore_fifo_push_blocking(cmd_enable_timeout);
        uint32_t result = multicore_fifo_pop_blocking();
        if(result == return_ok) {
            printf("result: Timeout enabled!\n");
        } else {
            printf("error: Enabling timeout failed!\n");
        }
        return true;
    }
    if(strcmp(command, "di") == 0 || strcmp(command, "disable_timeout") == 0) {
        multicore_fifo_push_blocking(cmd_disable_timeout);
        uint32_t result = multicore_fifo_pop_blocking();
        if(result == return_ok) {
            printf("result: Timeout disabled!\n");
        } else {
            printf("error: Disabling timeout failed!\n");
        }
        return true;
    }
    if(strcmp(command, "f") == 0 || strcmp(command, "fast_trigger") == 0) {
        multicore_fifo_push_blocking(cmd_fast_trigger);
        uint32_t result = multicore_fifo_pop_blocking();
        if(result == return_ok) {
            printf("result: Fast trigger active...\n");
            multicore_fifo_pop_blocking();
            printf("inter: Triggered!\n");
        } else {
            printf("error: Setting up fast trigger failed.\n");
        }
        return true;
    }
    if(strcmp(command, "fa") == 0 || strcmp(command, "fast_trigger_configure") == 0) {
        char **unused;
        printf("inter: configure in cycles\n");
        printf("inter: 1 cycle = 8ns\n");
        printf("inter: 1us = 125 cycles\n");
        printf("inter: 1ms = 125000 cycles\n");
        printf("inter: max = MAX_UINT32 = 4294967295 cycles = 34359ms\n");

        printf("inter: pulse_delay_cycles (current: %d, default: %d)?> \n", pulse_delay_cycles, PULSE_DELAY_CYCLES_DEFAULT);
        read_line();
        printf("inter: \n");
        if (serial_buffer[0] == 0)
            printf("inter: Using default\n");
        else
            pulse_delay_cycles = strtoul(serial_buffer, unused, 10);

        printf("inter: pulse_time_cycles (current: %d, default: %d)?> \n", pulse_time_cycles, PULSE_TIME_CYCLES_DEFAULT);
        read_line();
        printf("\n");
        if (serial_buffer[0] == 0)
            printf("inter: Using default\n");
        else
            pulse_time_cycles = strtoul(serial_buffer, unused, 10);

        multicore_fifo_push_blocking(cmd_config_pulse_delay_cycles);
        multicore_fifo_push_blocking(pulse_delay_cycles);
        uint32_t result = multicore_fifo_pop_blocking();
        if(result != return_ok) {
            printf("inter: Config pulse_delay_cycles failed.\n");
        }

        multicore_fifo_push_blocking(cmd_config_pulse_time_cycles);
        multicore_fifo_push_blocking(pulse_time_cycles);
        result = multicore_fifo_pop_blocking();
        if(result != return_ok) {
            printf("inter: Config pulse_time_cycles failed.\n");
        }

        printf("result: pulse_delay_cycles=%d, pulse_time_cycles=%d\n", pulse_delay_cycles, pulse_time_cycles);

        return true;
    }
    if(strcmp(command, "in") == 0 || strcmp(command, "internal_hvp") == 0) {
        multicore_fifo_push_blocking(cmd_internal_hvp);
        uint32_t result = multicore_fifo_pop_blocking();
        if(result == return_ok) {
            printf("result: Internal HVP mode active!\n");
        } else {
            printf("error: Setting up internal HVP mode failed.\n");
        }
        return true;
    }
    if(strcmp(command, "ex") == 0 || strcmp(command, "external_hvp") == 0) {
        multicore_fifo_push_blocking(cmd_external_hvp);
        uint32_t result = multicore_fifo_pop_blocking();
        if(result == return_ok) {
            printf("result: External HVP mode active!\n");
        } else {
            printf("error: Setting up external HVP mode failed.\n");
        }
        return true;
    }

    if(strcmp(command, "c") == 0 || strcmp(command, "configure") == 0) {
        char **unused;
        printf("inter: pulse_time (current: %d, default: %d)?> \n", pulse_time, PULSE_TIME_US_DEFAULT);
        read_line();
        printf("inter: \n");
        if (serial_buffer[0] == 0)
            printf("inter: Using default\n");
        else
            pulse_time = strtoul(serial_buffer, unused, 10);

        printf("inter: pulse_power (current: %f, default: %f)?> \n", pulse_power.f, PULSE_POWER_DEFAULT);
        read_line();
        printf("inter: \n");
        if (serial_buffer[0] == 0)
            printf("inter: Using default\n");
        else
            pulse_power.f = strtof(serial_buffer, unused);

        multicore_fifo_push_blocking(cmd_config_pulse_time);
        multicore_fifo_push_blocking(pulse_time);
        uint32_t result = multicore_fifo_pop_blocking();
        if(result != return_ok) {
            printf("inter: Config pulse_time failed.\n");
        }

        multicore_fifo_push_blocking(cmd_config_pulse_power);
        multicore_fifo_push_blocking(pulse_power.ui32);
        result = multicore_fifo_pop_blocking();
        if(result != return_ok) {
            printf("inter: Config pulse_power failed.\n");
        }

        printf("result: pulse_time=%d, pulse_power=%f\n", pulse_time, pulse_power.f);

        return true;
    }

    if(strcmp(command, "t") == 0 || strcmp(command, "toggle_gp1") == 0) {
        multicore_fifo_push_blocking(cmd_toggle_gp1);

        uint32_t result = multicore_fifo_pop_blocking();
        if(result != return_ok) {
            printf("error: target_reset failed.\n");
        } else {
            printf("result: success\n");
        }

        return true;
    }

    if(strcmp(command, "r") == 0 || strcmp(command, "reset") == 0) {
        watchdog_enable(1, 1);
        while(1);
    }

    return false;
}

void serial_console() {
    multicore_fifo_drain();

    memset(last_command, 0, sizeof(last_command));

    pulse_time = PULSE_TIME_US_DEFAULT;
    pulse_power.f = PULSE_POWER_DEFAULT;
    pulse_delay_cycles = PULSE_DELAY_CYCLES_DEFAULT;
    pulse_time_cycles = PULSE_TIME_CYCLES_DEFAULT;

    while(1) {
        read_line();
        printf("inter:\n");
        if(!handle_command(serial_buffer)) {
            printf("result: PicoEMP Commands:\n");
            printf("inter: - <empty to repeat last command>\n");
            printf("inter: - [h]elp\n");
            printf("inter: - [a]rm\n");
            printf("inter: - [d]isarm\n");
            printf("inter: - [p]ulse\n");
            printf("inter: - [en]able_timeout\n");
            printf("inter: - [di]sable_timeout\n");
            printf("inter: - [f]ast_trigger\n");
            printf("inter: - [fa]st_trigger_configure: delay_cycles=%d, time_cycles=%d\n", pulse_delay_cycles, pulse_time_cycles);
            printf("inter: - [in]ternal_hvp\n");
            printf("inter: - [ex]ternal_hvp\n");
            printf("inter: - [c]onfigure: pulse_time=%d, pulse_power=%f\n", pulse_time, pulse_power.f);
            printf("inter: - [t]oggle_gp1\n");
            printf("inter: - [s]tatus\n");
            printf("inter: - [r]eset\n");
        }

        if (last_command[0] != 0) {
            printf("inter: [%s] > \n", last_command);
        } else {
            printf("inter: > \n");
        }
    }
}
