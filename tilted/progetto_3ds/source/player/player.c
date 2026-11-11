#include "player.h"
#include "ds_map.h"
#include "encounter.h"

typedef struct {
    int             grid_x;
    int             grid_y;
    int             target_x;
    int             target_y;
    float           pos_x;
    float           pos_y;
    PlayerDirection dir;
    bool            moving;
    float           move_progress;
    int             step_parity;
} PlayerState;

static PlayerState s_player;

static int getPlayerAnimFrame(void) {
    int idle_frame  = 10;
    int step1_frame = 11;
    int step2_frame = 12;

    switch (s_player.dir) {
        case DIR_UP:
            idle_frame  = 0;
            step1_frame = 8;
            step2_frame = 9;
            break;
        case DIR_LEFT:
            idle_frame  = 2;
            step1_frame = 1;
            step2_frame = 3;
            break;
        case DIR_RIGHT:
            idle_frame  = 4;
            step1_frame = 5;
            step2_frame = 6;
            break;
        case DIR_DOWN:
        default:
            idle_frame  = 10;
            step1_frame = 11;
            step2_frame = 12;
            break;
    }

    if (!s_player.moving) {
        return idle_frame;
    }

    int first_step  = (s_player.step_parity == 0) ? step1_frame : step2_frame;
    int second_step = (s_player.step_parity == 0) ? step2_frame : step1_frame;

    if (s_player.move_progress < 0.45f) {
        return first_step;
    } else if (s_player.move_progress < 0.55f) {
        return idle_frame;
    } else if (s_player.move_progress < 0.95f) {
        return second_step;
    }
    return idle_frame;
}

void playerInit(int start_x, int start_y) {
    s_player.grid_x        = start_x;
    s_player.grid_y        = start_y;
    s_player.target_x      = start_x;
    s_player.target_y      = start_y;
    s_player.pos_x         = (float)start_x;
    s_player.pos_y         = (float)start_y;
    s_player.dir           = DIR_DOWN;
    s_player.moving        = false;
    s_player.move_progress = 0.0f;
    s_player.step_parity   = 0;
}

void playerSetDirection(PlayerDirection dir) {
    s_player.dir = dir;
}

void playerUpdate(u32 kDown, u32 kHeld) {
    if (s_player.moving) {
        s_player.move_progress += PLAYER_MOVE_SPEED;
        if (s_player.move_progress >= 1.0f) {
            s_player.grid_x        = s_player.target_x;
            s_player.grid_y        = s_player.target_y;
            s_player.pos_x         = (float)s_player.grid_x;
            s_player.pos_y         = (float)s_player.grid_y;
            s_player.move_progress = 0.0f;
            s_player.moving        = false;
            s_player.step_parity  ^= 1;
            encounterOnStep(s_player.grid_x, s_player.grid_y);
            if (encounterGetState() == GAME_STATE_ENCOUNTER) {
                return;
            }
        } else {
            s_player.pos_x = (float)s_player.grid_x + (float)(s_player.target_x - s_player.grid_x) * s_player.move_progress;
            s_player.pos_y = (float)s_player.grid_y + (float)(s_player.target_y - s_player.grid_y) * s_player.move_progress;
            return;
        }
    }

    circlePosition cp;
    hidCircleRead(&cp);

    u32 input = kDown | kHeld;
    int dx = 0;
    int dy = 0;
    PlayerDirection wanted_dir = s_player.dir;
    bool wants_move = false;

    if ((input & KEY_UP) || cp.dy > 45) {
        dx = 0;
        dy = -1;
        wanted_dir = DIR_UP;
        wants_move = true;
    } else if ((input & KEY_DOWN) || cp.dy < -45) {
        dx = 0;
        dy = 1;
        wanted_dir = DIR_DOWN;
        wants_move = true;
    } else if ((input & KEY_LEFT) || cp.dx < -45) {
        dx = -1;
        dy = 0;
        wanted_dir = DIR_LEFT;
        wants_move = true;
    } else if ((input & KEY_RIGHT) || cp.dx > 45) {
        dx = 1;
        dy = 0;
        wanted_dir = DIR_RIGHT;
        wants_move = true;
    }

    if (wants_move) {
        s_player.dir = wanted_dir;
        int nx = s_player.grid_x + dx;
        int ny = s_player.grid_y + dy;

        if (dsMapCanMove(s_player.grid_x, s_player.grid_y, nx, ny)) {
            s_player.target_x      = nx;
            s_player.target_y      = ny;
            s_player.moving        = true;
            s_player.move_progress = 0.0f;
        }
    }
}

void playerGetGridPos(float* out_x, float* out_y) {
    if (out_x) *out_x = s_player.pos_x;
    if (out_y) *out_y = s_player.pos_y;
}

void playerGetWorldPos(float* out_x, float* out_y, float* out_z) {
    dsMapGetWorldPos(s_player.pos_x, s_player.pos_y, out_x, out_y, out_z);
}

void playerBuildQuad(TileVertex out_verts[6]) {
    float wx, wy, wz;
    playerGetWorldPos(&wx, &wy, &wz);

    float half_pw = PLAYER_SPRITE_W * 0.5f;
    float x_left  = wx - half_pw;
    float x_right = wx + half_pw;

    float z_bot = wz + 6.0f;
    float z_top = z_bot - PLAYER_SLANT_Z;
    float y_bot = wy + 0.5f;
    float y_top = y_bot + PLAYER_RISE_Y;

    int frame = getPlayerAnimFrame();
    float u_left   = 0.0f;
    float u_right  = 1.0f;
    float v_top    = 1.0f - ((float)frame * PLAYER_SPRITE_H / PLAYER_TEX_H);
    float v_bottom = 1.0f - ((float)(frame + 1) * PLAYER_SPRITE_H / PLAYER_TEX_H);

    out_verts[0] = (TileVertex){x_left,  y_bot, z_bot, u_left,  v_bottom, 0.0f, 0.0f};
    out_verts[1] = (TileVertex){x_right, y_bot, z_bot, u_right, v_bottom, 0.0f, 0.0f};
    out_verts[2] = (TileVertex){x_right, y_top, z_top, u_right, v_top,    0.0f, 0.0f};

    out_verts[3] = (TileVertex){x_left,  y_bot, z_bot, u_left,  v_bottom, 0.0f, 0.0f};
    out_verts[4] = (TileVertex){x_right, y_top, z_top, u_right, v_top,    0.0f, 0.0f};
    out_verts[5] = (TileVertex){x_left,  y_top, z_top, u_left,  v_top,    0.0f, 0.0f};
}

bool playerShouldOverlayTile(int gx, int gy) {
    if (!s_player.moving) {
        return (gx == s_player.grid_x && gy == s_player.grid_y);
    }

    switch (s_player.dir) {
        case DIR_DOWN:
            // Movimento verso il basso (Sud):
            // Il tile a monte (grid_y) è alle spalle del personaggio e non deve
            // MAI mostrare l'overlay sul corpo o sulla testa.
            // Solo il tile di arrivo (target_y) mostra l'overlay non appena i piedi vi entrano.
            if (gx == s_player.target_x && gy == s_player.target_y) {
                return (s_player.move_progress >= 0.40f);
            }
            return false;

        case DIR_UP:
            // Movimento verso l'alto (Nord):
            // Il tile di partenza (grid_y) è a Sud del personaggio e copre i piedi nei primi istanti del passo.
            // Il tile a Nord (target_y) NON deve mostrare l'overlay durante la salita,
            // altrimenti finisce sopra la testa e la schiena del personaggio.
            // L'overlay del tile di arrivo si attiverà non appena il personaggio vi si ferma sopra.
            if (gx == s_player.grid_x && gy == s_player.grid_y) {
                return (s_player.move_progress < 0.40f);
            }
            return false;

        case DIR_LEFT:
        case DIR_RIGHT:
            if (gy != s_player.grid_y) return false;
            if (s_player.move_progress < 0.50f) {
                return (gx == s_player.grid_x);
            } else {
                return (gx == s_player.target_x);
            }
    }
    return false;
}
