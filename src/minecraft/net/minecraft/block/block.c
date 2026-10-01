#include "block.h"
#include "item/item.h"

bool mc_block_valid(uint16_t state) { return (state >> 4) < 198; }
bool mc_block_replaceable(uint16_t state) {
    switch (state >> 4) {
        case 0: case 8: case 9: case 10: case 11: case 31: case 32:
        case 51: case 106: return true;
        case 78: return (state & 7) == 0;
        default: return false;
    }
}
bool mc_block_placeable_item(int16_t id) {
    uint16_t state;
    return mc_item_block_state(id, 0, &state);
}
static bool stair(unsigned id) {
    switch (id) {
        case 53: case 67: case 108: case 109: case 114: case 128:
        case 134: case 135: case 136: case 156: case 163: case 164:
        case 180: return true;
        default: return false;
    }
}
unsigned mc_block_collision(uint16_t state, mc_box boxes[3]) {
    unsigned id = state >> 4, meta = state & 15;
    if (!boxes || !mc_block_valid(state)) return 0;
    boxes[0] = (mc_box){0, 0, 0, 1, 1, 1};
    if (stair(id)) {
        bool upper = (meta & 4) != 0;
        boxes[0].min_y = upper ? 0.5f : 0;
        boxes[0].max_y = upper ? 1 : 0.5f;
        boxes[1] = (mc_box){0, upper ? 0 : 0.5f, 0, 1, upper ? 0.5f : 1, 1};
        switch (meta & 3) {
            case 0: boxes[1].min_x = 0.5f; break;
            case 1: boxes[1].max_x = 0.5f; break;
            case 2: boxes[1].min_z = 0.5f; break;
            case 3: boxes[1].max_z = 0.5f; break;
        }
        return 2;
    }
    switch (id) {
        case 0: case 6: case 8: case 9: case 10: case 11: case 27: case 28:
        case 30: case 31: case 32: case 37: case 38: case 39: case 40:
        case 50: case 51: case 55: case 59: case 63: case 65: case 66:
        case 68: case 69: case 75: case 76: case 77: case 83: case 90:
        case 104: case 105: case 106: case 115: case 119: case 127:
        case 131: case 132: case 141: case 142: case 143: case 157:
        case 175: case 176: case 177: return 0;
        case 44: case 126: case 182:
            boxes[0].min_y = (meta & 8) ? 0.5f : 0;
            boxes[0].max_y = (meta & 8) ? 1 : 0.5f; break;
        case 26: boxes[0].max_y = 0.5625f; break;
        case 54: case 130: case 146:
            boxes[0] = (mc_box){0.0625f, 0, 0.0625f, 0.9375f, 0.875f, 0.9375f}; break;
        case 60: boxes[0].max_y = 0.9375f; break;
        case 70: case 72: case 147: case 148: return 0;
        case 78:
            if (!(meta & 7)) return 0;
            boxes[0].max_y = (float)(meta & 7) / 8.0f; break;
        case 81: boxes[0] = (mc_box){0.0625f, 0, 0.0625f, 0.9375f, 0.9375f, 0.9375f}; break;
        case 85: case 113: case 188: case 189: case 190: case 191: case 192:
            boxes[0] = (mc_box){0.375f, 0, 0.375f, 0.625f, 1.5f, 0.625f}; break;
        case 88: boxes[0].max_y = 0.875f; break;
        case 92: boxes[0] = (mc_box){0.0625f + (float)(meta > 6 ? 6 : meta) / 8.0f, 0, 0.0625f, 0.9375f, 0.5f, 0.9375f}; break;
        case 96: case 167:
            if (meta & 4) {
                switch (meta & 3) {
                    case 0: boxes[0].min_z = 0.8125f; break;
                    case 1: boxes[0].max_z = 0.1875f; break;
                    case 2: boxes[0].min_x = 0.8125f; break;
                    case 3: boxes[0].max_x = 0.1875f; break;
                }
            } else {
                boxes[0].min_y = (meta & 8) ? 0.8125f : 0;
                boxes[0].max_y = (meta & 8) ? 1 : 0.1875f;
            }
            break;
        case 101: case 102: case 160:
            boxes[0] = (mc_box){0.4375f, 0, 0.4375f, 0.5625f, 1, 0.5625f}; break;
        case 107: case 183: case 184: case 185: case 186: case 187:
            if (meta & 4) return 0;
            boxes[0] = (meta & 1) ? (mc_box){0.375f, 0, 0, 0.625f, 1.5f, 1}
                                        : (mc_box){0, 0, 0.375f, 1, 1.5f, 0.625f}; break;
        case 111: boxes[0].max_y = 0.015625f; break;
        case 116: boxes[0].max_y = 0.75f; break;
        case 120: boxes[0].max_y = 0.8125f; break;
        case 139: boxes[0] = (mc_box){0.25f, 0, 0.25f, 0.75f, 1.5f, 0.75f}; break;
        case 140: boxes[0] = (mc_box){0.3125f, 0, 0.3125f, 0.6875f, 0.375f, 0.6875f}; break;
        case 144: boxes[0] = (mc_box){0.25f, 0, 0.25f, 0.75f, 0.5f, 0.75f}; break;
        case 151: case 178: boxes[0].max_y = 0.375f; break;
        case 171: boxes[0].max_y = 0.0625f; break;
        default: break;
    }
    return 1;
}
bool mc_block_opaque(uint16_t state) {
    unsigned id = state >> 4;
    if (!mc_block_valid(state) || stair(id)) return false;
    switch (id) {
        case 0: case 6: case 8: case 9: case 10: case 11: case 18: case 20:
        case 26: case 27: case 28: case 30: case 31: case 32: case 37: case 38:
        case 39: case 40: case 44: case 50: case 51: case 52: case 54: case 55:
        case 59: case 60: case 63: case 64: case 65: case 66: case 68: case 69:
        case 70: case 71: case 72: case 75: case 76: case 77: case 78: case 79:
        case 81: case 83: case 85: case 90: case 92: case 93: case 94: case 95:
        case 96: case 101: case 102: case 104: case 105: case 106: case 107:
        case 111: case 113: case 115: case 116: case 117: case 118: case 119:
        case 120: case 122: case 126: case 127: case 130: case 131: case 132:
        case 138: case 139: case 140: case 141: case 142: case 143: case 144:
        case 145: case 146: case 147: case 148: case 149: case 150: case 151:
        case 154: case 157: case 160: case 161: case 165: case 167: case 171:
        case 175: case 176: case 177: case 178: case 182: case 183: case 184:
        case 185: case 186: case 187: case 188: case 189: case 190: case 191:
        case 192: case 193: case 194: case 195: case 196: case 197: return false;
        default: return true;
    }
}

/* Immutable native registry view of the original Block Material references.
   Metadata does not select a different material. Full Block/Material classes
   and their constructor/virtual dispatch are separate ports. */
bool mc_block_material_flags(uint16_t state,bool *movement,bool *leaves) {
    static const uint8_t flags[198]={
        0,1,1,1,1,1,0,1,0,0,0,0,1,1,1,1,1,1,
        3,1,1,1,1,1,1,1,1,0,0,1,0,0,0,1,1,1,
        1,0,0,0,0,1,1,1,1,1,1,1,1,1,0,0,1,1,
        1,0,1,1,1,0,1,1,1,1,1,0,0,1,1,0,1,1,
        1,1,1,0,0,0,0,1,1,1,1,0,1,1,1,1,1,1,
        0,1,1,0,0,1,1,1,1,1,1,1,1,1,0,0,0,1,
        1,1,1,0,1,1,1,0,1,1,1,0,1,1,1,1,1,1,
        1,0,1,1,1,0,0,1,1,1,1,1,1,1,0,0,0,0,
        0,1,1,1,1,0,0,1,1,1,1,1,1,0,1,1,1,3,
        1,1,1,1,1,1,1,1,1,0,1,1,1,0,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
    };
    if(!movement||!leaves||!mc_block_valid(state))return false;
    *movement=(flags[state>>4]&1)!=0;*leaves=(flags[state>>4]&2)!=0;return true;
}

/* Independently observed registry field facts. Metadata does not change the
   reached Block.getLightOpacity() getter; full block/state classes are pending. */
bool mc_block_light_opacity(uint16_t state,int32_t *out) {
    static const uint8_t opacity[198]={
        0,255,255,255,255,255,0,255,3,3,0,0,255,255,255,255,255,255,
        1,255,0,255,255,255,255,255,0,0,0,0,1,0,0,0,0,255,
        0,0,0,0,0,255,255,255,255,255,255,255,255,255,0,0,0,255,
        0,0,255,255,255,0,255,255,255,0,0,0,0,255,0,0,0,0,
        0,255,255,0,0,0,0,3,255,0,255,0,255,0,255,255,255,255,
        0,255,0,0,0,0,0,255,255,255,255,0,0,255,0,0,0,0,
        255,255,255,0,255,0,255,0,0,0,0,0,0,255,0,255,255,255,
        255,0,255,255,0,0,0,255,255,255,255,255,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,255,255,0,255,255,0,255,255,0,1,
        255,255,255,0,0,0,255,255,255,0,255,255,255,0,0,0,0,255,
        255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    };
    if(!out||!mc_block_valid(state))return false;
    *out=opacity[state>>4];return true;
}
