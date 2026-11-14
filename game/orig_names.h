/* Names for Mazeworld Original's globals (offsets below A5: og16(OG_LIFE)) and object record fields
 * (G_53C8 + n * $40), where what they hold is certain. The rest keep their offsets. */
#ifndef ORIG_NAMES_H
#define ORIG_NAMES_H
enum {
    OG_OBJECTS = 0x53C8,        /* long: the object records */
    OG_MAP = 0x53D4,            /* long: the map, $20 bytes a cell */
    OG_PLAYER_X = 0x56D8, OG_PLAYER_Y = 0x56D4, OG_PLAYER_CELL = 0x56D0, OG_PLAYER_HEADING = 0x56DA,
    OG_LIFE = 0x56EA,           /* 0: dead */
    OG_MESSAGE = 0x56C0,        /* the text in the message box */
    OG_FULL_VIEW = 0x56E0,      /* $80 and up: a full-view picture is showing */
    OG_GEORGE_JOINED = 0x56C2,
    OG_LEVEL = 0x56DC,
    OG_GO_LEVEL = 0x57DC, OG_GO_X = 0x57D8, OG_GO_Y = 0x57D4, OG_GO_HEADING = 0x57DA,   /* the next level (x, y 16.16) */
    OG_SELF = 0x3834,           /* the running object's record offset */
    OG_NEAR_DIST = 0x382E, OG_NEAR_DIR = 0x3830,         /* fn_S7_12BE: the player's distance and direction */
    OG_TARGET_X = 0x381C, OG_TARGET_Y = 0x3818,          /* for fn_S7_1488 */
    OG_MOVE_X = 0x382C, OG_MOVE_Y = 0x3828, OG_MOVE_CELL = 0x3824,   /* for fn_S7_1578 */
};
enum { OB_TYPE = 2, OB_X = 4, OB_Y = 8, OB_HEADING = 0xC, OB_STATE = 0x16, OB_CELL = 0x30 };
#endif
