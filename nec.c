#pragma once

#include <pico/time.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


#include "types/arrays.c"

// To get an NEC command, just make address2 the inverse of address 1.
// I'm not gonna validate SHIT.
typedef struct nec_cmd {
    uint8_t address1, address2, command;
} nec_cmd;

// Builds the NEC bit train appropriately,
// such that the LSB of the address will be the first byte it reads,
// as it reads the LSB and shifts over and over again.
const uint32_t _nec_build_bits(nec_cmd cmd) {
    // See: https://www.sbprojects.net/knowledge/ir/nec.php
    uint32_t ret = 0;
    ret += ~cmd.command;
    ret = ret << 8;
    ret += cmd.command;
    ret = ret << 8;
    ret += cmd.address2;
    ret = ret << 8;
    ret += cmd.address1;
    return ret;
}

void nec_print_bit_train(nec_cmd cmd) {
    uint32_t bit_train = _nec_build_bits(cmd);
    printf("\nBit train:\n");
    for (int byte = 0; byte < sizeof(uint32_t); byte++) {
        printf(" | Byte %d: ", byte + 1);
        uint8_t this_byte = (uint8_t)(bit_train & 0xFF);
        for (int bit = 0; bit < 8; bit++) {
            bool thisBit = bit_train & 1;
            if (thisBit) printf(" 1 ");
            else printf(" 0 ");

            sleep_ms(10);
            bit_train = bit_train >> 1;
        }
        printf("(0x%02x)", this_byte);
        sleep_ms(100);
    }
    printf(" | \n");
}

array_uint nec_build_timings(nec_cmd cmd) {
    uint32_t bit_train = _nec_build_bits(cmd);

    // burst for 9ms -> 2.25ms gap -> (transmit 4*8 bits of data by flipping) -> one extra 560us burst = 2 + 64 flips + 1 final 560us burst
    array_uint res = {
        .length = 67,
        .data = malloc(sizeof(uint) * 67),
    };

    set_uint(&res, 0, 9000); // 9ms burst
    set_uint(&res, 1, 4500); // 4.5ms gap

    int idx = 0;
    for (int i = 0; i < sizeof(uint32_t) * 8; i++) {
        int offset = 2 + 2 * i; // initial burst->gap->(each bit * 2 per flip)
        bool bit = bit_train & 1;
        printf("Bit @ pos %d is %d", offset, bit);

        if (bit) {
            // logical 1: 2.25ms in total
            set_uint(&res, offset, 560);
            set_uint(&res, offset + 1, 2250 - 560);
        }
        else {
            // logical 0: 1.125ms in total
            set_uint(&res, offset, 560);
            set_uint(&res, offset + 1, 1125 - 560);
        }
        
        bit_train = bit_train >> 1;
    }

    set_uint(&res, res.length - 1, 560); // end with a 560us burst
    return res;
}