#include "player.h"
#include "collision.h"
#include "world.h"

typedef struct {
    int grid_x;
    int grid_y;
    int target_x;
    int target_y;
    int level;
    float pos_x;
    float pos_y;
    PlayerDirection dir;
    bool moving;
    float move_progress;
    int step_parity;
} PlayerState;

static PlayerState player;

// Restituisce l'indice del frame (0..12) nello spritesheet 32x512 in base alla direzione e allo stato di camminata
static int getPlayerAnimFrame(void) {
    int idle_frame  = 10;
    int step1_frame = 11;
    int step2_frame = 12;

    switch (player.dir) {
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

    if (!player.moving) {
        return idle_frame;
    }

    int first_step  = (player.step_parity == 0) ? step1_frame : step2_frame;
    int second_step = (player.step_parity == 0) ? step2_frame : step1_frame;

    // Durante il movimento verso il tile successivo mostriamo l'intero ciclo del passo
    if (player.move_progress < 0.45f) {
        return first_step;
    } else if (player.move_progress < 0.55f) {
        return idle_frame;
    } else if (player.move_progress < 0.95f) {
        return second_step;
    }
    return idle_frame;
}

void playerInit(int start_x, int start_y) {
    player.grid_x        = start_x;
    player.grid_y        = start_y;
    player.target_x      = start_x;
    player.target_y      = start_y;
    player.pos_x         = (float)start_x;
    player.pos_y         = (float)start_y;
    player.dir           = DIR_DOWN;
    player.moving        = false;
    player.move_progress = 0.0f;
    player.step_parity   = 0;

    int walk_level = collisionGetWalkableLevel(start_x, start_y);
    player.level = (walk_level != COLLISION_BLOCKED_LEVEL) ? walk_level : worldGetBaseGroundLevel();
}

void playerUpdate(u32 kDown, u32 kHeld) {
    // Se il giocatore sta già camminando verso il prossimo tile, continua l'animazione fino all'arrivo
    if (player.moving) {
        player.move_progress += PLAYER_MOVE_SPEED;
        if (player.move_progress >= 1.0f) {
            player.grid_x        = player.target_x;
            player.grid_y        = player.target_y;
            player.pos_x         = (float)player.grid_x;
            player.pos_y         = (float)player.grid_y;
            player.move_progress = 0.0f;
            player.moving        = false;
            player.step_parity  ^= 1;
        } else {
            player.pos_x = (float)player.grid_x + (float)(player.target_x - player.grid_x) * player.move_progress;
            player.pos_y = (float)player.grid_y + (float)(player.target_y - player.grid_y) * player.move_progress;
            return;
        }
    }

    // Controlla se è stato premuto (anche un solo click) o tenuto premuto un tasto direzionale
    u32 input = kDown | kHeld;
    int dx = 0;
    int dy = 0;
    PlayerDirection wanted_dir = player.dir;
    bool wants_move = false;

    if (input & KEY_UP) {
        dx = 0;
        dy = -1;
        wanted_dir = DIR_UP;
        wants_move = true;
    } else if (input & KEY_DOWN) {
        dx = 0;
        dy = 1;
        wanted_dir = DIR_DOWN;
        wants_move = true;
    } else if (input & KEY_LEFT) {
        dx = -1;
        dy = 0;
        wanted_dir = DIR_LEFT;
        wants_move = true;
    } else if (input & KEY_RIGHT) {
        dx = 1;
        dy = 0;
        wanted_dir = DIR_RIGHT;
        wants_move = true;
    }

    if (wants_move) {
        player.dir = wanted_dir;
        int nx = player.grid_x + dx;
        int ny = player.grid_y + dy;

        if (collisionCanMove(player.grid_x, player.grid_y, player.level, nx, ny)) {
            player.target_x      = nx;
            player.target_y      = ny;
            player.moving        = true;
            player.move_progress = 0.0f;
        }
    }
}

void playerGetWorldPos(float* out_x, float* out_y, float* out_z) {
    float map_half_w = (MAP_GEN_W * TILE_W) * 0.5f;
    float map_half_d = (MAP_GEN_H * TILE_H) * 0.5f;

    if (out_x) *out_x = (player.pos_x * TILE_W + TILE_W * 0.5f) - map_half_w;
    if (out_y) *out_y = (float)player.level * LEVEL_HEIGHT;
    if (out_z) *out_z = (player.pos_y * TILE_H + TILE_H * 0.5f) - map_half_d;
}

int playerGetSortRow(void) {
    // Usiamo ceilf(pos_y) così che durante il passo verticale entrambi i tile di terreno sotto i piedi
    // vengano disegnati prima del giocatore, mentre gli alberi con base >= sort_row vengano disegnati davanti
    return (int)ceilf(player.pos_y - 0.001f);
}

void playerBuildQuad(TileVertex out_verts[6]) {
    float wx, wy, wz;
    playerGetWorldPos(&wx, &wy, &wz);

    float half_pw = PLAYER_SPRITE_W * 0.5f; // 16.0f
    float x_left  = wx - half_pw;
    float x_right = wx + half_pw;

    // I piedi poggiano nella metà inferiore della cella; il corpo si alza in +Y inclinato verso la telecamera
    float z_bot = wz + 6.0f;
    float z_top = z_bot - PLAYER_SLANT_Z;
    float y_bot = wy + 0.2f;
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

