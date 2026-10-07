#pragma once

#include <hardware/gpio.h>
#include <hardware/pwm.h>
#include <hardware/structs/clocks.h>
#include <hardware/structs/io_bank0.h>
#include <hardware/timer.h>
#include <hardware/clocks.h>
#include <pico/platform/compiler.h>
#include <pico/stdlib.h>
#include <pico/time.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "types/arrays.c"
#include "./nec.c"


#ifndef AC_MAX
#define AC_MAX 86
#endif

#ifndef AC_MIN
#define AC_MIN 60
#endif


typedef enum {
    AC_SAVER = 0,
    AC_COOL = 1,
    AC_FAN = 2,
    AC_DRY = 3,
    MODES_COUNT = 4,
} ac_mode;


static const nec_cmd ac_cmd_up = {
    .address1 = 0x81,
    .address2 = 0x66,
    .command = 0x85
};
static const nec_cmd ac_cmd_down = {
    .address1 = 0x81,
    .address2 = 0x66,
    .command = 0x8A
};
static const nec_cmd ac_cmd_mode = {
    .address1 = 0x81,
    .address2 = 0x66,
    .command = 0x9B
};
static const nec_cmd ac_cmd_pwr = {
    .address1 = 0x81,
    .address2 = 0x66,
    .command = 0x81
};

void transmit_ir(array_uint* timing_info) {
    const uint PIN = 22;
    uint CLOCK = clock_get_hz(clk_sys);
    const uint DUTY_CYCLE_SLICES = 3;
    const uint WRAP_PLUS_ONE = DUTY_CYCLE_SLICES * 20;
    const uint DUTY_ON = WRAP_PLUS_ONE / 3;
    const uint freq = 38000;
    gpio_set_function(PIN, GPIO_FUNC_PWM);

    uint pin_slice = pwm_gpio_to_slice_num(PIN);
    uint pin_chan = pwm_gpio_to_channel(PIN);
    uint divider = CLOCK / freq / WRAP_PLUS_ONE + (CLOCK % (freq * WRAP_PLUS_ONE) != 0);
    uint freq_recalc = CLOCK / (divider * (WRAP_PLUS_ONE));
    printf("Initializing PWM slice %d, channel %d, and clkdiv %d, leading to an effective freq of %d\n", pin_slice, pin_chan, divider, freq_recalc);

    pwm_set_clkdiv_int_frac(pin_slice, divider, 0);
    
    // 33% duty cycle -> count from 0 to 2
    pwm_set_wrap(pin_slice, WRAP_PLUS_ONE - 1);
    pwm_set_chan_level(pin_slice, pin_chan, 0);
    pwm_set_enabled(pin_slice, true);

    bool curr_state = true; // start high
    for (int i = 0; i < timing_info->length; i++) {
        pwm_set_chan_level(pin_slice, pin_chan, curr_state * DUTY_ON);

        uint element = get_uint(timing_info, i);
        busy_wait_us(element);

        curr_state = !curr_state;
    }
    pwm_set_chan_level(pin_slice, pin_chan, 0);

    busy_wait_ms(40); // so things can 

    printf("done!\n");
}

void transmit_nec(const nec_cmd cmd) {
    array_uint nec_ir = nec_build_timings(cmd);

    transmit_ir(&nec_ir);
    
    free(nec_ir.data);
}

void ir_test_raw() {
    // float us_per_s = 1000000;
    // float freq_hz = 38000;
    // float pulse_len_us = us_per_s / freq_hz;
    // float duty_cyle = 0.33;
    // float cycle_on = 

    array_uint timing_info = {
        .length = 15,
        .data = malloc(sizeof(uint) * 15), 
    };

    printf("array struct holds %d elements\n", timing_info.length);

    set_uint(&timing_info, 0, 1989);
    set_uint(&timing_info, 1, 27513);
    set_uint(&timing_info, 2, 408);
    set_uint(&timing_info, 3, 1569);
    set_uint(&timing_info, 4, 386);
    set_uint(&timing_info, 5, 3503);
    set_uint(&timing_info, 6, 408);
    set_uint(&timing_info, 7, 62013);
    set_uint(&timing_info, 8, 1985);
    set_uint(&timing_info, 9, 27517);
    set_uint(&timing_info, 10, 404);
    set_uint(&timing_info, 11, 1573);
    set_uint(&timing_info, 12, 382);
    set_uint(&timing_info, 13, 3506);
    set_uint(&timing_info, 14, 405);

    printf("Set timing info!\n");
    transmit_ir(&timing_info);
}

void ir_test_nec() {
    // name: Ac_pwr
    // address: 81 66 00 00
    // command: 81 7E 00 00
    // (the 7E is just the inverse of 81, we're fine.)

    nec_cmd cmd = {
        .address1 = 0x81,
        .address2 = 0x66,
        .command = 0x81,
    };

    // nec_print_bit_train(cmd);
    printf("building timings...");
    array_uint timings = nec_build_timings(cmd);
    printf("transmitting...");
    transmit_ir(&timings);
    free(timings.data);
    printf("done!");
}

void ir_test() {
    // ir_test_raw();
    ir_test_nec();
}


static int8_t set_temp = AC_MAX;
static ac_mode set_mode = AC_SAVER;

// This is gonna be used from HomeAssistant. Automations can run timers already.
// static int8_t set_timer = 0;


// Fan speed can't be reset because it has a saved memory that persists across
// poweroffs and mode switches, so don't bother tracking it
// static int8_t fan_speed = SAVER;
// (it also changes like nothing)

void ir_ac_up() {
    transmit_nec(ac_cmd_up);
    
    set_temp = MIN(set_temp + 1, AC_MAX);
}
void ir_ac_down() {
    transmit_nec(ac_cmd_down);
    
    set_temp = MAX(set_temp - 1, AC_MIN);
}
ac_mode ir_ac_cycle_mode() {
    transmit_nec(ac_cmd_mode);

    // There's no "set to this mode" command, so it wraps around the mode list
    set_mode = (set_mode + 1) % MODES_COUNT;
    return set_mode;
}

void ir_set_mode(ac_mode mode) {
    // seems like itd be easy for this to get out of sync so reset the mode
    // via power cycling before setting it
    transmit_nec(ac_cmd_pwr);
    transmit_nec(ac_cmd_pwr);
    set_mode = AC_SAVER;

    while (set_mode != mode)
        ir_ac_cycle_mode();
}
void ir_set_temp(int8_t temp) {
    while(temp > set_temp)
        ir_ac_up();

    while(temp < set_temp)
        ir_ac_down();
    // if (temp > set_temp)
    // {
        
    // }
}

void ir_reset() {
    array_uint ac_ir_pwr = nec_build_timings(ac_cmd_pwr);

    array_uint ac_ir_up = nec_build_timings(ac_cmd_up);    
    array_uint ac_ir_down = nec_build_timings(ac_cmd_down);

    transmit_ir(&ac_ir_pwr);
    transmit_ir(&ac_ir_pwr);
    set_mode = AC_SAVER;

    for (int32_t i = 0; i < (AC_MAX - AC_MIN); i++) {
        transmit_ir(&ac_ir_up);
    }

    // set to 72 deg F
    for (int32_t i = 0; i < (AC_MAX - 72); i++) {
        transmit_ir(&ac_ir_down);
    }
    set_temp = 72;


    free(ac_ir_up.data);
    free(ac_ir_down.data);
    free(ac_ir_pwr.data);
}