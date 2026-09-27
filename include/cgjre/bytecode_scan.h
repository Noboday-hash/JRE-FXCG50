#ifndef CGJRE_BYTECODE_SCAN_H
#define CGJRE_BYTECODE_SCAN_H

#include <stddef.h>
#include <stdint.h>

/* Structural scan only. Counts instruction starts, not operand bytes. */
int cgjre_bytecode_scan(const uint8_t *code, size_t length,
    uint32_t counts[256]);

#endif
