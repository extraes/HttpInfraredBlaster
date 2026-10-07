#include <hardware/gpio.h>
#include <hardware/timer.h>
#include <pico/stdio.h>
#include <pico/stdlib.h>
#include <pico/time.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ir.c"
#include "wifi_http_server.h"

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

// Pico W devices use a GPIO on the WIFI chip for the LED,
// so when building for Pico W, CYW43_WL_GPIO_LED_PIN will be defined
#ifdef CYW43_WL_GPIO_LED_PIN
#include "pico/cyw43_arch.h"
#endif

typedef struct req_data {
    char* method;
    char* route;
} req_data;

void pico_set_led(bool led_on) {
    #if defined(PICO_DEFAULT_LED_PIN)
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
    #elif defined (CYW43_WL_GPIO_LED_PIN)
    return cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
    #endif
}



static req_data extract_method_and_route(const char* request, size_t length) {
    size_t first_line_length = 0;
    for (int i = 0; i < length; i++) {
        first_line_length++;

        char ch = request[i];
        
        // if (ch == ' ') {
        //     if (method_length == 0 && ch == ' ')
        //         ch = i;
        //     if ()
        // }
        
        if (ch == '\n')
        {
            break;
        }
    }

    char* first_line = malloc(first_line_length + 1); // so its null terminated
    strncpy(first_line, request, first_line_length);

    char* method = strtok(first_line, " ");
    char* route = strtok(NULL, " ");
    char* http_ver = strtok(NULL, "\n ");

    if (!method || !route || !http_ver)
    {
        free(first_line);
        return (req_data){0};
    }


    int method_size = route - method;
    char* realloc_method = malloc(method_size + 1); // for null byte
    strncpy(realloc_method, method, method_size);
    
    int route_size = http_ver - route;
    char* realloc_route = malloc(route_size + 1); // still for null byte
    strncpy(realloc_route, route, route_size);

    req_data ret = {
        .method = realloc_method,
        .route = realloc_route,
    };
    free(first_line);
    return ret;
}

static void on_http_request(const char *request, size_t length) {
    // busy_wait_ms(500);
    // printf("fire callback fire callback fire callback fire callback fire callback fire callback fire callback fire callback fire callback fire callback fire callback fire callback @:\n%.*s\n", (int)length, request);
    // printf("\n\n%.*s\n\n", (int)length, request);
    // busy_wait_ms(500);
    
    #ifdef ACTIVITY_LED_PIN
    gpio_put(ACTIVITY_LED_PIN, true);
    #endif
    req_data data = extract_method_and_route(request, length);
    if (!data.method)
    {
        printf("Malformed request, skipping to end: %.*s", (int)length, request);
        goto done;
    }

    printf("Got a %s request to %s\n", data.method, data.route);

    if (!strcmp(data.method, "GET")) {
        // ignore, this callback doesnt write to response
        // needs to exit at the bottom to turn off activity LED

        // if (!strcmp(data.route, "reset"))
        //     ir_reset();
        // else if (!strcmp(data.route, "acup"))
        //     ir_ac_up();
        goto done;
    }
    else if (!strcmp(data.method, "POST"))
    {
        if (!strcmp(data.route, "/reset"))
            ir_reset();
        else if (!strcmp(data.route, "/acup"))
            ir_ac_up();
        else if (!strcmp(data.route, "/acdown"))
            ir_ac_down();
        else if (!strcmp(data.route, "/acmode"))
            ir_ac_cycle_mode();
        else if (!strncmp(data.route, "/acmodeset", sizeof("/acmodeset")-1)) // null term increases size by 1
        {
            int routesize = sizeof("/acmodeset")-1;
            if (!strcmp(data.route + routesize, "saver"))
                ir_set_mode(AC_SAVER);
            else if (!strcmp(data.route + routesize, "cool"))
                ir_set_mode(AC_COOL);
            else if (!strcmp(data.route + routesize, "fan"))
                ir_set_mode(AC_FAN);
            else if (!strcmp(data.route + routesize, "dry"))
                ir_set_mode(AC_DRY);
            else {
                if (data.route[routesize] != '\0')
                    printf("unknown AC mode: %s, defaulting to cycling.", (data.route + routesize));

                ir_ac_cycle_mode();
            }
        }
    }

    done:
    free(data.method);
    free(data.route);

    #ifdef ACTIVITY_LED_PIN
    gpio_put(ACTIVITY_LED_PIN, false);
    #endif
}

int main() {
    stdio_init_all();

    ir_reset();

    #ifdef ACTIVITY_LED_PIN
    gpio_init(ACTIVITY_LED_PIN);
    gpio_set_dir(ACTIVITY_LED_PIN, true);
    #endif

    wifi_http_server_register_request_callback(on_http_request);
    // wifi_http_server_start initializes the CYW43 chip used by both Wi-Fi and
    // the Pico W's LED. Configure credentials with CMake; see the build command
    // in the project handoff notes.
    printf("logging on to %s with %s\n", WIFI_SSID, WIFI_PASSWORD);

    for (int i = 0; i < 5; i++) {
        bool success = wifi_http_server_start(WIFI_SSID, WIFI_PASSWORD, 80);

        if (success){
            sleep_ms(500);
            printf("successfully connected & started server!\n");
            break;
        }

        sleep_ms(5000);
    }

    // pico_set_led(true);
    // sleep_ms(100);
    // pico_set_led(false);

    // ir_test();

    while(true) {
        sleep_ms(500);
        pico_set_led(true);
        sleep_ms(500);
        pico_set_led(false);
    }
}
