/* CODE segment 3: the development system's Toolbox glue (Pascal calling convention): small
 * routines that pass their arguments on to a trap. In the port these become direct calls. */
#include "mz.h"

/* fn_S3_038E (JT 126): NumToString(num, VAR str) via Pack7. */
void fn_S3_038E(int32_t num, uint32_t str) { mac_NumToString(num, str); }

/* fn_S3_03A0 (JT 127): StripAddress(addr): the trap, or the mask in low memory (Lo3Bytes, $31A)
 * when the machine is 32-bit clean (bit 6 of ROM85's high byte, $28E). */
uint32_t fn_S3_03A0(uint32_t addr) {
    if (r8(0x028E) & 0x40) return addr & r32(0x031A);
    return mac_StripAddress(addr);
}

/* fn_S3_020E (JT 112): GetHandleSize(h), with an error (negative) reported as 0. */
int32_t fn_S3_020E(uint32_t h) {
    int32_t n = mac_GetHandleSize(h);
    return n < 0 ? 0 : n;
}

/* fn_S3_0000 (JT 111): Gestalt(selector, VAR response): OSErr, through the trap. (Without the
 * Gestalt trap the glue answers a few selectors itself; every system the port targets has it.) */
int16_t fn_S3_0000(uint32_t selector, uint32_t presponse) {
    if (mac_NGetTrapAddress(0xA0AD, 0) == mac_NGetTrapAddress(0xA89F, 1)) {
        mz_unsupported("Gestalt glue without the Gestalt trap");
        return -5551;                                   /* gestaltUndefSelectorErr */
    }
    return mac_Gestalt(selector, presponse);
}

/* fn_S3_021C (JT 113): NGetTrapAddress(trapNum, tTyp): the Toolbox table if tTyp, else the OS one. */
uint32_t fn_S3_021C(int16_t trap, uint8_t toolbox) { return mac_NGetTrapAddress((uint16_t)trap, toolbox != 0); }

/* fn_S3_0230 (JT 114): GetDCtlEntry(refNum): the driver's entry in the unit table (UTableBase $11C,
 * UnitNtryCnt $1D2), or NIL for a unit number outside it. */
uint32_t fn_S3_0230(int16_t ref) {
    uint16_t unit = (uint16_t)~ref;
    if (unit >= r16(0x01D2)) return 0;
    return r32(r32(0x011C) + (uint32_t)(int32_t)(int16_t)(uint16_t)(unit << 2));
}

/* fn_S3_03BA (JT 128): GetIndString(VAR str, strListID, index): string `index` (from 1) of
 * 'STR#' strListID, or the empty string. */
void fn_S3_03BA(uint32_t str, int16_t id, int16_t index) {
    uint32_t h = mac_GetResource(0x53545223, id);        /* 'STR#' */
    w8(str, 0);
    if (!h) return;
    uint32_t p = deref(h);
    uint16_t count = r16(p); p += 2;
    if (!index || (uint16_t)index > count) return;
    for (uint16_t i = (uint16_t)(index - 1); i; i--) p += 1u + r8(p);
    mac_BlockMove(p, str, (int32_t)r8(p) + 1);
}

/* fn_S3_0400 (JT 110): FindFolder(vRefNum, folderType, createFolder, VAR foundVRefNum,
 * VAR foundDirID): OSErr, when the Folder Manager is present (Gestalt 'fold' bit 0). (Without it the
 * glue finds a few folders through the File Manager; every system the port targets has it.) */
int16_t fn_S3_0400(int16_t vref, uint32_t type, uint8_t create, uint32_t pvref, uint32_t pdir) {
    MZ_FRAME;
    uint32_t resp = mz_local(4);
    if (fn_S3_0000(0x666F6C64, resp) == 0 && (r32(resp) & 1))   /* 'fold' */
        return mac_FindFolder(vref, type, create != 0, pvref, pdir);
    mz_unsupported("FindFolder glue without the Folder Manager");
    return -50;
}

/* ---------- File Manager glue: each builds a parameter block (the original's frame) and calls the
 * register-based trap ---------- */
static uint32_t param_block(void) { return mz_local(0x32); }   /* in the caller's frame */

/* fn_S3_024A (JT 115): FSClose(refNum) */
int16_t fn_S3_024A(int16_t ref) {
    MZ_FRAME;
    uint32_t pb = param_block();
    w16(pb + 0x18, (uint16_t)ref);                          /* ioRefNum */
    return mac_PBCall(0xA001, pb);                          /* _PBClose */
}

/* fn_S3_0264 / 0268 (JT 116 / 117): FSRead / FSWrite(refNum, VAR count, buffPtr): from the current
 * position; count comes back as the bytes transferred. */
static int16_t read_write(int write, int16_t ref, uint32_t pcount, uint32_t buf) {
    MZ_FRAME;
    uint32_t pb = param_block();
    w32(pb + 0x20, buf);                                    /* ioBuffer */
    w16(pb + 0x18, (uint16_t)ref);
    w32(pb + 0x24, r32(pcount));                            /* ioReqCount */
    w16(pb + 0x2C, 0); w32(pb + 0x2E, 0);                   /* ioPosMode fsAtMark, ioPosOffset */
    int16_t err = mac_PBCall(write ? 0xA003 : 0xA002, pb);
    w32(pcount, r32(pb + 0x28));                            /* ioActCount */
    return err;
}
int16_t fn_S3_0264(int16_t ref, uint32_t pcount, uint32_t buf) { return read_write(0, ref, pcount, buf); }
int16_t fn_S3_0268(int16_t ref, uint32_t pcount, uint32_t buf) { return read_write(1, ref, pcount, buf); }

/* fn_S3_02AC (JT 118): FlushVol(volName, vRefNum) */
int16_t fn_S3_02AC(uint32_t name, int16_t vref) {
    MZ_FRAME;
    uint32_t pb = param_block();
    w16(pb + 0x16, (uint16_t)vref); w32(pb + 0x12, name);   /* ioVRefNum, ioNamePtr */
    return mac_PBCall(0xA013, pb);
}

/* fn_S3_02CC (JT 119): GetEOF(refNum, VAR logEOF) */
int16_t fn_S3_02CC(int16_t ref, uint32_t peof) {
    MZ_FRAME;
    uint32_t pb = param_block();
    w16(pb + 0x18, (uint16_t)ref);
    int16_t err = mac_PBCall(0xA011, pb);
    w32(peof, r32(pb + 0x1C));                              /* ioMisc */
    return err;
}

/* fn_S3_02EE (JT 120): SetEOF(refNum, logEOF) */
int16_t fn_S3_02EE(int16_t ref, int32_t eof) {
    MZ_FRAME;
    uint32_t pb = param_block();
    w16(pb + 0x18, (uint16_t)ref); w32(pb + 0x1C, (uint32_t)eof);
    return mac_PBCall(0xA012, pb);
}

/* fn_S3_030E (JT 121): SetFPos(refNum, posMode, posOff) */
int16_t fn_S3_030E(int16_t ref, int16_t mode, int32_t off) {
    MZ_FRAME;
    uint32_t pb = param_block();
    w16(pb + 0x18, (uint16_t)ref); w16(pb + 0x2C, (uint16_t)mode); w32(pb + 0x2E, (uint32_t)off);
    return mac_PBCall(0xA044, pb);
}

/* fn_S3_0334 (JT 122): GetVRefNum(refNum, VAR vRefNum): the volume of an open file, from its FCB
 * (FCBSPtr $34E, FSFCBLen $3F6, else $5E-byte FCBs); rfNumErr (-51) for a bad refNum. */
int16_t fn_S3_0334(int16_t ref, uint32_t pvref) {
    uint32_t fcbs = r32(0x034E);
    int16_t len = (int16_t)r16(0x03F6);
    uint16_t r = (uint16_t)ref;
    uint16_t rem = (uint16_t)(r % (uint16_t)(len < 0 ? 0x5E : len));
    uint16_t vref = 0; int16_t err = -51;
    if (rem == 2 && r < r16(fcbs)) {
        vref = r16(r32(fcbs + (uint32_t)(int32_t)(int16_t)r + 0x14) + 0x4E);   /* the FCB's VCB: vcbVRefNum */
        err = 0;
    }
    w16(pvref, vref);
    return err;
}
