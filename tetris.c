#include "raylib.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define COLS 10
#define ROWS 20

// Each piece lives in its own square grid (size in SIZES) centered on the grid
// midpoint, so a 90-degree rotation about that midpoint leaves the piece where
// it is. 3x3 for T/L/J/S/Z, 2x2 for O, 4x4 for I (centered on its middle row).
static const int SHAPES[7][4][4] = {
    { {0,0,0,0}, {1,1,1,1}, {0,0,0,0}, {0,0,0,0} }, // I
    { {1,1,0,0}, {1,1,0,0}, {0,0,0,0}, {0,0,0,0} }, // O
    { {0,1,0,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0} }, // T
    { {1,0,0,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0} }, // L
    { {0,0,1,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0} }, // J
    { {0,1,1,0}, {1,1,0,0}, {0,0,0,0}, {0,0,0,0} }, // S
    { {1,1,0,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0} }, // Z
};

static const int SIZES[7] = { 4, 2, 3, 3, 3, 3, 3 };

static const Color COLORS[8] = {
    {0,0,0,0},          // empty
    {0,255,255,255},    // I cyan
    {255,255,0,255},    // O yellow
    {160,0,255,255},    // T purple
    {255,150,0,255},    // L orange
    {0,0,255,255},      // J blue
    {0,255,0,255},      // S green
    {255,0,0,255},      // Z red
};

static int board[ROWS][COLS];

typedef struct { int s[4][4]; int x, y; int type; } Piece;

static int collide(int b[ROWS][COLS], int s[4][4], int px, int py) {
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++) {
            if (!s[y][x]) continue;
            int bx = px + x, by = py + y;
            if (bx < 0 || bx >= COLS || by >= ROWS) return 1;
            if (by >= 0 && b[by][bx]) return 1;
        }
    return 0;
}

static int drop_row(int b[ROWS][COLS], int s[4][4], int px, int py) {
    int dy = 0;
    while (!collide(b, s, px, py + dy + 1)) dy++;
    return py + dy;
}




// 90-degree rotation of the n x n sub-grid about the grid midpoint. Because
// every piece is centered in its own grid, the occupied cells stay centered.
static void rotate(int s[4][4], int n) {
    int t[4][4] = {{0}};
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++)
            t[x][n-1-y] = s[y][x];
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++)
            s[y][x] = t[y][x];
}

// Column bx keeps every occupied cell of s inside the well.
static int colFits(int s[4][4], int bx) {
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            if (s[y][x] && (bx + x < 0 || bx + x >= COLS)) return 0;
    return 1;
}

// Remove every full row, shifting the stack down; returns how many went.
static int clearLines(int b[ROWS][COLS]) {
    int cleared = 0;
    for (int y = ROWS-1; y >= 0; y--) {
        int full = 1;
        for (int x = 0; x < COLS; x++) if (!b[y][x]) { full = 0; break; }
        if (full) {
            cleared++;
            for (int yy = y; yy > 0; yy--)
                memcpy(b[yy], b[yy-1], sizeof(b[0]));
            memset(b[0], 0, sizeof(b[0]));
            y++;
        }
    }
    return cleared;
}

// Easy mode's shape heuristic, for a board whose lines are already cleared:
// punishes a high pile (both the tallest column and total stack), rough column
// tops, and holes -- weighted by how far below the column surface they sit, so
// deeply buried empty cells cost far more than surface ones.
static int scoreBoard(int b[ROWS][COLS]) {
    int h[COLS], sum = 0, top = 0, buried = 0, bump = 0;
    for (int x = 0; x < COLS; x++) {
        int t = 0;
        while (t < ROWS && !b[t][x]) t++;
        h[x] = ROWS - t;
        sum += h[x];
        if (h[x] > top) top = h[x];
        for (int y = t; y < ROWS; y++)
            if (!b[y][x]) buried += y - t + 1;
    }
    for (int x = 1; x < COLS; x++) {
        int d = h[x] - h[x-1];
        if (d < 0) d = -d;
        bump += d;
    }
    return -top*12 - sum*5 - buried*8 - bump*4;
}

// Hard-drop `type` in orientation s at column bx onto a copy of b, lock it in,
// and clear whatever lines that fills. Returns the number of lines cleared;
// the resulting (already-cleared) board is written to out.
static int tryPlace(int b[ROWS][COLS], int s[4][4], int bx, int type, int out[ROWS][COLS]) {
    int fy = drop_row(b, s, bx, 0);
    memcpy(out, b, sizeof(int) * ROWS * COLS);
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            if (s[y][x]) out[fy+y][bx+x] = type + 1;
    return clearLines(out);
}

// Best score the piece `type` can produce from board b, given that `prior`
// lines were already cleared by the piece before it: search every orientation
// and column, hard-drop straight down, and score each final position as its
// own clears plus the earlier ones, over the shape heuristic. Ties lean toward
// the middle of the well.
static int bestNext(int b[ROWS][COLS], int type, int prior) {
    int bestSc = 0, have = 0;
    int s[4][4], nb[ROWS][COLS];
    memcpy(s, SHAPES[type], sizeof(s));
    for (int rot = 0; rot < 4; rot++) {
        for (int bx = -4; bx < COLS + 4; bx++) {
            if (!colFits(s, bx) || collide(b, s, bx, 0)) continue;
            int c = tryPlace(b, s, bx, type, nb);
            int sc = (prior + c) * 400 + scoreBoard(nb) - abs(bx - (COLS/2 - 1));
            if (!have || sc > bestSc) { have = 1; bestSc = sc; }
        }
        rotate(s, SIZES[type]);
    }
    if (!have) bestSc = prior * 400 + scoreBoard(b);
    return bestSc;
}

// Easy mode: scan every orientation and column the current piece could spawn
// in, assume the player hard-drops it straight down from there, then search
// the next piece's placements the same way from that result, and preset the
// current piece to the placement whose two-piece outcome scores best. Ties and
// near-ties lean toward the middle of the well.
static void easyBestDrop(int b[ROWS][COLS], Piece *p, int nextType) {
    int bestSc = 0, bestX = p->x, have = 0;
    int bestS[4][4], s[4][4];
    memcpy(s, p->s, sizeof(s));
    for (int rot = 0; rot < 4; rot++) {
        for (int bx = -4; bx < COLS + 4; bx++) {
            if (!colFits(s, bx) || collide(b, s, bx, 0)) continue;

            int nb[ROWS][COLS];
            int c1 = tryPlace(b, s, bx, p->type, nb);
            int sc = bestNext(nb, nextType, c1) - abs(bx - (COLS/2 - 1));
            if (!have || sc > bestSc) {
                have = 1; bestSc = sc; bestX = bx;
                memcpy(bestS, s, sizeof(bestS));
            }
        }
        rotate(s, SIZES[p->type]);
    }
    if (have) {
        memcpy(p->s, bestS, sizeof(p->s));
        p->x = bestX;
    }
}

// Pick the external monitor. On WSLg the built-in laptop panel and the
// external monitor are both named "rdp-N", so names alone can't tell them
// apart, but physical size can: the laptop panel is the smaller one. Native
// Linux built-ins (eDP/LVDS/...) are excluded by name.
static int SelectExternalMonitor(void) {
    int count = GetMonitorCount();
    if (count <= 1) return 0;

    int best = -1;
    long bestArea = -1;
    for (int i = 0; i < count; i++) {
        const char *name = GetMonitorName(i);
        if (name && (strstr(name, "eDP") || strstr(name, "LVDS") ||
                     strstr(name, "DSI") || strstr(name, "Built") ||
                     strstr(name, "Laptop") || strstr(name, "Internal"))) {
            continue;
        }
        int pw = GetMonitorPhysicalWidth(i);
        int ph = GetMonitorPhysicalHeight(i);
        long area = (pw > 0 && ph > 0) ? (long)pw * ph
                                       : (long)GetMonitorWidth(i) * GetMonitorHeight(i);
        if (area > bestArea) { bestArea = area; best = i; }
    }
    return (best < 0) ? 0 : best;
}

int main(void) {
    // WSLg's RAIL shell ignores SetWindowMonitor()/FLAG_FULLSCREEN_MODE and
    // pins fullscreen X11 windows to the main screen, so cover the chosen
    // monitor with an undecorated window (borderless-windowed fullscreen)
    // instead. Monitor geometry is only valid after InitWindow().
    SetConfigFlags(FLAG_WINDOW_UNDECORATED | FLAG_VSYNC_HINT);
    InitWindow(640, 480, "Tetris");

    int ext = SelectExternalMonitor();
    Vector2 mp = GetMonitorPosition(ext);
    SetWindowSize(GetMonitorWidth(ext), GetMonitorHeight(ext));
    SetWindowPosition((int)mp.x, (int)mp.y);

    SetTargetFPS(60);
    InitAudioDevice();

    Sound sMove   = LoadSound("resources/buttonfx.wav");
    Sound sRot    = LoadSound("resources/ping_send.wav");
    Sound sLand   = LoadSound("resources/coin.wav");
    Sound sClear  = LoadSound("resources/hit_splat.wav");
    Sound sOver   = LoadSound("resources/boom.wav");
    #if defined(PLATFORM_DESKTOP)
    Music bgm = LoadMusicStream("resources/country.mp3");
    #endif

    Piece cur = {0};
    int nextType = 0;
    int score = 0, lines = 0, level = 1, gameOver = 0, paused = 0;
    int easy = 0;
    int ghost = 1;
    float timer = 0.0f, gravity = 0.8f;
    float dropRepeat = 0.0f;

    #define SPAWN() do { \
        memcpy(cur.s, SHAPES[nextType], sizeof(cur.s)); cur.type = nextType; \
        cur.x = 3; cur.y = 0; \
        nextType = GetRandomValue(0, 6); \
        if (collide(board, cur.s, cur.x, cur.y)) gameOver = 1; \
        else if (easy) easyBestDrop(board, &cur, nextType); \
    } while (0)

    nextType = GetRandomValue(0, 6);
    SPAWN();

    #if defined(PLATFORM_DESKTOP)
    if (bgm.frameCount > 0) PlayMusicStream(bgm);
    #endif

    while (!WindowShouldClose()) {
        #if defined(PLATFORM_DESKTOP)
        if (bgm.frameCount > 0) UpdateMusicStream(bgm);
        #endif

        if (IsKeyPressed(KEY_E)) easy = !easy;
        if (IsKeyPressed(KEY_G)) ghost = !ghost;

        if (gameOver) {
            if (IsKeyPressed(KEY_ENTER)) {
                memset(board, 0, sizeof(board));
                score = lines = 0; level = 1; gameOver = 0;
                gravity = 0.8f;
                paused = 0;
                SPAWN();
            }
        } else if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_PAUSE)) {
            paused = !paused;
            #if defined(PLATFORM_DESKTOP)
            if (paused && bgm.frameCount > 0) PauseMusicStream(bgm);
            else if (!paused && bgm.frameCount > 0) ResumeMusicStream(bgm);
            #endif
        } else if (paused) {
            #if defined(PLATFORM_DESKTOP)
            if (bgm.frameCount > 0) UpdateMusicStream(bgm);
            #endif
        } else {
            if (IsKeyPressed(KEY_LEFT)  && !collide(board, cur.s, cur.x-1, cur.y)) { cur.x--; PlaySound(sMove); }
            if (IsKeyPressed(KEY_RIGHT) && !collide(board, cur.s, cur.x+1, cur.y)) { cur.x++; PlaySound(sMove); }
            if (IsKeyPressed(KEY_DOWN)  && !collide(board, cur.s, cur.x, cur.y+1)) { cur.y++; score += 1; }

            if (IsKeyPressed(KEY_UP)) {
                int tmp[4][4]; memcpy(tmp, cur.s, sizeof(tmp));
                rotate(tmp, SIZES[cur.type]);
                // DWIM kick: try the plain rotation first, then one square in
                // each direction from the current spot, preferring lateral and
                // upward shifts over dropping. Keeps rotates from failing when
                // a wall or the stack clips one corner.
                static const int kicks[][2] = {
                    {0,0}, {-1,0}, {1,0}, {0,-1}, {-1,-1}, {1,-1},
                    {0,1}, {-1,1}, {1,1}
                };
                for (int i = 0; i < 9; i++) {
                    int nx = cur.x + kicks[i][0], ny = cur.y + kicks[i][1];
                    if (!collide(board, tmp, nx, ny)) {
                        memcpy(cur.s, tmp, sizeof(tmp));
                        cur.x = nx; cur.y = ny;
                        PlaySound(sRot); break;
                    }
                }
            }

            // Hard drop. With easy mode on, holding SPACE keeps dropping the
            // piece that spawns after the lock, every 0.25s; otherwise a press
            // drops only the current piece.
            int doDrop = IsKeyPressed(KEY_SPACE);
            if (easy && IsKeyDown(KEY_SPACE)) {
                dropRepeat += GetFrameTime();
                if (dropRepeat >= 0.25f) { dropRepeat = 0.0f; doDrop = 1; }
            } else {
                dropRepeat = 0.0f;
            }
            if (doDrop) {
                while (!collide(board, cur.s, cur.x, cur.y+1)) { cur.y++; score += 2; }
                timer = 999.0f;
            }

            float interval = 1.0f / gravity;
            timer += GetFrameTime();
            if (timer >= interval) {
                timer = 0.0f;
                if (!collide(board, cur.s, cur.x, cur.y+1)) {
                    cur.y++;
                } else {
                    for (int y = 0; y < 4; y++)
                        for (int x = 0; x < 4; x++)
                            if (cur.s[y][x] && cur.y+y >= 0)
                                board[cur.y+y][cur.x+x] = cur.type + 1;

                    int cleared = 0;
                    for (int y = ROWS-1; y >= 0; y--) {
                        int full = 1;
                        for (int x = 0; x < COLS; x++) if (!board[y][x]) { full = 0; break; }
                        if (full) {
                            cleared++;
                            for (int yy = y; yy > 0; yy--)
                                memcpy(board[yy], board[yy-1], sizeof(board[0]));
                            memset(board[0], 0, sizeof(board[0]));
                            y++;
                        }
                    }
                    if (cleared) {
                        int pts[5] = {0, 100, 300, 500, 800};
                        score += pts[cleared] * level;
                        lines += cleared;
                        level = 1 + lines / 10;
                        gravity = 0.8f + (level-1) * 0.25f;
                        timer = 0.0f;
                        PlaySound(sClear);
                    } else {
                        PlaySound(sLand);
                    }
                    SPAWN();
                    if (gameOver) PlaySound(sOver);
                }
            }
        }

        BeginDrawing();
            ClearBackground((Color){18,18,24,255});

            int sw = GetRenderWidth(), sh = GetRenderHeight();
            int cell = sh / (ROWS + 1);
            if (cell > sw / (COLS + 7)) cell = sw / (COLS + 7);
            int boardW = COLS*cell, boardH = ROWS*cell;
            int ox = (sw - boardW - 6*cell) / 2;
            if (ox < cell) ox = cell;
            int oy = (sh - boardH) / 2;
            int px = ox + boardW + cell;
            int fs = cell * 3 / 4;
            if (fs < 10) fs = 10;

            DrawRectangleLinesEx((Rectangle){ox-2, oy-2, boardW+4, boardH+4}, 2, (Color){90,90,110,255});

            for (int y = 0; y < ROWS; y++)
                for (int x = 0; x < COLS; x++) {
                    if (!board[y][x]) continue;
                    DrawRectangle(ox+x*cell, oy+y*cell, cell-1, cell-1, COLORS[board[y][x]]);
                    DrawRectangleLines(ox+x*cell, oy+y*cell, cell-1, cell-1, (Color){0,0,0,120});
                }

            if (!gameOver) {
                int grounded = collide(board, cur.s, cur.x, cur.y+1);
                Color pc = COLORS[cur.type+1];
                float pulse = 0.0f;
                if (grounded) {
                    // Resting on the stack but not locked yet (heavy gravity
                    // still has to tick): pulse the fill toward white and blink
                    // a bounding-box halo so it reads as "about to lock".
                    pulse = 0.5f + 0.5f * sinf((float)GetTime() * 9.0f);
                    float mix = 0.30f + 0.45f * pulse;
                    pc.r = (unsigned char)(pc.r + (255 - pc.r) * mix);
                    pc.g = (unsigned char)(pc.g + (255 - pc.g) * mix);
                    pc.b = (unsigned char)(pc.b + (255 - pc.b) * mix);

                    int minx = 4, miny = 4, maxx = -1, maxy = -1;
                    for (int y = 0; y < 4; y++)
                        for (int x = 0; x < 4; x++)
                            if (cur.s[y][x]) {
                                if (x < minx) minx = x;
                                if (x > maxx) maxx = x;
                                if (y < miny) miny = y;
                                if (y > maxy) maxy = y;
                            }
                    Rectangle box = {
                        ox + (cur.x+minx)*cell, oy + (cur.y+miny)*cell,
                        (maxx-minx+1)*cell, (maxy-miny+1)*cell
                    };
                    Color halo = pulse > 0.5f ? (Color){255,255,255,255}
                                              : (Color){255,210,90,255};
                    DrawRectangleLinesEx(box, 3.0f, halo);
                }
                // Ghost: translucent projection of where a hard drop would land.
                // Drawn before the live piece so the piece sits on top when the
                // two coincide (piece already grounded). Toggleable with G.
                int gy = drop_row(board, cur.s, cur.x, cur.y);
                if (ghost && gy != cur.y) {
                    Color gc = COLORS[cur.type+1];
                    gc.a = 80;
                    Color go = COLORS[cur.type+1];
                    go.a = 170;
                    for (int y = 0; y < 4; y++)
                        for (int x = 0; x < 4; x++)
                            if (cur.s[y][x] && gy+y >= 0) {
                                DrawRectangle(ox+(cur.x+x)*cell, oy+(gy+y)*cell, cell-1, cell-1, gc);
                                DrawRectangleLines(ox+(cur.x+x)*cell, oy+(gy+y)*cell, cell-1, cell-1, go);
                            }
                }
                // Highlight rows that a hard drop in the current spot would
                // clear: the landing rows painted translucent white so a
                // pending line clear reads at a glance.
                for (int r = gy; r < gy + 4; r++) {
                    if (r < 0 || r >= ROWS) continue;
                    int full = 1;
                    for (int x = 0; x < COLS && full; x++) {
                        int filled = board[r][x];
                        for (int y = 0; y < 4 && !filled; y++) {
                            int bx = x - cur.x;
                            if (bx >= 0 && bx < 4 && cur.s[y][bx] && gy + y == r) filled = 1;
                        }
                        if (!filled) full = 0;
                    }
                    if (full)
                        DrawRectangle(ox, oy + r*cell, boardW, cell-1, (Color){255,255,255,70});
                }
                for (int y = 0; y < 4; y++)
                    for (int x = 0; x < 4; x++)
                        if (cur.s[y][x] && cur.y+y >= 0) {
                            DrawRectangle(ox+(cur.x+x)*cell, oy+(cur.y+y)*cell, cell-1, cell-1, pc);
                            DrawRectangleLines(ox+(cur.x+x)*cell, oy+(cur.y+y)*cell, cell-1, cell-1, (Color){0,0,0,120});
                        }
            }

            DrawText("NEXT", px, oy, fs, RAYWHITE);
            int ns = SIZES[nextType];
            for (int y = 0; y < ns; y++)
                for (int x = 0; x < ns; x++)
                    if (SHAPES[nextType][y][x])
                        DrawRectangle(px + x*cell, oy + fs + cell + y*cell, cell-1, cell-1, COLORS[nextType+1]);

            DrawText(TextFormat("SCORE %d", score), px, oy + boardH - fs*5, fs, RAYWHITE);
            DrawText(TextFormat("LINES %d", lines), px, oy + boardH - fs*4, fs, RAYWHITE);
            DrawText(TextFormat("LEVEL %d", level), px, oy + boardH - fs*3, fs, RAYWHITE);

            const char *modeTxt = easy ? "EASY: ON" : "EASY: OFF";
            Color modeC = easy ? (Color){110,235,140,255} : (Color){150,150,170,255};
            int mfs = fs * 2 / 3;
            if (mfs < 10) mfs = 10;
            int mw = MeasureText(modeTxt, mfs);
            int my = oy + 5*cell;
            DrawRectangle(px-4, my-4, mw+8, mfs+8, (Color){0,0,0,140});
            DrawRectangleLinesEx((Rectangle){px-4, my-4, mw+8, mfs+8}, 1.0f, modeC);
            DrawText(modeTxt, px, my, mfs, modeC);

            int cy = oy + 7*cell;
            DrawText("CONTROLS", px, cy, fs, (Color){180,180,200,255});
            static const char *ctl[][2] = {
                { "LEFT / RIGHT", "move" },
                { "DOWN",        "soft drop" },
                { "UP",          "rotate" },
                { "SPACE",       "hard drop" },
                { "P / PAUSE",   "pause" },
                { "E",           "easy mode" },
                { "G",           "ghost" },
                { "ENTER",       "restart" },
                { "ESC",         "quit" },
            };
            int cfs = fs * 2 / 3;
            if (cfs < 10) cfs = 10;
            int colX = 0;
            for (int i = 0; i < 9; i++) {
                int w = MeasureText(ctl[i][0], cfs);
                if (w > colX) colX = w;
            }
            colX += px + cfs;
            for (int i = 0; i < 9; i++) {
                int ly = cy + fs + i * (cfs + cfs/3);
                DrawText(ctl[i][0], px, ly, cfs, RAYWHITE);
                DrawText(ctl[i][1], colX, ly, cfs, (Color){160,160,180,255});
            }

            if (gameOver) {
                DrawRectangle(0, 0, sw, sh, (Color){0,0,0,180});
                int big = fs * 2;
                DrawText("GAME OVER", (sw - MeasureText("GAME OVER", big))/2, sh/2 - big, big, RAYWHITE);
                DrawText("ENTER to restart", (sw - MeasureText("ENTER to restart", fs))/2, sh/2 + fs, fs, RAYWHITE);
            }
        EndDrawing();
    }

    UnloadSound(sMove); UnloadSound(sRot); UnloadSound(sLand);
    UnloadSound(sClear); UnloadSound(sOver);
    #if defined(PLATFORM_DESKTOP)
    UnloadMusicStream(bgm);
    #endif
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
