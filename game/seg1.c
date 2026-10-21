/* CODE segment 1: the development system's start-up code. The long-arithmetic helpers (JT 4-8)
 * became plain C operators; what remains is the initialisation of the globals. */
#include "mzasm.h"

/* fn_S1_0044: initialises the globals from the 'DATA' 0 resource: its words are copied from the
 * bottom of the A5 world (CurStackBase, $908) up to A5, and each zero word copied is followed by a
 * run of zero bytes whose length is the next word of 'ZERO' 0. Then the 'DREL' 0
 * relocations: each word is an A5 offset of a long to which A5 is added. */
void fn_S1_0044(Regs *r) {
    uint32_t zero = mac_GetResource(0x5A45524F, 0);         /* 'ZERO' */
    uint32_t data = mac_GetResource(0x44415441, 0);         /* 'DATA' */
    uint32_t src = deref(data), dst = r32(0x0908), runs = deref(zero);
    while (dst != MZ_A5) {
        uint16_t w = r16(src); src += 2;
        w16(dst, w); dst += 2;
        if (!w) {
            uint16_t n = r16(runs); runs += 2;
            for (uint32_t k = 0; k < n; k++) w8(dst++, 0);
        }
    }
    mac_ReleaseResource(data);
    mac_ReleaseResource(zero);
    uint32_t drel = mac_GetResource(0x4452454C, 0);         /* 'DREL' */
    int16_t n = (int16_t)((int16_t)(uint16_t)mac_GetHandleSize(drel) >> 1);
    for (uint32_t p = deref(drel); --n >= 0; p += 2) {
        uint32_t a = MZ_A5 + (uint32_t)(int32_t)(int16_t)r16(p);
        w32(a, r32(a) + MZ_A5);
    }
    mac_ReleaseResource(drel);
    (void)r;
}
