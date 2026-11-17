/* made by tri2h.py from refsw2-offline's log: corners x, y, z (bit patterns) and colours */
#include <stdint.h>
#define NUM_DTRIS 1
typedef struct { const char *name; uint32_t v[3][3]; uint32_t col[3]; } dtri_t;
static const dtri_t dtris[NUM_DTRIS] = {
    { "0C056E89", { { 0x4416d920u, 0x435ab3e6u, 0x3cfd94ddu }, { 0x43fcba3au, 0x43c61cedu, 0x3cf9aa4bu }, { 0x44052397u, 0x43532a5bu, 0x3cee5b91u } }, { 0xFE3D3D4Bu, 0xFE3D3D4Bu, 0xFE3D3D4Bu } },
};
