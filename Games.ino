//////Games.ino



// =====================================================================
// DOOM LITE v4 - Raycasting FPS for the SMO Handheld
// Target: Raspberry Pi Pico 2 (RP2350) + ST7789 240x240 SPI TFT
//
// NEW in v4:
//   - Enemies carry pistols and SHOOT BACK at the player
//   - Each enemy has a fire-rate cooldown, engagement range check,
//     and a line-of-sight (LOS) wall test before shooting
//   - Enemy muzzle flash rendered on the sprite when it fires
//   - Hit flash on player screen (red vignette overlay) when damaged
//   - Player death: animated GAME OVER screen, SELECT to restart
//   - Enemy walk animation: arms swing left/right via frame counter
//   - Enemy raises gun arm when in attack mode (within range + LOS)
//   - Sprite body proportions improved: bigger head, clear gun arm
//   - Difficulty: enemy damage and fire rate scales with chosen enemy count
//   - Win screen when all enemies are dead
// =====================================================================

// NOTE: Assumes in main .ino:  tft, BTN_*, COLOR_*, waitRelease(), runLoop()

// ---------------------------------------------------------------------
// SCREEN
// ---------------------------------------------------------------------
#define SCR_W   240
#define SCR_H   240
#define HALF_H  120

// ---------------------------------------------------------------------
// COLOR PALETTE (RGB565)
// ---------------------------------------------------------------------
#define COL_SKY          0x10A2
#define COL_FLOOR        0x0841
#define COL_WALL_NEAR    0x07FF
#define COL_WALL_MID     0x045F
#define COL_WALL_FAR     0x0210
#define COL_WEAPON       0xDEDB
#define COL_WEAPON_DK    0x9CD3
#define COL_HUD          0xFFFF
#define COL_HUD_BG       0x0000
#define COL_ENEMY_BODY   0xF800   // red uniform
#define COL_ENEMY_SHADE  0xA000   // dark red shading
#define COL_ENEMY_HEAD   0xFDA0   // skin
#define COL_ENEMY_HEAD2  0xC8A0   // shadow side of head
#define COL_ENEMY_GUN    0x8410   // dark grey pistol
#define COL_ENEMY_MUZZLE 0xFFE0   // yellow enemy muzzle flash
#define COL_ENEMY_HELMET 0x4208   // dark helmet/cap
#define COL_ENEMY_VEST   0xC000   // darker vest trim
#define COL_ENEMY_ALERT  0xFFE0   // alert "!" indicator color
#define COL_MUZZLE       0xFFE0
#define COL_MINIMAP_BG   0x0000
#define COL_MINIMAP_WL   0x07FF
#define COL_MINIMAP_PL   0xFFFF
#define COL_MINIMAP_EN   0xF800
#define COL_BAR_HP       0xF800
#define COL_BAR_AMMO     0x07FF
#define COL_BAR_BG       0x2104
#define COL_DAMAGE_FLASH 0xF000   // dark red screen flash when hit
#define COL_WIN_TEXT     0xAFE5   // green win text

// Matrix menu palette
#define COL_MX_BG        0x0000
#define COL_MX_RAIN_HI   0xAFE5
#define COL_MX_RAIN_LO   0x0540
#define COL_MX_TEXT      0xAFE5
#define COL_MX_TEXT_DIM  0x0540
#define COL_MX_SELECT    0x0000
#define COL_MX_SELECT_BG 0xAFE5
#define COL_MX_BORDER    0xAFE5

// ---------------------------------------------------------------------
// FIXED POINT MATH
// ---------------------------------------------------------------------
#define FP_SHIFT      10
#define FP_ONE        (1 << FP_SHIFT)
#define FP_MUL(a,b)   ((int32_t)((int64_t)(a)*(b) >> FP_SHIFT))
#define FP_DIV(a,b)   ((int32_t)(((int64_t)(a) << FP_SHIFT) / (b)))

#define ANG_MAX   1024
#define ANG_MASK  (ANG_MAX - 1)
#define ANG_90    (ANG_MAX / 4)
#define ANG_180   (ANG_MAX / 2)
#define ANG_270   (ANG_MAX * 3 / 4)

// ---------------------------------------------------------------------
// WEAPONS (player)
// ---------------------------------------------------------------------
enum WeaponType { WEAPON_PISTOL = 0, WEAPON_RIFLE = 1, WEAPON_SHOTGUN = 2 };
#define WEAPON_COUNT 3

struct WeaponDef {
  const char* name;
  uint8_t fireCooldownFrames;
  int32_t coneAngle;
  int8_t  recoilFrames;
  uint8_t muzzleFlashFrames;
  int     startingAmmo;
};

static const WeaponDef weaponDefs[WEAPON_COUNT] = {
  { "PISTOL",  8,  ANG_MAX / 28, 6, 4,  99  },
  { "RIFLE",   3,  ANG_MAX / 16, 3, 3, 150  },
  { "SHOTGUN", 14, ANG_MAX / 9,  9, 5,  40  }
};

static WeaponType currentWeapon = WEAPON_PISTOL;
static uint8_t    fireCooldown  = 0;

static int32_t sinTable[ANG_MAX];
static int32_t cosTable[ANG_MAX];

// ---------------------------------------------------------------------
// MAPS  (2 maps, 16x16) -- "ROOMS" map removed by request
// ---------------------------------------------------------------------
#define MAP_SIZE  16
#define MAP_COUNT  2

static const uint8_t worldMap0[MAP_SIZE][MAP_SIZE] = {
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,1,1,0,0,0,0,0,0,0,0,1,1,0,1},
  {1,0,1,0,0,0,0,0,0,0,0,0,0,1,0,1},
  {1,0,0,0,0,1,1,0,0,1,1,0,0,0,0,1},
  {1,0,0,0,1,0,0,0,0,0,0,1,0,0,0,1},
  {1,0,0,0,1,0,0,0,0,0,0,1,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,1,0,0,0,0,0,0,1,0,0,0,1},
  {1,0,0,0,1,0,0,0,0,0,0,1,0,0,0,1},
  {1,0,0,0,0,1,1,0,0,1,1,0,0,0,0,1},
  {1,0,1,0,0,0,0,0,0,0,0,0,0,1,0,1},
  {1,0,1,1,0,0,0,0,0,0,0,0,1,1,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

static const uint8_t worldMap1[MAP_SIZE][MAP_SIZE] = {
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
  {1,0,0,0,1,0,0,0,0,0,1,0,0,0,0,1},
  {1,0,1,0,1,0,1,1,1,0,1,0,1,1,0,1},
  {1,0,1,0,0,0,0,0,1,0,0,0,0,1,0,1},
  {1,0,1,1,1,1,1,0,1,1,1,1,0,1,0,1},
  {1,0,0,0,0,0,1,0,0,0,0,1,0,1,0,1},
  {1,1,1,0,1,0,1,0,1,1,0,1,0,0,0,1},
  {1,0,0,0,1,0,0,0,1,0,0,0,1,1,0,1},
  {1,0,1,1,1,1,1,1,1,0,1,1,1,0,0,1},
  {1,0,0,0,0,0,0,0,1,0,0,0,1,0,1,1},
  {1,1,1,1,1,0,1,0,1,1,0,0,0,0,1,1},
  {1,0,0,0,1,0,1,0,0,0,0,1,1,0,0,1},
  {1,0,1,0,1,0,1,1,1,1,0,1,0,0,1,1},
  {1,0,1,0,0,0,0,0,0,1,0,0,0,0,0,1},
  {1,0,1,1,1,1,1,1,0,1,1,1,1,1,0,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

static const uint8_t* mapPointers[MAP_COUNT] = {
  &worldMap0[0][0], &worldMap1[0][0]
};
static const char* mapNames[MAP_COUNT] = { "ARENA", "MAZE" };
static int currentMap = 0;

static inline uint8_t mapAt(int cx, int cy)
{
  if (cx < 0 || cy < 0 || cx >= MAP_SIZE || cy >= MAP_SIZE) return 1;
  return mapPointers[currentMap][cy * MAP_SIZE + cx];
}

#define SPAWN_POINT_COUNT 12
static const uint8_t spawnX0[SPAWN_POINT_COUNT] = {1, 7,13, 1,13, 1,13, 7, 4,10, 4,10};
static const uint8_t spawnY0[SPAWN_POINT_COUNT] = {1, 1, 1, 7, 7,13,13,13, 4, 4,11,11};
static const uint8_t spawnX1[SPAWN_POINT_COUNT] = {1, 5,12, 3,11, 1, 7,13, 5, 9, 3,12};
static const uint8_t spawnY1[SPAWN_POINT_COUNT] = {1, 1, 1, 3, 3, 9, 7, 9,12,12,14,14};
static const uint8_t* spawnXtable[MAP_COUNT] = { spawnX0, spawnX1 };
static const uint8_t* spawnYtable[MAP_COUNT] = { spawnY0, spawnY1 };

static const float playerStartX[MAP_COUNT] = { 7.5f, 1.5f };
static const float playerStartY[MAP_COUNT] = { 2.5f, 1.5f };

// ---------------------------------------------------------------------
// DIFFICULTY PRESETS (extra menu choice)
// ---------------------------------------------------------------------
enum Difficulty { DIFF_EASY = 0, DIFF_NORMAL = 1, DIFF_HARD = 2, DIFF_NIGHTMARE = 3 };
#define DIFF_COUNT 4
static const char* diffNames[DIFF_COUNT] = { "EASY", "NORMAL", "HARD", "NIGHTMARE" };
// multipliers applied on top of enemy-count base difficulty
static const float diffDmgMul[DIFF_COUNT]   = { 0.65f, 1.0f, 1.3f, 1.7f };
static const float diffSpeedMul[DIFF_COUNT] = { 0.75f, 1.0f, 1.2f, 1.4f };
static const float diffCdMul[DIFF_COUNT]    = { 1.4f,  1.0f, 0.75f, 0.55f };
static Difficulty currentDifficulty = DIFF_NORMAL;

// ---------------------------------------------------------------------
// PLAYER STATE
// ---------------------------------------------------------------------
static int32_t playerX, playerY;
static int32_t playerAngle;
static int playerHealth = 100;
static int playerAmmo   = 99;
static int playerKills  = 0;
static bool playerDead  = false;

#define MOVE_SPEED        55
#define ROT_SPEED         18   // faster left/right turning (was 9)
#define PLAYER_RADIUS_FP  200

// Damage flash state (red screen tint when hit)
static uint8_t damageFlashTimer = 0;
#define DAMAGE_FLASH_FRAMES  8

// ---------------------------------------------------------------------
// ENEMY STATE  — now armed, smarter, and better looking!
// ---------------------------------------------------------------------
#define MAX_ENEMIES_CAP 12

// Enemy AI state machine
enum EnemyState {
  ESTATE_PATROL,    // wandering, hasn't seen player yet
  ESTATE_ALERT,     // just spotted player - brief reaction pause
  ESTATE_CHASE,     // moving toward player
  ESTATE_STRAFE,    // circling/flanking while in range but repositioning
  ESTATE_ATTACK,    // in range + LOS, raising gun to fire
  ESTATE_DEAD       // playing death animation
};

struct Enemy {
  int32_t   x, y;
  bool      alive;
  uint8_t   hitFlashTimer;    // white flash when HIT by player
  uint8_t   muzzleTimer;      // enemy muzzle flash frames
  uint16_t  fireCooldown;     // frames until enemy can fire again
  EnemyState state;
  uint8_t   walkFrame;        // 0-3 cycle for arm-swing animation
  uint8_t   walkTimer;        // ticks between walk frame advances
  uint8_t   attackWindup;     // frames the enemy holds gun up before shooting
  uint8_t   alertTimer;       // frames spent reacting before chasing
  int8_t    strafeDir;        // +1 / -1, which way it's circling
  uint8_t   strafeTimer;      // ticks until strafe direction may flip
  int32_t   patrolDx, patrolDy;  // patrol wander direction
  uint8_t   patrolTimer;         // ticks until direction change
  uint8_t   missedShots;      // consecutive misses, used to bias accuracy
};

static Enemy enemies[MAX_ENEMIES_CAP];
static int   activeEnemyCount = 6;

// Difficulty scaling: damage and fire-rate based on enemy count preset
// (more enemies = each one fires less often and deals less damage so
//  being surrounded is hard but not instant-death)
static int   enemyDamagePerShot  = 12;
static uint16_t enemyFireCooldownBase = 60;   // frames between shots
static int32_t enemySpeedFp = 10;

#define ENEMY_ENGAGE_RANGE_FP   (7 * FP_ONE)   // cells, fixed point (was 6 - spot player sooner)
#define ENEMY_FIRE_RANGE_FP     (5 * FP_ONE)   // must be closer to actually shoot
#define ENEMY_ATTACK_WINDUP     8               // frames gun is raised before shot (slightly faster reflexes)
#define ENEMY_ALERT_FRAMES      14              // brief "spotted you" reaction delay
#define ENEMY_SPEED             10

#define ENEMY_PRESET_COUNT 5
static const uint8_t enemyPresets[ENEMY_PRESET_COUNT] = { 3, 6, 9, 12, 12 };

// Difficulty params per preset: { damagePerShot, fireCooldown }
static const uint8_t  diffDamage[ENEMY_PRESET_COUNT]   = { 16, 12, 10, 8, 8 };
static const uint16_t diffCooldown[ENEMY_PRESET_COUNT] = { 85, 60, 45, 35, 28 };

// ---------------------------------------------------------------------
// RENDER BUFFERS
// ---------------------------------------------------------------------
static int32_t colDepth[SCR_W];
static uint8_t colSide[SCR_W];
static int16_t prevColTop[SCR_W];
static int16_t prevColBot[SCR_W];

// ---------------------------------------------------------------------
// PLAYER WEAPON ANIMATION
// ---------------------------------------------------------------------
static int8_t  weaponRecoil     = 0;
static uint8_t muzzleFlashTimer = 0;
static bool    lastSelectState  = HIGH;

// ---------------------------------------------------------------------
// HUD DIFF STATE
// ---------------------------------------------------------------------
static int prevHudHealth = -1;
static int prevHudAmmo   = -1;
static int prevHudKills  = -1;

// Global animation frame counter (incremented every game frame)
static uint32_t gameFrame = 0;

// ---------------------------------------------------------------------
// FORWARD DECLARATIONS
// ---------------------------------------------------------------------
void buildTrigTables();
void doomliteInit();
void handleDoomLite();
void doomliteInput();
void doomliteUpdateEnemies();
bool doomliteHasLOS(int32_t ax, int32_t ay, int32_t bx, int32_t by);
void doomliteRender();
void doomliteRenderWalls();
void doomliteRenderEnemySprites();
void doomliteDrawEnemySprite(int screenX, int32_t viewX, int sprH, int idx);
void doomliteRenderWeapon(bool fullDraw);
void doomliteRenderHud(bool fullDraw);
void doomliteRenderMinimap();
void doomliteRenderDamageFlash();
bool doomliteCanMove(int32_t nx, int32_t ny);
void doomliteFire();
void doomliteShowDeath();
void doomliteShowWin();
void doomlitePreGameMenu();
void doomliteRainInit();
void doomliteRainStep();
void doomliteDrawMenuPanel(int sel, int epi, WeaponType wp, int mi, int di, bool full);

// =====================================================================
// TRIG TABLES
// =====================================================================
void buildTrigTables()
{
  for (int i = 0; i < ANG_MAX; i++)
  {
    float r = (float)i * (2.0f * PI) / (float)ANG_MAX;
    sinTable[i] = (int32_t)(sinf(r) * FP_ONE);
    cosTable[i] = (int32_t)(cosf(r) * FP_ONE);
  }
}
static inline int32_t fpSin(int32_t a) { return sinTable[a & ANG_MASK]; }
static inline int32_t fpCos(int32_t a) { return cosTable[a & ANG_MASK]; }

// =====================================================================
// MATRIX RAIN  (now confined to a border strip so it never runs
// behind the menu panel / text)
// =====================================================================
#define RAIN_COLUMNS      24
#define RAIN_COL_SPACING  (SCR_W / RAIN_COLUMNS)
#define RAIN_TRAIL_LEN    4
#define GLYPH_H           8

static int16_t rainY[RAIN_COLUMNS];
static uint8_t rainSpeed[RAIN_COLUMNS];
static bool    rainColActive[RAIN_COLUMNS];   // false for columns hidden behind the panel
static const char rainGlyphChars[] = "01<>/\\{}[]*+=ABCDEF";
#define RAIN_GLYPH_COUNT (sizeof(rainGlyphChars) - 1)

// Forward-declared here so rain code can know where the panel sits
#define PANEL_X  20
#define PANEL_Y  40
#define PANEL_W  200
#define PANEL_H  155

void doomliteRainInit()
{
  tft.fillScreen(COL_MX_BG);
  for (int i = 0; i < RAIN_COLUMNS; i++)
  {
    rainY[i]     = (int16_t)random(-SCR_H, SCR_H);
    rainSpeed[i] = (uint8_t)random(2, 5);

    int colX = i * RAIN_COL_SPACING;
    // Disable columns whose X falls inside the panel's horizontal span
    // so rain only animates in the border margins around the panel.
    rainColActive[i] = !(colX + RAIN_COL_SPACING > PANEL_X - 2 &&
                          colX < PANEL_X + PANEL_W + 2);
  }
}

void doomliteRainStep()
{
  tft.setTextSize(1);
  for (int i = 0; i < RAIN_COLUMNS; i++)
  {
    if (!rainColActive[i]) continue;   // skip columns behind the panel

    int x = i * RAIN_COL_SPACING;
    int tailY = rainY[i] - (RAIN_TRAIL_LEN * GLYPH_H);
    if (tailY >= 0 && tailY < SCR_H)
      tft.fillRect(x, tailY, RAIN_COL_SPACING - 1, GLYPH_H, COL_MX_BG);

    int trailY = rainY[i] - GLYPH_H;
    if (trailY >= 0 && trailY < SCR_H)
    {
      tft.setTextColor(COL_MX_RAIN_LO, COL_MX_BG);
      tft.setCursor(x, trailY);
      tft.print(rainGlyphChars[random(0, RAIN_GLYPH_COUNT)]);
    }
    if (rainY[i] >= 0 && rainY[i] < SCR_H)
    {
      tft.setTextColor(COL_MX_RAIN_HI, COL_MX_BG);
      tft.setCursor(x, rainY[i]);
      tft.print(rainGlyphChars[random(0, RAIN_GLYPH_COUNT)]);
    }
    rainY[i] += rainSpeed[i];
    if (rainY[i] - (RAIN_TRAIL_LEN * GLYPH_H) > SCR_H)
    {
      rainY[i]     = (int16_t)random(-40, 0);
      rainSpeed[i] = (uint8_t)random(2, 5);
    }
  }
}

// =====================================================================
// PRE-GAME MENU PANEL (wire-frame, rain behind it)
// =====================================================================
#define MENU_ITEM_ENEMIES    0
#define MENU_ITEM_DIFFICULTY 1
#define MENU_ITEM_WEAPON     2
#define MENU_ITEM_MAP        3
#define MENU_ITEM_START      4
#define MENU_ITEM_COUNT      5

void doomliteDrawMenuPanel(int sel, int epi, WeaponType wp, int mi, int di, bool full)
{
  if (full)
  {
    tft.drawRect(PANEL_X,     PANEL_Y,     PANEL_W,     PANEL_H,     COL_MX_BORDER);
    tft.drawRect(PANEL_X + 2, PANEL_Y + 2, PANEL_W - 4, PANEL_H - 4, COL_MX_TEXT_DIM);

    tft.fillRect(PANEL_X + 4, PANEL_Y + 6, PANEL_W - 8, 16, COL_MX_BG);
    tft.setTextSize(2);
    tft.setTextColor(COL_MX_TEXT);
    tft.setCursor(PANEL_X + 22, PANEL_Y + 8);
    tft.print("DOOM LITE v4");

    tft.fillRect(PANEL_X + 4, PANEL_Y + PANEL_H + 4, PANEL_W - 8, 10, COL_MX_BG);
    tft.setTextSize(1);
    tft.setTextColor(COL_MX_TEXT_DIM);
    tft.setCursor(PANEL_X + 6, PANEL_Y + PANEL_H + 6);
    tft.print("UP/DN=SELECT  L/R=CHANGE");
  }

  tft.setTextSize(1);

  char eBuf[4]; itoa(enemyPresets[epi], eBuf, 10);

  struct { int ry; bool s; const char* lbl; const char* val; } rows[4] = {
    { PANEL_Y + 30, sel == MENU_ITEM_ENEMIES,    "ENEMIES: < ", eBuf                  },
    { PANEL_Y + 47, sel == MENU_ITEM_DIFFICULTY, "DIFF:    < ", diffNames[di]         },
    { PANEL_Y + 64, sel == MENU_ITEM_WEAPON,     "WEAPON:  < ", weaponDefs[wp].name   },
    { PANEL_Y + 81, sel == MENU_ITEM_MAP,        "MAP:     < ", mapNames[mi]          }
  };

  for (int r = 0; r < 4; r++)
  {
    tft.fillRect(PANEL_X + 5, rows[r].ry - 2, PANEL_W - 10, 14,
                 rows[r].s ? COL_MX_SELECT_BG : COL_MX_BG);
    tft.setTextColor(rows[r].s ? COL_MX_SELECT : COL_MX_TEXT);
    tft.setCursor(PANEL_X + 10, rows[r].ry);
    tft.print(rows[r].lbl);
    tft.print(rows[r].val);
    tft.print(" >");
  }

  // Separator
  tft.drawFastHLine(PANEL_X + 5, PANEL_Y + 24,  PANEL_W - 10, COL_MX_TEXT_DIM);
  tft.drawFastHLine(PANEL_X + 5, PANEL_Y + 100, PANEL_W - 10, COL_MX_TEXT_DIM);

  // START
  {
    int ry = PANEL_Y + 112;
    bool s = (sel == MENU_ITEM_START);
    tft.fillRect(PANEL_X + 5, ry - 2, PANEL_W - 10, 18, s ? COL_MX_SELECT_BG : COL_MX_BG);
    tft.setTextColor(s ? COL_MX_SELECT : COL_MX_TEXT);
    tft.setCursor(PANEL_X + 60, ry + 3);
    tft.print("> START GAME <");
  }

  // Small footer hint with current loadout summary
  tft.fillRect(PANEL_X + 5, PANEL_Y + 135, PANEL_W - 10, 12, COL_MX_BG);
  tft.setTextColor(COL_MX_TEXT_DIM);
  tft.setCursor(PANEL_X + 8, PANEL_Y + 137);
  tft.print(eBuf);
  tft.print(" enemies / ");
  tft.print(diffNames[di]);
}

void doomlitePreGameMenu()
{
  doomliteRainInit();
  int sel = 0, epi = 1, mi = 0, di = (int)DIFF_NORMAL;
  WeaponType wp = WEAPON_PISTOL;
  bool lastUp = HIGH, lastDown = HIGH, lastL = HIGH, lastR = HIGH, lastSel = HIGH;
  doomliteDrawMenuPanel(sel, epi, wp, mi, di, true);

  while (true)
  {
    doomliteRainStep();
    doomliteDrawMenuPanel(sel, epi, wp, mi, di, false);

    bool up = digitalRead(BTN_UP), dn = digitalRead(BTN_DOWN);
    bool lf = digitalRead(BTN_L),  rt = digitalRead(BTN_R);
    bool s  = digitalRead(BTN_SELECT);

    if (lastUp == HIGH && up == LOW) { sel = (sel - 1 + MENU_ITEM_COUNT) % MENU_ITEM_COUNT; }
    if (lastDown == HIGH && dn == LOW) { sel = (sel + 1) % MENU_ITEM_COUNT; }

    if (lastL == HIGH && lf == LOW) {
      if      (sel == MENU_ITEM_ENEMIES)    epi = (epi - 1 + ENEMY_PRESET_COUNT) % ENEMY_PRESET_COUNT;
      else if (sel == MENU_ITEM_DIFFICULTY) di  = (di - 1 + DIFF_COUNT) % DIFF_COUNT;
      else if (sel == MENU_ITEM_WEAPON)     wp  = (WeaponType)((wp - 1 + WEAPON_COUNT) % WEAPON_COUNT);
      else if (sel == MENU_ITEM_MAP)        mi  = (mi - 1 + MAP_COUNT)  % MAP_COUNT;
    }
    if (lastR == HIGH && rt == LOW) {
      if      (sel == MENU_ITEM_ENEMIES)    epi = (epi + 1) % ENEMY_PRESET_COUNT;
      else if (sel == MENU_ITEM_DIFFICULTY) di  = (di + 1) % DIFF_COUNT;
      else if (sel == MENU_ITEM_WEAPON)     wp  = (WeaponType)((wp + 1) % WEAPON_COUNT);
      else if (sel == MENU_ITEM_MAP)        mi  = (mi + 1) % MAP_COUNT;
    }

    bool startPressed = (lastSel == HIGH && s == LOW && sel == MENU_ITEM_START);
    bool backPressed  = (digitalRead(BTN_BACK) == LOW);

    if (startPressed || backPressed) {
      activeEnemyCount       = enemyPresets[epi];
      currentWeapon          = wp;
      currentMap             = mi;
      currentDifficulty      = (Difficulty)di;
      enemyDamagePerShot     = (int)(diffDamage[epi] * diffDmgMul[di]);
      enemyFireCooldownBase  = (uint16_t)(diffCooldown[epi] * diffCdMul[di]);
      enemySpeedFp           = (int32_t)(ENEMY_SPEED * diffSpeedMul[di]);
      if (enemySpeedFp < 4) enemySpeedFp = 4;
      return;
    }

    lastUp = up; lastDown = dn; lastL = lf; lastR = rt; lastSel = s;
    delay(20);
  }
}

// =====================================================================
// DEATH SCREEN - animated, waits for SELECT to restart
// =====================================================================
void doomliteShowDeath()
{
  // Red wash fade-in (3 passes of increasingly opaque rects)
  for (int p = 0; p < 3; p++)
  {
    tft.fillRect(0, 0, SCR_W, SCR_H - 22, COL_DAMAGE_FLASH);
    delay(80);
    tft.fillScreen(0x0000);
    delay(60);
  }
  tft.fillRect(0, 0, SCR_W, SCR_H, 0x2000);  // dark red bg

  // Skull icon (simple ASCII art rendered as rectangles)
  // Eyes
  tft.fillRect(82,  72, 18, 16, 0x0000);
  tft.fillRect(140, 72, 18, 16, 0x0000);
  // Skull outline (white)
  tft.drawRoundRect(65, 55, 110, 80, 12, COL_HUD);
  tft.drawRoundRect(66, 56, 108, 78, 12, COL_HUD);
  // Teeth
  for (int t = 0; t < 5; t++)
    tft.fillRect(72 + t * 20, 122, 14, 12, 0x0000);

  // "YOU DIED" text
  tft.setTextSize(3);
  tft.setTextColor(0xF800);
  tft.setCursor(38, 155);
  tft.print("YOU DIED");

  tft.setTextSize(1);
  tft.setTextColor(COL_HUD);
  tft.setCursor(50, 195);
  tft.print("Kills: ");
  tft.print(playerKills);
  tft.print("  /  ");
  tft.print(activeEnemyCount);

  tft.setCursor(45, 215);
  tft.print("SELECT = PLAY AGAIN");

  // Pulse "SELECT" text while waiting
  bool bright = true;
  while (digitalRead(BTN_SELECT) == HIGH && digitalRead(BTN_BACK) == HIGH)
  {
    tft.setTextColor(bright ? 0xFFE0 : 0x8410);
    tft.setCursor(45, 215);
    tft.print("SELECT = PLAY AGAIN");
    bright = !bright;
    delay(400);
  }
  waitRelease(BTN_SELECT);
}

// =====================================================================
// WIN SCREEN
// =====================================================================
void doomliteShowWin()
{
  tft.fillScreen(0x0020);   // very dark blue-green

  // Victory flash
  for (int f = 0; f < 4; f++)
  {
    tft.fillRect(0, 0, SCR_W, SCR_H, f % 2 == 0 ? COL_WIN_TEXT : 0x0020);
    delay(60);
  }
  tft.fillRect(0, 0, SCR_W, SCR_H, 0x0020);

  tft.setTextSize(3);
  tft.setTextColor(COL_WIN_TEXT);
  tft.setCursor(28, 80);
  tft.print("YOU WIN!");

  tft.setTextSize(2);
  tft.setTextColor(COL_HUD);
  tft.setCursor(30, 130);
  tft.print("ALL ENEMIES DEAD");

  tft.setTextSize(1);
  tft.setTextColor(0xAFE5);
  tft.setCursor(55, 165);
  tft.print("Kills: ");
  tft.print(playerKills);
  tft.print("  HP left: ");
  tft.print(playerHealth);

  tft.setTextColor(COL_HUD);
  tft.setCursor(45, 200);
  tft.print("SELECT = PLAY AGAIN");

  bool bright = true;
  while (digitalRead(BTN_SELECT) == HIGH && digitalRead(BTN_BACK) == HIGH)
  {
    tft.setTextColor(bright ? 0xFFE0 : 0x8410);
    tft.setCursor(45, 200);
    tft.print("SELECT = PLAY AGAIN");
    bright = !bright;
    delay(400);
  }
  waitRelease(BTN_SELECT);
}

// =====================================================================
// GAME INIT
// =====================================================================
void doomliteInit()
{
  buildTrigTables();
  doomlitePreGameMenu();

  playerX      = (int32_t)(playerStartX[currentMap] * FP_ONE);
  playerY      = (int32_t)(playerStartY[currentMap] * FP_ONE);
  playerAngle  = 0;
  playerHealth = 100;
  playerAmmo   = weaponDefs[currentWeapon].startingAmmo;
  playerKills  = 0;
  playerDead   = false;
  fireCooldown = 0;
  damageFlashTimer = 0;
  gameFrame    = 0;

  for (int i = 0; i < MAX_ENEMIES_CAP; i++)
  {
    if (i < activeEnemyCount)
    {
      int sp = i % SPAWN_POINT_COUNT;
      enemies[i].x             = (spawnXtable[currentMap][sp] * FP_ONE) + (FP_ONE / 2);
      enemies[i].y             = (spawnYtable[currentMap][sp] * FP_ONE) + (FP_ONE / 2);
      enemies[i].alive         = true;
      enemies[i].hitFlashTimer = 0;
      enemies[i].muzzleTimer   = 0;
      enemies[i].fireCooldown  = (uint16_t)(enemyFireCooldownBase + random(0, 40));
      enemies[i].state         = ESTATE_PATROL;
      enemies[i].walkFrame     = (uint8_t)(i % 4);
      enemies[i].walkTimer     = 0;
      enemies[i].attackWindup  = 0;
      enemies[i].alertTimer    = 0;
      enemies[i].strafeDir     = (random(0, 2) == 0) ? 1 : -1;
      enemies[i].strafeTimer   = (uint8_t)random(20, 50);
      enemies[i].patrolDx      = (random(0, 2) == 0) ? FP_ONE : -FP_ONE;
      enemies[i].patrolDy      = 0;
      enemies[i].patrolTimer   = (uint8_t)random(30, 90);
      enemies[i].missedShots   = 0;
    }
    else
    {
      enemies[i].alive = false;
      enemies[i].state = ESTATE_DEAD;
    }
  }

  weaponRecoil     = 0;
  muzzleFlashTimer = 0;
  lastSelectState  = HIGH;

  for (int i = 0; i < SCR_W; i++) { prevColTop[i] = -1; prevColBot[i] = -1; }
  prevHudHealth = -1; prevHudAmmo = -1; prevHudKills = -1;

  tft.fillRect(0, 0, SCR_W, HALF_H, COL_SKY);
  tft.fillRect(0, HALF_H, SCR_W, HALF_H - 22, COL_FLOOR);
  doomliteRenderWeapon(true);
  doomliteRenderHud(true);
}

// =====================================================================
// MAIN ENTRY (called by runLoop)
// =====================================================================
static bool doomliteInitialized = false;

void handleDoomLite()
{
  if (!doomliteInitialized)
  {
    doomliteInit();
    doomliteInitialized = true;
  }

  if (playerDead)
  {
    doomliteShowDeath();
    doomliteInitialized = false;   // restart from pre-game menu
    return;
  }

  // Check win condition
  int aliveCount = 0;
  for (int i = 0; i < activeEnemyCount; i++)
    if (enemies[i].alive) aliveCount++;
  if (aliveCount == 0)
  {
    doomliteShowWin();
    doomliteInitialized = false;
    return;
  }

  doomliteInput();
  doomliteUpdateEnemies();
  doomliteRender();
  gameFrame++;

  if (digitalRead(BTN_BACK) == LOW)
    doomliteInitialized = false;
}

// =====================================================================
// COLLISION
// =====================================================================
bool doomliteCanMove(int32_t nx, int32_t ny)
{
  int32_t r = PLAYER_RADIUS_FP;
  if (mapAt((nx - r) >> FP_SHIFT, (ny - r) >> FP_SHIFT) != 0) return false;
  if (mapAt((nx + r) >> FP_SHIFT, (ny - r) >> FP_SHIFT) != 0) return false;
  if (mapAt((nx - r) >> FP_SHIFT, (ny + r) >> FP_SHIFT) != 0) return false;
  if (mapAt((nx + r) >> FP_SHIFT, (ny + r) >> FP_SHIFT) != 0) return false;
  return true;
}

// =====================================================================
// LINE OF SIGHT CHECK  (DDA ray from A to B, returns true if clear)
// =====================================================================
bool doomliteHasLOS(int32_t ax, int32_t ay, int32_t bx, int32_t by)
{
  int32_t dx = bx - ax;
  int32_t dy = by - ay;

  // Step in small increments along the line; if any step hits a wall, no LOS
  int steps = 16;
  for (int s = 1; s < steps; s++)
  {
    int32_t tx = ax + (dx / steps) * s;
    int32_t ty = ay + (dy / steps) * s;
    if (mapAt(tx >> FP_SHIFT, ty >> FP_SHIFT) != 0) return false;
  }
  return true;
}

// =====================================================================
// INPUT HANDLING
// =====================================================================
void doomliteInput()
{
  if (digitalRead(BTN_L) == LOW)
    playerAngle = (playerAngle - ROT_SPEED) & ANG_MASK;
  if (digitalRead(BTN_R) == LOW)
    playerAngle = (playerAngle + ROT_SPEED) & ANG_MASK;

  int32_t dx = fpCos(playerAngle);
  int32_t dy = fpSin(playerAngle);

  if (digitalRead(BTN_UP) == LOW)
  {
    int32_t nx = playerX + FP_MUL(dx, MOVE_SPEED);
    int32_t ny = playerY + FP_MUL(dy, MOVE_SPEED);
    if (doomliteCanMove(nx, playerY)) playerX = nx;
    if (doomliteCanMove(playerX, ny)) playerY = ny;
  }
  if (digitalRead(BTN_DOWN) == LOW)
  {
    int32_t nx = playerX - FP_MUL(dx, MOVE_SPEED);
    int32_t ny = playerY - FP_MUL(dy, MOVE_SPEED);
    if (doomliteCanMove(nx, playerY)) playerX = nx;
    if (doomliteCanMove(playerX, ny)) playerY = ny;
  }

  bool sel         = digitalRead(BTN_SELECT);
  bool pressEdge   = (lastSelectState == HIGH && sel == LOW);
  bool autoFire    = (sel == LOW && currentWeapon == WEAPON_RIFLE);
  if (fireCooldown > 0) fireCooldown--;
  if ((pressEdge || autoFire) && fireCooldown == 0)
  {
    doomliteFire();
    fireCooldown = weaponDefs[currentWeapon].fireCooldownFrames;
  }
  lastSelectState = sel;

  if (weaponRecoil > 0)     weaponRecoil--;
  if (muzzleFlashTimer > 0) muzzleFlashTimer--;
  if (damageFlashTimer > 0) damageFlashTimer--;
}

// =====================================================================
// PLAYER FIRE
// =====================================================================
void doomliteFire()
{
  if (playerAmmo <= 0) return;
  const WeaponDef &wd = weaponDefs[currentWeapon];
  playerAmmo--;
  weaponRecoil     = wd.recoilFrames;
  muzzleFlashTimer = wd.muzzleFlashFrames;

  // Shotgun fires a spread of "pellets" (multiple angle checks) so it can
  // hit more than one enemy at once if they're bunched up.
  int pelletCount = (currentWeapon == WEAPON_SHOTGUN) ? 3 : 1;
  bool killedAny = false;

  for (int p = 0; p < pelletCount; p++)
  {
    int32_t bestDist = 0x7FFFFFFF;
    int     bestIdx  = -1;

    for (int i = 0; i < activeEnemyCount; i++)
    {
      if (!enemies[i].alive) continue;
      int32_t ex = enemies[i].x - playerX;
      int32_t ey = enemies[i].y - playerY;
      int32_t distSq = FP_MUL(ex, ex) + FP_MUL(ey, ey);
      float angRad = atan2f((float)ey, (float)ex);
      int32_t angToE = (int32_t)(angRad * (float)ANG_MAX / (2.0f * PI)) & ANG_MASK;
      int32_t angDiff = (angToE - playerAngle) & ANG_MASK;
      if (angDiff > ANG_MAX / 2) angDiff -= ANG_MAX;
      if (abs(angDiff) <= wd.coneAngle && distSq < bestDist)
      {
        bestDist = distSq;
        bestIdx  = i;
      }
    }

    if (bestIdx >= 0)
    {
      enemies[bestIdx].alive         = false;
      enemies[bestIdx].state         = ESTATE_DEAD;
      enemies[bestIdx].hitFlashTimer = 12;
      playerKills++;
      killedAny = true;
    }
  }
  (void)killedAny;
}

// =====================================================================
// ENEMY AI + SHOOTING  — smarter behaviour:
//   * brief "alert" reaction pause when first spotting the player
//   * strafes/circles instead of always walking straight at you
//   * remembers consecutive misses and gets slightly more accurate
//     the longer it has had a clean shot (keeps fights from dragging)
//   * avoids walking into other enemies (simple separation)
// =====================================================================
void doomliteUpdateEnemies()
{
  for (int i = 0; i < activeEnemyCount; i++)
  {
    Enemy &e = enemies[i];
    if (!e.alive) continue;

    // Distance to player
    int32_t dx = playerX - e.x;
    int32_t dy = playerY - e.y;
    int32_t distSq = FP_MUL(dx, dx) + FP_MUL(dy, dy);
    bool inEngageRange = (distSq < FP_MUL(ENEMY_ENGAGE_RANGE_FP, ENEMY_ENGAGE_RANGE_FP));
    bool inFireRange   = (distSq < FP_MUL(ENEMY_FIRE_RANGE_FP,   ENEMY_FIRE_RANGE_FP));

    // Decay timers
    if (e.hitFlashTimer > 0) e.hitFlashTimer--;
    if (e.muzzleTimer   > 0) e.muzzleTimer--;
    if (e.fireCooldown  > 0) e.fireCooldown--;

    // Walk animation tick
    e.walkTimer++;
    if (e.walkTimer >= 8)
    {
      e.walkTimer = 0;
      e.walkFrame = (e.walkFrame + 1) % 4;
    }

    // Simple separation: nudge away from any other enemy that's too close,
    // so enemies don't clump into a single overlapping blob.
    int32_t sepX = 0, sepY = 0;
    for (int j = 0; j < activeEnemyCount; j++)
    {
      if (j == i || !enemies[j].alive) continue;
      int32_t odx = e.x - enemies[j].x;
      int32_t ody = e.y - enemies[j].y;
      int32_t odSq = FP_MUL(odx, odx) + FP_MUL(ody, ody);
      if (odSq < FP_MUL(FP_ONE, FP_ONE) && odSq > 0)
      {
        sepX += (odx > 0) ? 2 : -2;
        sepY += (ody > 0) ? 2 : -2;
      }
    }

    // --- STATE MACHINE ---
    switch (e.state)
    {
      case ESTATE_PATROL:
      {
        // Random wander
        e.patrolTimer--;
        if (e.patrolTimer == 0)
        {
          int dir = random(0, 4);
          e.patrolDx = (dir == 0) ? enemySpeedFp : (dir == 1) ? -enemySpeedFp : 0;
          e.patrolDy = (dir == 2) ? enemySpeedFp : (dir == 3) ? -enemySpeedFp : 0;
          e.patrolTimer = (uint8_t)random(20, 60);
        }

        int32_t nx = e.x + e.patrolDx + sepX;
        int32_t ny = e.y + e.patrolDy + sepY;
        if (mapAt(nx >> FP_SHIFT, e.y >> FP_SHIFT) == 0) e.x = nx;
        if (mapAt(e.x >> FP_SHIFT, ny >> FP_SHIFT) == 0) e.y = ny;

        if (inEngageRange && doomliteHasLOS(e.x, e.y, playerX, playerY))
        {
          e.state      = ESTATE_ALERT;
          e.alertTimer = ENEMY_ALERT_FRAMES;
        }
        break;
      }

      case ESTATE_ALERT:
      {
        // Brief "I see you!" pause before reacting - makes spotting feel
        // readable instead of instant, while still being fast.
        if (e.alertTimer > 0) e.alertTimer--;
        if (e.alertTimer == 0)
        {
          if (inEngageRange && doomliteHasLOS(e.x, e.y, playerX, playerY))
            e.state = ESTATE_CHASE;
          else
            e.state = ESTATE_PATROL;
        }
        break;
      }

      case ESTATE_CHASE:
      {
        // Move toward player
        int32_t stepX = 0, stepY = 0;
        if      (dx >  FP_ONE / 8) stepX =  enemySpeedFp;
        else if (dx < -FP_ONE / 8) stepX = -enemySpeedFp;
        if      (dy >  FP_ONE / 8) stepY =  enemySpeedFp;
        else if (dy < -FP_ONE / 8) stepY = -enemySpeedFp;

        int32_t nx = e.x + stepX + sepX;
        int32_t ny = e.y + stepY + sepY;
        if (mapAt(nx >> FP_SHIFT, e.y >> FP_SHIFT) == 0) e.x = nx;
        if (mapAt(e.x >> FP_SHIFT, ny >> FP_SHIFT) == 0) e.y = ny;

        if (inFireRange && doomliteHasLOS(e.x, e.y, playerX, playerY))
        {
          // Sometimes strafe a bit before settling into attack, so enemies
          // don't all just stand still in a line.
          if (random(0, 3) == 0)
          {
            e.state       = ESTATE_STRAFE;
            e.strafeTimer = (uint8_t)random(10, 24);
          }
          else
          {
            e.state        = ESTATE_ATTACK;
            e.attackWindup = ENEMY_ATTACK_WINDUP;
          }
        }
        else if (!inEngageRange)
        {
          e.state = ESTATE_PATROL;
        }
        break;
      }

      case ESTATE_STRAFE:
      {
        // Circle sideways around the player (perpendicular to the line
        // to the player) while keeping roughly the same distance.
        // Perpendicular direction to (dx,dy) is (-dy, dx) or (dy, -dx).
        int32_t mag = (int32_t)sqrtf((float)distSq);
        if (mag < 1) mag = 1;
        int32_t ndx = FP_DIV(dx, mag);
        int32_t ndy = FP_DIV(dy, mag);
        int32_t perpX = -ndy;
        int32_t perpY =  ndx;

        int32_t stepX = FP_MUL(perpX, enemySpeedFp * e.strafeDir);
        int32_t stepY = FP_MUL(perpY, enemySpeedFp * e.strafeDir);

        int32_t nx = e.x + stepX + sepX;
        int32_t ny = e.y + stepY + sepY;
        if (mapAt(nx >> FP_SHIFT, e.y >> FP_SHIFT) == 0) e.x = nx;
        else e.strafeDir = -e.strafeDir;   // bounce off wall, flip direction
        if (mapAt(e.x >> FP_SHIFT, ny >> FP_SHIFT) == 0) e.y = ny;
        else e.strafeDir = -e.strafeDir;

        if (e.strafeTimer > 0) e.strafeTimer--;

        if (!inEngageRange)
        {
          e.state = ESTATE_PATROL;
        }
        else if (!doomliteHasLOS(e.x, e.y, playerX, playerY))
        {
          e.state = ESTATE_CHASE;
        }
        else if (e.strafeTimer == 0)
        {
          e.state        = ESTATE_ATTACK;
          e.attackWindup = ENEMY_ATTACK_WINDUP;
        }
        break;
      }

      case ESTATE_ATTACK:
      {
        // Hold position, face player, windup then fire
        if (e.attackWindup > 0)
        {
          e.attackWindup--;
        }
        else if (e.fireCooldown == 0)
        {
          if (doomliteHasLOS(e.x, e.y, playerX, playerY))
          {
            // Base miss chance ~30%, but an enemy that's whiffed several
            // shots in a row gets a touch more accurate (feels smarter,
            // avoids endless harmless spray).
            int missChance = 3 - min((int)e.missedShots / 2, 2);
            if (missChance < 1) missChance = 1;

            if (random(0, 10) >= missChance)
            {
              playerHealth -= enemyDamagePerShot;
              damageFlashTimer = DAMAGE_FLASH_FRAMES;
              e.missedShots = 0;
              if (playerHealth <= 0)
              {
                playerHealth = 0;
                playerDead   = true;
              }
            }
            else
            {
              e.missedShots++;
            }
            e.muzzleTimer  = 5;
            e.fireCooldown = enemyFireCooldownBase + (uint16_t)random(0, 30);
          }

          // After firing, occasionally reposition instead of turtling
          if (random(0, 4) == 0)
          {
            e.state       = ESTATE_STRAFE;
            e.strafeTimer = (uint8_t)random(8, 18);
          }
          else if (!inFireRange)
          {
            e.state = ESTATE_CHASE;
          }
        }

        if (!inEngageRange)
          e.state = ESTATE_PATROL;
        break;
      }

      case ESTATE_DEAD:
        // Already handled by alive == false
        break;
    }
  }
}

// =====================================================================
// RENDER MASTER
// =====================================================================
#define FOV_ANGLE (ANG_MAX / 4)

void doomliteRender()
{
  doomliteRenderWalls();
  doomliteRenderEnemySprites();
  doomliteRenderWeapon(false);
  doomliteRenderHud(false);
  doomliteRenderMinimap();
  if (damageFlashTimer > 0) doomliteRenderDamageFlash();
}

// =====================================================================
// DAMAGE FLASH OVERLAY
// =====================================================================
void doomliteRenderDamageFlash()
{
  // Red vignette around the edge of the screen - not a full cover so
  // the player can still see what's happening.
  uint8_t alpha = damageFlashTimer * 4;   // 0-32 border width
  int bw = (int)alpha / 4 + 1;
  if (bw > 12) bw = 12;

  tft.fillRect(0,         0,         SCR_W,       bw,         COL_DAMAGE_FLASH);
  tft.fillRect(0,         SCR_H - 22 - bw, SCR_W, bw,         COL_DAMAGE_FLASH);
  tft.fillRect(0,         0,         bw,          SCR_H - 22, COL_DAMAGE_FLASH);
  tft.fillRect(SCR_W - bw, 0,        bw,          SCR_H - 22, COL_DAMAGE_FLASH);

  // Invalidate column edges so wall render repaints them next frame
  for (int c = 0; c < bw && c < SCR_W; c++) { prevColTop[c] = -1; prevColBot[c] = -1; }
  for (int c = SCR_W - bw; c < SCR_W; c++)  { prevColTop[c] = -1; prevColBot[c] = -1; }
}

// =====================================================================
// WALL RAYCASTING (DDA)
// =====================================================================
void doomliteRenderWalls()
{
  int32_t startAngle = playerAngle - (FOV_ANGLE / 2);

  for (int col = 0; col < SCR_W; col++)
  {
    int32_t rayAngle = (startAngle + (col * FOV_ANGLE) / SCR_W) & ANG_MASK;
    int32_t rdx = fpCos(rayAngle); if (rdx == 0) rdx = 1;
    int32_t rdy = fpSin(rayAngle); if (rdy == 0) rdy = 1;

    int mapX = playerX >> FP_SHIFT;
    int mapY = playerY >> FP_SHIFT;

    int32_t ddx = abs(FP_DIV(FP_ONE, rdx));
    int32_t ddy = abs(FP_DIV(FP_ONE, rdy));
    int     sx, sy;
    int32_t sdx, sdy;

    if (rdx < 0) { sx = -1; sdx = FP_MUL(playerX - (mapX * FP_ONE), ddx); }
    else         { sx =  1; sdx = FP_MUL((mapX * FP_ONE) + FP_ONE - playerX, ddx); }
    if (rdy < 0) { sy = -1; sdy = FP_MUL(playerY - (mapY * FP_ONE), ddy); }
    else         { sy =  1; sdy = FP_MUL((mapY * FP_ONE) + FP_ONE - playerY, ddy); }

    uint8_t side = 0;
    for (int saf = 0; saf < 32; saf++)
    {
      if (sdx < sdy) { sdx += ddx; mapX += sx; side = 0; }
      else           { sdy += ddy; mapY += sy; side = 1; }
      if (mapAt(mapX, mapY) != 0) break;
    }

    int32_t perp = (side == 0) ? (sdx - ddx) : (sdy - ddy);
    if (perp < 1) perp = 1;
    colDepth[col] = perp;
    colSide[col]  = side;

    int32_t lh = FP_DIV(FP_ONE * HALF_H, perp) >> FP_SHIFT;
    if (lh > SCR_H)      lh = SCR_H;
    if (lh < 1)          lh = 1;

    int ds = HALF_H - (lh / 2); if (ds < 0)        ds = 0;
    int de = HALF_H + (lh / 2); if (de >= SCR_H-22) de = SCR_H - 23;

    int32_t  dc = perp >> FP_SHIFT;
    uint16_t wc;
    if      (dc <= 2) wc = COL_WALL_NEAR;
    else if (dc <= 5) wc = COL_WALL_MID;
    else              wc = COL_WALL_FAR;
    if (side == 1)
    {
      if      (wc == COL_WALL_NEAR) wc = COL_WALL_MID;
      else if (wc == COL_WALL_MID)  wc = COL_WALL_FAR;
    }

    int16_t pt = prevColTop[col], pb = prevColBot[col];
    if (pt >= 0)
    {
      if (ds > pt) tft.drawFastVLine(col, pt, ds - pt, COL_SKY);
      if (de < pb) tft.drawFastVLine(col, de + 1, pb - de, COL_FLOOR);
    }
    else
    {
      tft.drawFastVLine(col, 0, ds, COL_SKY);
      tft.drawFastVLine(col, de + 1, (SCR_H - 22) - de - 1, COL_FLOOR);
    }

    tft.drawFastVLine(col, ds, (de - ds) + 1, wc);
    prevColTop[col] = ds;
    prevColBot[col] = de;
  }
}

// =====================================================================
// ENEMY SPRITE RENDERING  — richer animated humanoid with pistol,
// helmet, vest trim, and a state-aware alert indicator.
// =====================================================================

// Draw one enemy's sprite at a given screen column and depth.
// sprH is the total sprite pixel height. Detailed body-part breakdown:
//   Helmet: thin dark cap above the head
//   Head  : top 16%  — round, skin tone, one dark eye stripe
//   Torso : next 30% — red uniform with a darker vest-trim stripe
//   Gun arm: hangs off the right side at torso height
//           In ATTACK/STRAFE state the arm is raised above torso (aim pose)
//   Legs  : next 28% — two separate columns, dark trousers
//   Feet  : bottom 8% — slightly lighter for boots
// An "!" alert mark flashes briefly above an enemy's head the instant
// it spots the player (ESTATE_ALERT), so the player gets visual warning.
void doomliteDrawEnemySprite(int screenX, int32_t viewX, int sprH, int idx)
{
  Enemy &e = enemies[idx];
  bool flashing = (e.hitFlashTimer > 0);

  int sprW    = sprH * 11 / 20;    // slightly narrower than tall
  int sprTop  = HALF_H - (sprH / 2);
  int sprBot  = sprTop + sprH;
  int sprLeft = screenX - (sprW / 2);
  int sprRight= screenX + (sprW / 2);

  if (sprRight < 0 || sprLeft >= SCR_W) return;
  int drawL = max(sprLeft,  0);
  int drawR = min(sprRight, SCR_W - 1);

  // Body-part Y boundaries
  int helmH  = sprH * 5  / 100;
  int headH  = sprH * 16 / 100;
  int torsoH = sprH * 30 / 100;
  int legsH  = sprH * 28 / 100;
  // feet occupy the rest

  int helmTop  = sprTop;
  int helmBot  = helmTop + helmH;
  int headTop  = helmBot;
  int headBot  = headTop + headH;
  int torsoTop = headBot;
  int torsoBot = torsoTop + torsoH;
  int legsTop  = torsoBot;
  int legsBot  = legsTop + legsH;
  int feetBot  = sprBot;

  // Colors - flash white when hit
  uint16_t cHelm  = flashing ? 0xEEEE : COL_ENEMY_HELMET;
  uint16_t cHead  = flashing ? 0xFFFF : COL_ENEMY_HEAD;
  uint16_t cHead2 = flashing ? 0xFFFF : COL_ENEMY_HEAD2;
  uint16_t cBody  = flashing ? 0xFFFF : COL_ENEMY_BODY;
  uint16_t cShade = flashing ? 0xCCCC : COL_ENEMY_SHADE;
  uint16_t cVest  = flashing ? 0xDDDD : COL_ENEMY_VEST;
  uint16_t cGun   = flashing ? 0xFFFF : COL_ENEMY_GUN;
  uint16_t cPants = flashing ? 0x8888 : 0x2104;   // dark navy trousers
  uint16_t cBoots = flashing ? 0x9999 : 0x4208;   // dark brown boots

  bool inAttack  = (e.state == ESTATE_ATTACK);
  bool inStrafe  = (e.state == ESTATE_STRAFE);
  bool isAlert   = (e.state == ESTATE_ALERT);

  // Column-per-column drawing
  for (int sx = drawL; sx <= drawR; sx++)
  {
    if (colDepth[sx] < viewX) continue;   // wall is closer

    int lx = sx - sprLeft;
    bool isLeft   = (lx <  sprW / 3);
    bool isRight  = (lx >= 2 * sprW / 3);
    bool isCenter = !isLeft && !isRight;

    // HELMET
    if (helmBot > helmTop)
      tft.drawFastVLine(sx, helmTop, helmBot - helmTop, cHelm);

    // HEAD
    if (headBot > headTop)
    {
      uint16_t hc = isRight ? cHead2 : cHead;
      tft.drawFastVLine(sx, headTop, headBot - headTop, hc);
      int eyeY = headTop + headH * 4 / 10;
      if (eyeY >= headTop && eyeY < headBot && isCenter)
        tft.drawPixel(sx, eyeY, 0x0000);
    }

    // TORSO with a vest-trim stripe down the center
    if (torsoBot > torsoTop)
    {
      uint16_t tc = (isLeft || isRight) ? cShade : cVest;
      tft.drawFastVLine(sx, torsoTop, torsoBot - torsoTop, tc);
      if (!isCenter)
      {
        // small accent band so torso isn't a flat block
        int bandY = torsoTop + torsoH * 2 / 3;
        tft.drawPixel(sx, bandY, cBody);
      }
    }

    // LEGS — two separate columns (gap in true-center), slight bend look
    if (legsBot > legsTop)
    {
      if (!isCenter)
        tft.drawFastVLine(sx, legsTop, legsBot - legsTop, cPants);
    }

    // FEET / BOOTS
    if (feetBot > legsBot && !isCenter)
      tft.drawFastVLine(sx, legsBot, feetBot - legsBot, cBoots);

    prevColTop[sx] = -1;
    prevColBot[sx] = -1;
  }

  // --- ALERT INDICATOR ("!" mark above head when just spotted) ---
  if (isAlert && screenX >= 2 && screenX < SCR_W - 2 && helmTop - 10 >= 0)
  {
    if (colDepth[screenX] >= viewX)
    {
      tft.fillRect(screenX - 1, helmTop - 10, 2, 6, COL_ENEMY_ALERT);
      tft.fillRect(screenX - 1, helmTop - 3,  2, 2, COL_ENEMY_ALERT);
    }
  }

  // --- GUN ARM (drawn outside the per-column loop for simplicity) ---
  bool swingUp  = (e.walkFrame == 1 || e.walkFrame == 2);

  int armOriginX = sprRight;
  if (armOriginX < 0 || armOriginX >= SCR_W) goto skipGun;
  if (armOriginX < SCR_W && colDepth[armOriginX] < viewX) goto skipGun;

  {
    int armLen   = sprW / 2;
    int armThick = max(2, sprH / 16);

    int gunArmY;
    if (inAttack || inStrafe)
      gunArmY = torsoTop + headH / 2;    // raised, aiming roughly at mid-head height
    else
      gunArmY = torsoTop + torsoH / 2 + (swingUp ? -(torsoH / 5) : (torsoH / 5));

    int ax = armOriginX;
    int aw = armLen;
    if (ax + aw >= SCR_W) aw = SCR_W - ax - 1;
    if (aw > 0)
      tft.fillRect(ax, gunArmY, aw, armThick, cGun);

    int bx = ax + aw;
    if (bx < SCR_W - 2)
      tft.fillRect(bx, gunArmY, 4, max(1, armThick - 1), cGun);

    if (e.muzzleTimer > 0)
    {
      int mx = bx + 4;
      if (mx < SCR_W - 3 && mx >= 0)
      {
        tft.fillRect(mx, gunArmY - 3, 6, armThick + 6, COL_ENEMY_MUZZLE);
        tft.fillRect(mx + 1, gunArmY - 1, 3, armThick + 2, 0xFFFF);
      }
    }
  }

skipGun:;
}

// Sort enemies back-to-front by viewX depth so closer enemies
// paint over further ones (simple insertion sort, N≤12 is fine).
static int enemySortOrder[MAX_ENEMIES_CAP];

void doomliteRenderEnemySprites()
{
  // Build sort array of alive enemies
  int n = 0;
  for (int i = 0; i < activeEnemyCount; i++)
    if (enemies[i].alive) enemySortOrder[n++] = i;

  // Sort by distance descending (draw far ones first)
  for (int a = 0; a < n - 1; a++)
    for (int b = a + 1; b < n; b++)
    {
      int ia = enemySortOrder[a], ib = enemySortOrder[b];
      int32_t dxa = playerX - enemies[ia].x, dya = playerY - enemies[ia].y;
      int32_t dxb = playerX - enemies[ib].x, dyb = playerY - enemies[ib].y;
      int32_t da = FP_MUL(dxa,dxa) + FP_MUL(dya,dya);
      int32_t db = FP_MUL(dxb,dxb) + FP_MUL(dyb,dyb);
      if (da < db) { int t = enemySortOrder[a]; enemySortOrder[a] = enemySortOrder[b]; enemySortOrder[b] = t; }
    }

  for (int si = 0; si < n; si++)
  {
    int i = enemySortOrder[si];
    Enemy &e = enemies[i];

    int32_t relX = e.x - playerX;
    int32_t relY = e.y - playerY;

    int32_t cosA = fpCos((-playerAngle) & ANG_MASK);
    int32_t sinA = fpSin((-playerAngle) & ANG_MASK);
    int32_t viewX = FP_MUL(relX, cosA) - FP_MUL(relY, sinA);
    int32_t viewY = FP_MUL(relX, sinA) + FP_MUL(relY, cosA);

    if (viewX <= (FP_ONE / 2)) continue;

    int32_t projScale = (SCR_W / 2) * FP_ONE;
    int32_t screenXfp = (int32_t)((SCR_W / 2) * FP_ONE)
                      + FP_MUL(FP_DIV(viewY, viewX), projScale);
    int screenX = screenXfp >> FP_SHIFT;

    // Sprite height: scale by depth, 1.6x boost so enemies are clearly visible
    int32_t sprH = (FP_DIV(FP_ONE * HALF_H, viewX) >> FP_SHIFT) * 8 / 5;
    if (sprH > 120) sprH = 120;
    if (sprH < 8)   continue;

    doomliteDrawEnemySprite(screenX, viewX, (int)sprH, i);
  }
}

// =====================================================================
// PLAYER WEAPON RENDER
// =====================================================================
#define WEAPON_BASE_Y  (SCR_H - 68)
#define WEAPON_X       (SCR_W / 2 - 18)

void doomliteRenderWeapon(bool fullDraw)
{
  static int8_t     lastRecoil = 0;
  static bool       lastFlash  = false;
  static WeaponType lastType   = WEAPON_PISTOL;

  int8_t recoilOff = (weaponRecoil > 0) ? -(weaponRecoil * 2) : 0;
  bool   flash     = (muzzleFlashTimer > 0);

  if (!fullDraw && recoilOff == lastRecoil && flash == lastFlash && currentWeapon == lastType)
    return;

  int wy = WEAPON_BASE_Y + recoilOff;
  tft.fillRect(WEAPON_X - 10, WEAPON_BASE_Y - 40, 74, 60, COL_FLOOR);

  if (flash)
  {
    int ftX = (currentWeapon == WEAPON_RIFLE) ? WEAPON_X + 46 :
              (currentWeapon == WEAPON_SHOTGUN) ? WEAPON_X + 50 : WEAPON_X + 24;
    tft.fillTriangle(ftX, wy - 38, ftX - 14, wy - 18, ftX + 14, wy - 18, COL_MUZZLE);
    tft.fillTriangle(ftX - 8, wy - 30, ftX, wy - 46, ftX + 8, wy - 30, 0xFFFF);
  }

  if (currentWeapon == WEAPON_PISTOL)
  {
    tft.fillRect(WEAPON_X + 10, wy + 6,  12, 18, COL_WEAPON_DK);  // grip
    tft.fillRect(WEAPON_X,      wy - 14, 38, 18, COL_WEAPON);      // slide
    tft.fillRect(WEAPON_X + 30, wy - 10, 12,  8, COL_WEAPON_DK);  // barrel
    tft.fillRect(WEAPON_X + 6,  wy - 18,  8,  6, COL_WEAPON);      // rear sight
    tft.fillRect(WEAPON_X + 14, wy - 4,  10,  4, COL_WEAPON_DK);  // trigger guard
  }
  else if (currentWeapon == WEAPON_RIFLE)
  {
    tft.fillRect(WEAPON_X - 6,  wy + 4,  14, 20, COL_WEAPON_DK);  // stock
    tft.fillRect(WEAPON_X + 6,  wy - 12, 42, 16, COL_WEAPON);      // body
    tft.fillRect(WEAPON_X + 44, wy -  8, 22,  6, COL_WEAPON_DK);  // barrel
    tft.fillRect(WEAPON_X + 16, wy - 22,  8, 11, COL_WEAPON_DK);  // scope
    tft.fillRect(WEAPON_X + 20, wy +  2, 10,  6, COL_WEAPON_DK);  // mag
  }
  else // SHOTGUN - chunky double-barrel look
  {
    tft.fillRect(WEAPON_X - 4,  wy + 6,  16, 20, COL_WEAPON_DK);  // stock
    tft.fillRect(WEAPON_X + 10, wy - 10, 36, 20, COL_WEAPON);      // receiver
    tft.fillRect(WEAPON_X + 44, wy - 6,  26,  6, COL_WEAPON_DK);  // top barrel
    tft.fillRect(WEAPON_X + 44, wy + 2,  26,  6, COL_WEAPON_DK);  // bottom barrel
    tft.fillRect(WEAPON_X + 14, wy + 10, 14,  6, COL_WEAPON_DK);  // pump grip
  }

  lastRecoil = recoilOff;
  lastFlash  = flash;
  lastType   = currentWeapon;
}

// =====================================================================
// HUD
// =====================================================================
#define HUD_BAR_Y  (SCR_H - 22)
#define HUD_BAR_H  22
#define HUD_FILL_W 56

void doomliteRenderHud(bool fullDraw)
{
  if (!fullDraw &&
      playerHealth == prevHudHealth &&
      playerAmmo   == prevHudAmmo   &&
      playerKills  == prevHudKills) return;

  tft.fillRect(0, HUD_BAR_Y, SCR_W, HUD_BAR_H, COL_HUD_BG);
  tft.setTextSize(1);

  // HP bar
  tft.setTextColor(playerHealth <= 25 ? 0xF800 : COL_HUD);
  tft.setCursor(4, HUD_BAR_Y + 3);
  tft.print("HP");
  tft.drawRect(4, HUD_BAR_Y + 12, HUD_FILL_W, 6, COL_HUD);
  tft.fillRect(5, HUD_BAR_Y + 13, HUD_FILL_W - 2, 4, COL_BAR_BG);
  int hpFW = ((HUD_FILL_W - 2) * playerHealth) / 100;
  if (hpFW > 0) tft.fillRect(5, HUD_BAR_Y + 13, hpFW, 4, COL_BAR_HP);

  // AMMO bar
  int ammoMax = weaponDefs[currentWeapon].startingAmmo;
  tft.setTextColor(playerAmmo == 0 ? 0xF800 : COL_HUD);
  tft.setCursor(80, HUD_BAR_Y + 3);
  tft.print("AMMO");
  tft.drawRect(80, HUD_BAR_Y + 12, HUD_FILL_W, 6, COL_HUD);
  tft.fillRect(81, HUD_BAR_Y + 13, HUD_FILL_W - 2, 4, COL_BAR_BG);
  int ammoFW = ((HUD_FILL_W - 2) * playerAmmo) / (ammoMax > 0 ? ammoMax : 1);
  if (ammoFW > 0) tft.fillRect(81, HUD_BAR_Y + 13, ammoFW, 4, COL_BAR_AMMO);

  // Kills / total + weapon name
  tft.setTextColor(COL_HUD);
  tft.setCursor(162, HUD_BAR_Y + 3);
  tft.print("K:");
  tft.print(playerKills);
  tft.print("/");
  tft.print(activeEnemyCount);
  tft.setCursor(162, HUD_BAR_Y + 13);
  tft.print(weaponDefs[currentWeapon].name);

  prevHudHealth = playerHealth;
  prevHudAmmo   = playerAmmo;
  prevHudKills  = playerKills;
}

// =====================================================================
// MINIMAP
// =====================================================================
#define MM_SIZE  44
#define MM_X     (SCR_W - MM_SIZE - 4)
#define MM_Y     4
#define MM_CELL  (MM_SIZE / MAP_SIZE)

void doomliteRenderMinimap()
{
  tft.fillRect(MM_X, MM_Y, MM_SIZE, MM_SIZE, COL_MINIMAP_BG);

  for (int y = 0; y < MAP_SIZE; y++)
    for (int x = 0; x < MAP_SIZE; x++)
      if (mapAt(x, y) != 0)
        tft.fillRect(MM_X + x * MM_CELL, MM_Y + y * MM_CELL,
                     max(1, MM_CELL), max(1, MM_CELL), COL_MINIMAP_WL);

  int ppx = MM_X + ((playerX >> FP_SHIFT) * MM_CELL);
  int ppy = MM_Y + ((playerY >> FP_SHIFT) * MM_CELL);
  tft.fillRect(ppx, ppy, 2, 2, COL_MINIMAP_PL);

  // Direction arrow
  int al = 4;
  int arx = ppx + (int)((float)al * cosf((float)playerAngle * 2.0f * PI / ANG_MAX));
  int ary = ppy + (int)((float)al * sinf((float)playerAngle * 2.0f * PI / ANG_MAX));
  if (arx >= MM_X && arx < MM_X + MM_SIZE && ary >= MM_Y && ary < MM_Y + MM_SIZE)
    tft.drawLine(ppx + 1, ppy + 1, arx, ary, COL_MINIMAP_PL);

  // Enemy dots - color encodes AI state so you can read the fight at a glance
  for (int i = 0; i < activeEnemyCount; i++)
  {
    if (!enemies[i].alive) continue;
    int epx = MM_X + ((enemies[i].x >> FP_SHIFT) * MM_CELL);
    int epy = MM_Y + ((enemies[i].y >> FP_SHIFT) * MM_CELL);
    uint16_t ec;
    switch (enemies[i].state)
    {
      case ESTATE_ATTACK:  ec = 0xFFE0; break;  // yellow - firing
      case ESTATE_STRAFE:  ec = 0xFD20; break;  // orange - flanking
      case ESTATE_CHASE:   ec = 0xFD20; break;  // orange - chasing
      case ESTATE_ALERT:   ec = 0xFFFF; break;  // white - just spotted you
      default:             ec = COL_MINIMAP_EN; break; // red - patrolling
    }
    tft.fillRect(epx, epy, 2, 2, ec);
  }

  tft.drawRect(MM_X - 1, MM_Y - 1, MM_SIZE + 2, MM_SIZE + 2, COL_HUD);
}





//////////snake game 

void runSnake()
{
    static bool init = false;

    static int snakeX[100];
    static int snakeY[100];

    static int foodX;
    static int foodY;

    static int len;
    static int dir;
    static int score;

    static bool gameOver = false;

    static unsigned long lastMove;

    if(!init)
    {
        len = 3;

        snakeX[0] = 12;
        snakeY[0] = 10;

        snakeX[1] = 11;
        snakeY[1] = 10;

        snakeX[2] = 10;
        snakeY[2] = 10;

        foodX = random(24);
        foodY = random(20);

        dir = 3;
        score = 0;

        lastMove = millis();

        tft.fillScreen(COLOR_BG);

        tft.fillRect(0, 0, 240, 35, COLOR_HEADER);

        tft.setTextColor(COLOR_WHITE);
        tft.setTextSize(2);
        tft.setCursor(5, 9);
        tft.print("Score: 0");

        tft.drawRect(0, 38, 240, 200, COLOR_ACCENT);

        tft.fillRect(foodX * 10 + 1, foodY * 10 + 39, 8, 8, COLOR_HEADER);

        for(int i = 0; i < len; i++)
        {
            tft.fillRect(snakeX[i] * 10 + 1, snakeY[i] * 10 + 39, 8, 8, COLOR_ACCENT);
        }

        gameOver = false;
        init = true;
    }

    if(gameOver)
    {
        tft.fillScreen(COLOR_BG);

        tft.setTextColor(COLOR_WHITE);
        tft.setTextSize(3);
        tft.setCursor(35, 80);
        tft.print("GAME OVER");

        tft.setTextSize(2);
        tft.setCursor(70, 130);
        tft.print("Score ");
        tft.print(score);

        tft.setTextSize(1);
        tft.setCursor(55, 180);
        tft.print("SELECT TO RESTART");

        while(digitalRead(BTN_SELECT) == HIGH)
        {
            delay(10);
        }

        waitRelease(BTN_SELECT);

        init = false;

        return;
    }

    if(digitalRead(BTN_UP) == LOW && dir != 1) dir = 0;
    if(digitalRead(BTN_DOWN) == LOW && dir != 0) dir = 1;
    if(digitalRead(BTN_L) == LOW && dir != 3) dir = 2;
    if(digitalRead(BTN_R) == LOW && dir != 2) dir = 3;

    if(millis() - lastMove < 150)
    {
        return;
    }

    lastMove = millis();

    int tailX = snakeX[len - 1];
    int tailY = snakeY[len - 1];

    int headX = snakeX[0];
    int headY = snakeY[0];

    if(dir == 0) headY--;
    if(dir == 1) headY++;
    if(dir == 2) headX--;
    if(dir == 3) headX++;

    if(headX < 0 || headX >= 24 || headY < 0 || headY >= 20)
    {
        gameOver = true;
        return;
    }

    for(int i = 0; i < len; i++)
    {
        if(snakeX[i] == headX && snakeY[i] == headY)
        {
            gameOver = true;
            return;
        }
    }

    bool ateFood = false;

    if(headX == foodX && headY == foodY)
    {
        ateFood = true;

        if(len < 99)
        {
            len++;
        }

        score++;

        foodX = random(24);
        foodY = random(20);

        tft.fillRect(0, 0, 240, 35, COLOR_HEADER);

        tft.setTextColor(COLOR_WHITE);
        tft.setTextSize(2);
        tft.setCursor(5, 9);
        tft.print("Score: ");
        tft.print(score);

        tft.fillRect(foodX * 10 + 1, foodY * 10 + 39, 8, 8, COLOR_HEADER);
    }

    for(int i = len - 1; i > 0; i--)
    {
        snakeX[i] = snakeX[i - 1];
        snakeY[i] = snakeY[i - 1];
    }

    snakeX[0] = headX;
    snakeY[0] = headY;

    if(!ateFood)
    {
        tft.fillRect(tailX * 10 + 1, tailY * 10 + 39, 8, 8, COLOR_BG);
    }

    tft.fillRect(headX * 10 + 1, headY * 10 + 39, 8, 8, COLOR_ACCENT);
}




// =====================================================
// PONG — improved edition v2
// =====================================================
//
// CHANGES vs v1
// ---------------------------------------------------
// 1. playerY declared as proper static int
// 2. "READY?" flicker fixed — drawn once via pongReadyDrawn flag
// 3. Improved splash / title screen — animated, better layout,
//    Left/Right also changes difficulty on the title screen
// 4. Win score is now selectable (3, 5, 7, 10, 15) on a
//    dedicated screen between difficulty and match start,
//    navigable with Up/Down/Left/Right
// 5. Left/Right on title screen change difficulty (same as Up/Down)
// =====================================================

#define PONG_W        240
#define PONG_H        240

#define PONG_TOP      46     // top of play field (below header+HUD strip)
#define PONG_BOTTOM   234    // bottom of play field

#define PADDLE_W      6
#define PADDLE_H      36
#define PADDLE_SPEED  5
#define DASH_SPEED    9      // speed while dashing (Left/Right hold)
#define DASH_COOLDOWN 18     // frames before dash can be used again

#define BALL_SIZE     6

#define PLAYER_X      (PONG_W - 14 - PADDLE_W)
#define AI_X          14


// ---- win score options ----

const int pongWinOptions[]    = { 3, 5, 7, 10, 15 };
const int pongWinOptionCount  = 5;
static int pongWinOptionIndex = 2;   // default = 7

#define PONG_SCORE_LIMIT  pongWinOptions[pongWinOptionIndex]


// ---- difficulty levels ----

#define PONG_DIFF_EASY    0
#define PONG_DIFF_NORMAL  1
#define PONG_DIFF_HARD    2
#define PONG_DIFF_COUNT   3

const char* pongDiffNames[PONG_DIFF_COUNT] =
{
    "EASY",
    "NORMAL",
    "HARD"
};

const char* pongDiffHints[PONG_DIFF_COUNT] =
{
    "RELAXED CPU, SLOW BALL",
    "BALANCED CHALLENGE",
    "FAST CPU, QUICK BALL"
};

const int   pongDiffAiSpeed[PONG_DIFF_COUNT]    = { 2, 3, 5 };
const int   pongDiffAiDeadzone[PONG_DIFF_COUNT] = { 16, 8, 2 };
const float pongDiffBallSpeed[PONG_DIFF_COUNT]  = { 1.8f, 2.3f, 3.0f };


// ---- runtime state ----

// static int   playerY;    // FIX: was missing / commented out in v1
static int   aiY;

static float ballX, ballY;
static float ballVX, ballVY;

static int   playerScore;
static int   aiScore;
static int   rallyCount;

static bool  ballMoving;
static bool  pongGameOver;
static bool  pongPaused;
static bool  pongPendingReturnToTitle;
static bool  pongReadyDrawn;   // FIX: prevents "READY?" from redrawing every frame

static int   pongDifficulty = PONG_DIFF_NORMAL;

static bool  lastPongUp    = HIGH;
static bool  lastPongDown  = HIGH;
static bool  lastPongLeft  = HIGH;
static bool  lastPongRight = HIGH;
static bool  lastPongSel   = HIGH;
static bool  lastPongBack  = HIGH;

static int   prevPlayerY;
static int   prevAiY;
static int   prevBallX, prevBallY;

static int   pongFlashFrames = 0;
static int   pongShakeFrames = 0;
static int   pongDashCooldown = 0;
static bool  pongDashing = false;

static int   serveCountdown = 0;


// =====================================================
// SMALL HELPERS
// =====================================================

void pongClearScreen()
{
    tft.fillScreen(COLOR_BG);
}

int pongCenterTextX(const char* text, int textSize)
{
    int charW = 6 * textSize;
    return (PONG_W - (int)strlen(text) * charW) / 2;
}

void pongResetBall(int direction)
{
    ballX = PONG_W / 2 - BALL_SIZE / 2;
    ballY = PONG_H / 2 - BALL_SIZE / 2;

    float speed = pongDiffBallSpeed[pongDifficulty];

    ballVX = speed * direction;
    ballVY = (random(0, 2) == 0) ? (speed * 0.6f) : -(speed * 0.6f);

    ballMoving     = false;
    rallyCount     = 0;
    pongReadyDrawn = false;   // reset so the ready msg shows once on next serve
}


// =====================================================
// TITLE / SPLASH SCREEN  — improved
// =====================================================
// Animated demo rally runs behind the selector.
// Left/Right AND Up/Down both change the difficulty.
// A large animated "PONG" header pulses every 30 frames.
// "PRESS SELECT" blinks as before.

static float demoBallX, demoBallY, demoVX, demoVY;
static int   blinkCounter = 0;
static int   titlePulse   = 0;

void pongInitDemo()
{
    demoBallX = PONG_W / 2;
    demoBallY = 60;
    demoVX    = 1.8f;
    demoVY    = 1.2f;
}

void pongStepDemo()
{
    demoBallX += demoVX;
    demoBallY += demoVY;

    if(demoBallY <= 48 || demoBallY >= 80) demoVY = -demoVY;
    if(demoBallX <= AI_X + PADDLE_W + 2)   { demoVX = -demoVX; demoBallX = AI_X + PADDLE_W + 2; }
    if(demoBallX >= PLAYER_X - 2)           { demoVX = -demoVX; demoBallX = PLAYER_X - 2; }

    // erase demo strip
    tft.fillRect(0, 46, PONG_W, 38, COLOR_BG);

    // draw demo paddles
    tft.fillRoundRect(AI_X,     52, PADDLE_W, 24, 2, COLOR_ACCENT);
    tft.fillRoundRect(PLAYER_X, 52, PADDLE_W, 24, 2, COLOR_ACCENT);

    // draw demo ball
    tft.fillRect((int)demoBallX, (int)demoBallY, BALL_SIZE, BALL_SIZE, COLOR_WHITE);

    // thin separator line
    tft.drawFastHLine(0, 85, PONG_W, COLOR_ACCENT);
}

// Draw the big animated PONG title (only in the header area)
void pongDrawTitleHeader(bool pulse)
{
    tft.fillRect(0, 0, PONG_W, 46, COLOR_HEADER);

    // shadow offset for depth effect
    uint16_t shadowCol = 0xA000;  // dark red
    tft.setTextSize(3);
    tft.setTextColor(shadowCol);
    tft.setCursor(pongCenterTextX("PONG", 3) + 2, 9 + 2);
    tft.print("PONG");

    // main text — slightly brighter on pulse frames
    tft.setTextColor(pulse ? COLOR_WHITE : COLOR_ACCENT);
    tft.setCursor(pongCenterTextX("PONG", 3), 9);
    tft.print("PONG");
}

void pongDrawTitleStatic(int diffChoice)
{
    pongClearScreen();
    pongDrawTitleHeader(false);

    // demo strip placeholder (will be animated each frame)
    tft.drawFastHLine(0, 85, PONG_W, COLOR_ACCENT);

    // win condition
    tft.setTextSize(1);
    tft.setTextColor(COLOR_WHITE);
    char winLine[32];
    snprintf(winLine, sizeof(winLine), "FIRST TO %d POINTS WINS", PONG_SCORE_LIMIT);
    tft.setCursor(pongCenterTextX(winLine, 1), 91);
    tft.print(winLine);

    // divider
    tft.drawFastHLine(30, 104, PONG_W - 60, COLOR_ACCENT);

    // difficulty label
    tft.setTextColor(COLOR_WHITE);
    tft.setCursor(pongCenterTextX("DIFFICULTY", 1), 109);
    tft.print("DIFFICULTY");

    // difficulty selector row
    int boxY = 120;
    int boxH = 34;

    tft.fillRoundRect(50, boxY, 140, boxH, 8, COLOR_ACCENT);

    tft.setTextColor(COLOR_SELECTED);
    tft.setTextSize(2);
    tft.setCursor(56, boxY + 9);
    tft.print("<");
    tft.setCursor(170, boxY + 9);
    tft.print(">");

    int nameX = pongCenterTextX(pongDiffNames[diffChoice], 2);
    tft.setCursor(nameX, boxY + 9);
    tft.print(pongDiffNames[diffChoice]);

    tft.setTextSize(1);
    tft.setTextColor(COLOR_ACCENT);
    tft.setCursor(pongCenterTextX(pongDiffHints[diffChoice], 1), boxY + boxH + 6);
    tft.print(pongDiffHints[diffChoice]);

    // difficulty dots
    int dotsY      = boxY + boxH + 20;
    int dotsStartX = PONG_W/2 - (PONG_DIFF_COUNT * 16)/2 + 8;

    for(int i = 0; i < PONG_DIFF_COUNT; i++)
    {
        int dx = dotsStartX + i * 16;
        if(i == diffChoice)
            tft.fillCircle(dx, dotsY, 4, COLOR_ACCENT);
        else
            tft.drawCircle(dx, dotsY, 4, COLOR_WHITE);
    }

    // footer hint
    tft.drawFastHLine(20, 196, PONG_W - 40, COLOR_ACCENT);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_WHITE);
    tft.setCursor(pongCenterTextX("L/R OR U/D: CHANGE", 1), 202);
    tft.print("L/R OR U/D: CHANGE");
}

void pongDrawBlinkPrompt()
{
    tft.fillRect(0, 218, PONG_W, 14, COLOR_BG);

    if((blinkCounter / 15) % 2 == 0)
    {
        tft.setTextColor(COLOR_ACCENT);
        tft.setTextSize(1);
        tft.setCursor(pongCenterTextX("SELECT: START GAME", 1), 220);
        tft.print("SELECT: START GAME");
    }
}

// Returns chosen difficulty, or -1 if BACK pressed.
int pongTitleScreen()
{
    int diffChoice = pongDifficulty;

    bool lUp = HIGH, lDown = HIGH, lLeft = HIGH, lRight = HIGH, lSel = HIGH;

    pongInitDemo();
    pongDrawTitleStatic(diffChoice);
    blinkCounter = 0;
    titlePulse   = 0;

    while(true)
    {
        if(digitalRead(BTN_BACK) == LOW)
        {
            waitRelease(BTN_BACK);
            return -1;
        }

        bool up    = digitalRead(BTN_UP);
        bool down  = digitalRead(BTN_DOWN);
        bool left  = digitalRead(BTN_L);
        bool right = digitalRead(BTN_R);
        bool sel   = digitalRead(BTN_SELECT);

        // Up OR Left  = previous difficulty
        bool stepPrev = (lUp == HIGH && up == LOW) || (lLeft == HIGH && left == LOW);
        // Down OR Right = next difficulty
        bool stepNext = (lDown == HIGH && down == LOW) || (lRight == HIGH && right == LOW);

        if(stepPrev)
        {
            diffChoice--;
            if(diffChoice < 0) diffChoice = PONG_DIFF_COUNT - 1;
            pongDrawTitleStatic(diffChoice);
        }

        if(stepNext)
        {
            diffChoice++;
            if(diffChoice >= PONG_DIFF_COUNT) diffChoice = 0;
            pongDrawTitleStatic(diffChoice);
        }

        if(lSel == HIGH && sel == LOW)
        {
            waitRelease(BTN_SELECT);
            return diffChoice;
        }

        // animate header pulse every 30 frames
        titlePulse++;
        if(titlePulse == 30 || titlePulse == 60)
        {
            pongDrawTitleHeader(titlePulse == 30);
            if(titlePulse == 60) titlePulse = 0;
        }

        pongStepDemo();
        pongDrawBlinkPrompt();
        blinkCounter++;

        lUp    = up;
        lDown  = down;
        lLeft  = left;
        lRight = right;
        lSel   = sel;

        delay(20);
    }
}


// =====================================================
// WIN SCORE SELECTION SCREEN
// =====================================================
// Called after difficulty is chosen. Left/Right AND Up/Down
// change the win target. SELECT confirms, BACK goes back.
// Returns chosen index into pongWinOptions[], or -1 to go back.

void pongDrawWinScoreScreen(int idx)
{
    pongClearScreen();

    // header
    tft.fillRect(0, 0, PONG_W, 35, COLOR_HEADER);
    tft.setTextColor(COLOR_WHITE);
    tft.setTextSize(2);
    tft.setCursor(pongCenterTextX("PONG", 2), 9);
    tft.print("PONG");

    // subtitle
    tft.setTextSize(1);
    tft.setTextColor(COLOR_WHITE);
    tft.setCursor(pongCenterTextX("SET WIN SCORE", 1), 50);
    tft.print("SET WIN SCORE");

    tft.drawFastHLine(30, 63, PONG_W - 60, COLOR_ACCENT);

    // selector
    int boxY = 80;
    int boxH = 50;
    tft.fillRoundRect(40, boxY, 160, boxH, 10, COLOR_ACCENT);

    tft.setTextColor(COLOR_SELECTED);
    tft.setTextSize(2);
    tft.setCursor(46, boxY + 16);
    tft.print("<");
    tft.setCursor(175, boxY + 16);
    tft.print(">");

    char scoreStr[4];
    snprintf(scoreStr, sizeof(scoreStr), "%d", pongWinOptions[idx]);
    tft.setTextSize(3);
    int sx = pongCenterTextX(scoreStr, 3);
    tft.setCursor(sx, boxY + 9);
    tft.print(scoreStr);

    tft.setTextSize(1);
    tft.setTextColor(COLOR_WHITE);
    tft.setCursor(pongCenterTextX("POINTS TO WIN", 1), boxY + boxH + 8);
    tft.print("POINTS TO WIN");

    // option dots
    int dotsY      = boxY + boxH + 26;
    int dotsStartX = PONG_W/2 - (pongWinOptionCount * 16)/2 + 8;

    for(int i = 0; i < pongWinOptionCount; i++)
    {
        int dx = dotsStartX + i * 16;
        if(i == idx)
            tft.fillCircle(dx, dotsY, 4, COLOR_ACCENT);
        else
            tft.drawCircle(dx, dotsY, 4, COLOR_WHITE);
    }

    // quick-reference labels
    tft.setTextSize(1);
    tft.setTextColor(COLOR_ACCENT);
    for(int i = 0; i < pongWinOptionCount; i++)
    {
        int dx = dotsStartX + i * 16;
        char lbl[4];
        snprintf(lbl, sizeof(lbl), "%d", pongWinOptions[i]);
        tft.setCursor(dx - (int)strlen(lbl)*3, dotsY + 10);
        tft.print(lbl);
    }

    // footer
    tft.drawFastHLine(20, 195, PONG_W - 40, COLOR_ACCENT);
    tft.setTextColor(COLOR_WHITE);
    tft.setTextSize(1);
    tft.setCursor(pongCenterTextX("L/R OR U/D: CHANGE", 1), 202);
    tft.print("L/R OR U/D: CHANGE");

    tft.setTextColor(COLOR_ACCENT);
    tft.setCursor(pongCenterTextX("SELECT: CONFIRM  BACK: BACK", 1), 216);
    tft.print("SELECT: CONFIRM  BACK: BACK");
}

int pongWinScoreScreen()
{
    int idx = pongWinOptionIndex;

    bool lUp = HIGH, lDown = HIGH, lLeft = HIGH, lRight = HIGH, lSel = HIGH;

    pongDrawWinScoreScreen(idx);

    while(true)
    {
        if(digitalRead(BTN_BACK) == LOW)
        {
            waitRelease(BTN_BACK);
            return -1;
        }

        bool up    = digitalRead(BTN_UP);
        bool down  = digitalRead(BTN_DOWN);
        bool left  = digitalRead(BTN_L);
        bool right = digitalRead(BTN_R);
        bool sel   = digitalRead(BTN_SELECT);

        bool stepPrev = (lUp == HIGH && up == LOW) || (lLeft == HIGH && left == LOW);
        bool stepNext = (lDown == HIGH && down == LOW) || (lRight == HIGH && right == LOW);

        if(stepPrev)
        {
            idx--;
            if(idx < 0) idx = pongWinOptionCount - 1;
            pongDrawWinScoreScreen(idx);
        }

        if(stepNext)
        {
            idx++;
            if(idx >= pongWinOptionCount) idx = 0;
            pongDrawWinScoreScreen(idx);
        }

        if(lSel == HIGH && sel == LOW)
        {
            waitRelease(BTN_SELECT);
            return idx;
        }

        lUp    = up;
        lDown  = down;
        lLeft  = left;
        lRight = right;
        lSel   = sel;

        delay(20);
    }
}


// =====================================================
// MATCH HUD + COURT
// =====================================================

void pongDrawCourt()
{
    pongClearScreen();

    tft.fillRect(0, 0, PONG_W, 35, COLOR_HEADER);
    tft.setTextColor(COLOR_WHITE);
    tft.setTextSize(2);
    tft.setCursor(pongCenterTextX("PONG", 2), 9);
    tft.print("PONG");

    tft.setTextSize(1);
    tft.setTextColor(COLOR_ACCENT);
    tft.setCursor(6, 38);
    tft.print(pongDiffNames[pongDifficulty]);

    char target[16];
    snprintf(target, sizeof(target), "TO %d", PONG_SCORE_LIMIT);
    tft.setCursor(PONG_W - 6 - (int)strlen(target)*6, 38);
    tft.print(target);

    for(int y = PONG_TOP + 4; y < PONG_BOTTOM; y += 14)
        tft.fillRect(PONG_W/2 - 1, y, 2, 8, COLOR_WHITE);

    tft.setTextSize(3);
    tft.setTextColor(COLOR_WHITE);

    char aiStr[4], plStr[4];
    snprintf(aiStr, sizeof(aiStr), "%d", aiScore);
    snprintf(plStr, sizeof(plStr), "%d", playerScore);

    tft.setCursor(PONG_W/2 - 46, 8);
    tft.print(aiStr);

    tft.setCursor(PONG_W/2 + 30, 8);
    tft.print(plStr);

    tft.drawFastHLine(0, PONG_TOP, PONG_W, COLOR_ACCENT);
}


void pongDrawPaddles()
{
    tft.fillRect(AI_X,     prevAiY,     PADDLE_W, PADDLE_H, COLOR_BG);
    tft.fillRect(PLAYER_X, prevPlayerY, PADDLE_W, PADDLE_H, COLOR_BG);

    for(int y = PONG_TOP + 4; y < PONG_BOTTOM; y += 14)
    {
        bool hitAi = (y + 8 > prevAiY && y < prevAiY + PADDLE_H);
        bool hitPl = (y + 8 > prevPlayerY && y < prevPlayerY + PADDLE_H);
        if(hitAi || hitPl)
            tft.fillRect(PONG_W/2 - 1, y, 2, 8, COLOR_WHITE);
    }

    tft.fillRoundRect(AI_X, aiY, PADDLE_W, PADDLE_H, 3, COLOR_ACCENT);

    uint16_t playerColor = pongDashing ? COLOR_WHITE : COLOR_ACCENT;
    tft.fillRoundRect(PLAYER_X, playerY, PADDLE_W, PADDLE_H, 3, playerColor);

    prevAiY     = aiY;
    prevPlayerY = playerY;
}


void pongDrawBall()
{
    tft.fillRect(prevBallX, prevBallY, BALL_SIZE, BALL_SIZE, COLOR_BG);

    uint16_t ballColor = (pongFlashFrames > 0) ? COLOR_ACCENT : COLOR_WHITE;
    tft.fillRect((int)ballX, (int)ballY, BALL_SIZE, BALL_SIZE, ballColor);

    prevBallX = (int)ballX;
    prevBallY = (int)ballY;

    if(pongFlashFrames > 0) pongFlashFrames--;
}


void pongDrawRallyCount()
{
    tft.fillRect(PONG_W/2 - 24, 38, 48, 8, COLOR_BG);

    if(rallyCount > 0)
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "x%d", rallyCount);
        tft.setTextSize(1);
        tft.setTextColor(COLOR_WHITE);
        tft.setCursor(PONG_W/2 - (int)strlen(buf)*3, 38);
        tft.print(buf);
    }
}


void pongShowMessage(const char* line1, const char* line2)
{
    tft.fillRoundRect(20, PONG_H/2 - 34, PONG_W - 40, 68, 8, COLOR_BG);
    tft.drawRoundRect(20, PONG_H/2 - 34, PONG_W - 40, 68, 8, COLOR_ACCENT);

    tft.setTextSize(2);
    tft.setTextColor(COLOR_WHITE);
    tft.setCursor(pongCenterTextX(line1, 2), PONG_H/2 - 22);
    tft.print(line1);

    if(line2 != NULL)
    {
        tft.setTextSize(1);
        tft.setTextColor(COLOR_ACCENT);
        tft.setCursor(pongCenterTextX(line2, 1), PONG_H/2 + 8);
        tft.print(line2);
    }
}


void pongDoScoreFlashShake()
{
    tft.fillScreen(COLOR_WHITE);
    delay(45);
    pongShakeFrames = 6;
}


// =====================================================
// MATCH STEP
// =====================================================

void pongStep()
{
    bool up    = digitalRead(BTN_UP);
    bool down  = digitalRead(BTN_DOWN);
    bool left  = digitalRead(BTN_L);
    bool right = digitalRead(BTN_R);
    bool sel   = digitalRead(BTN_SELECT);
    bool back  = digitalRead(BTN_BACK);


    // ---- pause toggle ----

    if(lastPongBack == HIGH && back == LOW)
    {
        waitRelease(BTN_BACK);
        pongPaused = !pongPaused;

        if(pongPaused)
        {
            pongShowMessage("PAUSED", "SELECT: RESUME   BACK: EXIT");
        }
        else
        {
            pongDrawCourt();
            pongDrawPaddles();
            pongReadyDrawn = false;
        }
    }

    if(pongPaused)
    {
        if(lastPongSel == HIGH && sel == LOW)
        {
            waitRelease(BTN_SELECT);
            pongPaused     = false;
            pongReadyDrawn = false;
            pongDrawCourt();
            pongDrawPaddles();
        }

        lastPongUp = up; lastPongDown = down;
        lastPongLeft = left; lastPongRight = right;
        lastPongSel = sel; lastPongBack = back;
        return;
    }


    // ---- player paddle movement ----

    bool wantUp   = (up == LOW);
    bool wantDown = (down == LOW);

    pongDashing = false;

    if(pongDashCooldown > 0) pongDashCooldown--;

    if((left == LOW || right == LOW) && pongDashCooldown == 0 && (wantUp || wantDown))
    {
        pongDashing = true;
        pongDashCooldown = DASH_COOLDOWN;
    }

    int speed = pongDashing ? DASH_SPEED : PADDLE_SPEED;

    if(wantUp)   playerY -= speed;
    if(wantDown) playerY += speed;

    if(playerY < PONG_TOP + 4)           playerY = PONG_TOP + 4;
    if(playerY > PONG_BOTTOM - PADDLE_H) playerY = PONG_BOTTOM - PADDLE_H;


    // ---- serve / confirm ----

    if(lastPongSel == HIGH && sel == LOW)
    {
        if(pongGameOver)
        {
            pongPendingReturnToTitle = true;
        }
        else if(!ballMoving && serveCountdown == 0)
        {
            serveCountdown = 60;
            pongReadyDrawn = false;
        }
    }


    // ---- serve countdown ----

    if(!pongGameOver && !ballMoving && serveCountdown > 0)
    {
        serveCountdown--;

        int secondsLeft = (serveCountdown / 20) + 1;
        if(secondsLeft > 3) secondsLeft = 3;

        char buf[2];
        buf[0] = '0' + secondsLeft;
        buf[1] = '\0';

        pongShowMessage(buf, "GET READY");

        if(serveCountdown == 0)
        {
            ballMoving = true;
            pongDrawCourt();
            pongDrawPaddles();
        }
    }


    if(!pongGameOver && ballMoving)
    {
        // ---- AI ----

        int aiSpeed    = pongDiffAiSpeed[pongDifficulty];
        int aiDeadzone = pongDiffAiDeadzone[pongDifficulty];

        int aiCenter = aiY + PADDLE_H/2;
        int targetY  = (int)ballY + BALL_SIZE/2;

        if(aiCenter < targetY - aiDeadzone) aiY += aiSpeed;
        else if(aiCenter > targetY + aiDeadzone) aiY -= aiSpeed;

        if(aiY < PONG_TOP + 4)           aiY = PONG_TOP + 4;
        if(aiY > PONG_BOTTOM - PADDLE_H) aiY = PONG_BOTTOM - PADDLE_H;


        // ---- ball motion ----

        ballX += ballVX;
        ballY += ballVY;

        if(ballY <= PONG_TOP + 4)             { ballY = PONG_TOP + 4;        ballVY = -ballVY; }
        if(ballY >= PONG_BOTTOM - BALL_SIZE)  { ballY = PONG_BOTTOM - BALL_SIZE; ballVY = -ballVY; }


        // AI paddle collision
        if(ballX <= AI_X + PADDLE_W && ballX >= AI_X &&
           ballY + BALL_SIZE >= aiY && ballY <= aiY + PADDLE_H)
        {
            ballX  = AI_X + PADDLE_W;
            ballVX = -ballVX * 1.06f;

            float offset = ((ballY + BALL_SIZE/2) - (aiY + PADDLE_H/2)) / (float)(PADDLE_H/2);
            ballVY += offset * 1.6f;

            pongFlashFrames = 4;
            rallyCount++;
        }


        // player paddle collision
        if(ballX + BALL_SIZE >= PLAYER_X && ballX <= PLAYER_X + PADDLE_W &&
           ballY + BALL_SIZE >= playerY && ballY <= playerY + PADDLE_H)
        {
            ballX  = PLAYER_X - BALL_SIZE;
            ballVX = -ballVX * 1.06f;

            float offset = ((ballY + BALL_SIZE/2) - (playerY + PADDLE_H/2)) / (float)(PADDLE_H/2);
            ballVY += offset * 1.6f;

            if(pongDashing) ballVX *= 1.08f;

            pongFlashFrames = 4;
            rallyCount++;
        }


        // clamp speed
        if(ballVX >  8) ballVX =  8;
        if(ballVX < -8) ballVX = -8;
        if(ballVY >  6) ballVY =  6;
        if(ballVY < -6) ballVY = -6;


        // scoring
        if(ballX < 0)
        {
            playerScore++;
            pongDoScoreFlashShake();
            pongResetBall(1);
            pongDrawCourt();
            pongDrawPaddles();
        }

        if(ballX > PONG_W)
        {
            aiScore++;
            pongDoScoreFlashShake();
            pongResetBall(-1);
            pongDrawCourt();
            pongDrawPaddles();
        }


        if(playerScore >= PONG_SCORE_LIMIT || aiScore >= PONG_SCORE_LIMIT)
            pongGameOver = true;
    }


    // ---- draw ----

    pongDrawPaddles();
    pongDrawRallyCount();

    if(ballMoving && !pongGameOver)
    {
        pongDrawBall();
    }

    // FIX: only draw "READY?" once, not every frame (prevents flicker)
    if(!ballMoving && !pongGameOver && serveCountdown == 0)
    {
        if(!pongReadyDrawn)
        {
            pongShowMessage("READY?", "PRESS SELECT TO SERVE");
            pongReadyDrawn = true;
        }
    }

    if(pongGameOver)
    {
        const char* winner = (playerScore > aiScore) ? "YOU WIN!" : "CPU WINS";
        char scoreLine[24];
        snprintf(scoreLine, sizeof(scoreLine), "%d - %d", aiScore, playerScore);
        pongShowMessage(winner, "SELECT: REMATCH  BACK: EXIT");

        tft.setTextSize(1);
        tft.setTextColor(COLOR_WHITE);
        tft.setCursor(pongCenterTextX(scoreLine, 1), PONG_H/2 + 22);
        tft.print(scoreLine);
    }


    lastPongUp    = up;
    lastPongDown  = down;
    lastPongLeft  = left;
    lastPongRight = right;
    lastPongSel   = sel;
    lastPongBack  = back;
}


// =====================================================
// ENTRY POINT — called from gameItems switch as runPong()
// =====================================================

void runPong()
{
    pongClearScreen();
    waitRelease(BTN_SELECT);

    while(true)
    {
        // ---- 1. Title / difficulty screen ----
        int chosenDiff = pongTitleScreen();

        if(chosenDiff < 0)
        {
            pongClearScreen();
            return;
        }

        pongDifficulty = chosenDiff;

        // ---- 2. Win score selection screen ----
        int chosenWinIdx = pongWinScoreScreen();

        if(chosenWinIdx < 0)
        {
            // BACK pressed — return to difficulty screen
            continue;
        }

        pongWinOptionIndex = chosenWinIdx;

        // ---- 3. Init match ----
        playerY = PONG_H/2 - PADDLE_H/2;
        aiY     = PONG_H/2 - PADDLE_H/2;

        prevPlayerY = playerY;
        prevAiY     = aiY;

        playerScore = 0;
        aiScore     = 0;
        rallyCount  = 0;

        pongGameOver             = false;
        pongPaused               = false;
        pongPendingReturnToTitle = false;
        pongReadyDrawn           = false;
        pongFlashFrames          = 0;
        pongShakeFrames          = 0;
        pongDashCooldown         = 0;
        pongDashing              = false;
        serveCountdown           = 0;

        pongResetBall((random(0,2)==0) ? 1 : -1);

        prevBallX = (int)ballX;
        prevBallY = (int)ballY;

        lastPongUp    = HIGH;
        lastPongDown  = HIGH;
        lastPongLeft  = HIGH;
        lastPongRight = HIGH;
        lastPongSel   = HIGH;
        lastPongBack  = HIGH;

        pongDrawCourt();
        pongDrawPaddles();

        // ---- 4. Game loop ----
        while(true)
        {
            pongStep();

            if(pongPendingReturnToTitle)
                break;

            delay(20);
        }
    }
}

// =====================================================
// FLAPPY BIRD — SMO Edition
// =====================================================
// Integrates with the existing SMO menu system.
// Launch via direct call: runFlappyBird();
// (NOT runLoop(runFlappyBird) — see the note above runFlappyBird()
// near the bottom of this file for why.)
//
// Honest scope note: this is a full, playable, polished
// build within a real ~1k line budget. It includes one
// deep pipe theme (Neon) rather than five shallow ones,
// and an in-RAM stats/high-score system rather than
// EEPROM persistence (no buzzer/flash pin was defined in
// smo.ino, so audio + flash saving are left as a follow-up
// once those pins are specified — wiring guesses for
// flash storage risk corrupting other saved state).
// =====================================================
// =====================================================
// FLAPPY BIRD — SMO Edition  (v2 — fixes Arduino auto-
// prototype / name-collision compile errors from v1)
// =====================================================
// Integrates with the existing SMO menu system.
// Launch via direct call: runFlappyBird();
// (NOT runLoop(runFlappyBird) — see the note above
// runFlappyBird() near the bottom of this file for why.)
//
// WHAT CHANGED FROM v1 AND WHY (so this doesn't happen again):
//
// 1. Arduino's IDE auto-generates forward declarations for every
//    function in a sketch by scanning ALL concatenated .ino files
//    as one giant translation unit, then inserting those prototypes
//    near the top — BEFORE your structs/enums are defined. In a
//    small sketch this is invisible. In a 3000+ line merged sketch
//    (Doom Lite + Snake + Pong + Flappy Bird all in Games.ino) the
//    scanner can place a prototype for a function like
//    fbResetPipe(FbPipe &p, float x) above the point where struct
//    FbPipe is actually declared, so the compiler sees "FbPipe"
//    before it exists. Same thing happened to FbMedal.
//    FIX: every struct/enum type used in a function signature is
//    now forward-declared at the very top of this file, and every
//    function that takes one of those types is ALSO explicitly
//    forward-declared right after, so there is no ordering
//    ambiguity left for Arduino's scanner to get wrong.
//
// 2. smo.ino already declares global `bool lastUp, lastDown,
//    lastBack` (etc.) at file scope for the main menu's button
//    state. v1 of this file declared its OWN globals with the
//    exact same names (lastUp, lastDown, lastSel, lastBack, lastL,
//    lastR). `static` does not let you shadow a same-named global
//    at file scope in C++ — it's a hard redefinition error.
//    FIX: every Flappy-Bird-local button-state variable is now
//    prefixed fb (fbLastUp, fbLastDown, fbLastSel, fbLastBack,
//    fbLastL, fbLastR) so it can never collide with smo.ino's,
//    Pong's, or Doom Lite's button-state globals.
//
// Scope note (unchanged from v1): one deep pipe theme (Neon)
// instead of five shallow ones, and in-RAM stats/high-score
// tracking instead of EEPROM persistence (no buzzer/flash pin is
// defined anywhere in smo.ino, so audio + flash saving are a
// follow-up once those pins are specified).
// =====================================================

// ---------------- DEFINITIONS ----------------

#define FB_W 240
#define FB_H 240
#define FB_TOP 30
#define FB_GROUND (FB_H - 18)

#define FB_BIRD_X 60
#define FB_BIRD_SIZE 14

#define FB_PIPE_W 34
#define FB_GAP_MAX 92
#define FB_GAP_MIN 56

#define FB_MAX_PIPES 4
#define FB_MAX_PARTICLES 24
#define FB_MAX_POPUPS 6

#define FB_SKY_DAY     0x5D9F
#define FB_SKY_SUNSET  0xFB8C
#define FB_SKY_NIGHT   0x10A6
#define FB_GROUND_COL  0x4ACA
#define FB_GROUND_DARK 0x2A86
#define FB_NEON_PINK   0xF81F
#define FB_NEON_CYAN   0x07FF
#define FB_COIN_COL    0xFFE0

// ---------------- TYPE FORWARD DECLARATIONS ----------------
// Declared here, before anything else, so Arduino's auto-prototype
// scanner can never insert a function prototype that references
// these types above their definition point. (See note #1 at top
// of file.)

enum FbWeather { FB_CLEAR, FB_RAIN, FB_SNOW, FB_FOG };
enum FbState   { FB_TITLE, FB_PLAYING, FB_DEAD, FB_STATS, FB_ACHIEVE };
enum FbMedal   { MEDAL_NONE, MEDAL_BRONZE, MEDAL_SILVER, MEDAL_GOLD, MEDAL_PLAT, MEDAL_LEGEND };

struct FbPipe {
  float x;
  int gapY;
  int gapH;
  bool passed;
  float bob;        // vertical bob phase (moving obstacle)
  bool moving;
};

struct FbParticle {
  float x, y, vx, vy;
  int life;
  uint16_t col;
};

struct FbPopup {
  float x, y;
  int life;
  char text[12];
  uint16_t col;
};

struct FbAchievement {
  const char* name;
  bool unlocked;
};

struct FbStats {
  uint32_t totalGames;
  uint32_t highScore;
  uint32_t totalScoreSum;
  uint32_t coinsCollected;
  uint32_t totalDistance;
  uint32_t nearMisses;
  uint32_t perfectGaps;
};

// ---------------- EXPLICIT FUNCTION FORWARD DECLARATIONS ----------------
// Every function whose signature mentions one of the structs/enums
// above gets an explicit prototype here too. This makes the file
// correct on its own merits, independent of whatever Arduino's
// scanner does or doesn't manage to auto-generate.

static void fbResetPipe(FbPipe &p, float x);
static void fbDrawPipe(FbPipe &p);
static FbMedal fbMedalFor(int score);
static uint16_t fbMedalColor(FbMedal m);
static const char* fbMedalName(FbMedal m);

// ---------------- GLOBAL STATE ----------------

static FbState fbState;
static int fbScore;
static int fbCoins;
static int fbCombo;
static int fbComboTimer;
static int fbDifficultyTier;

static float birdY, birdVY;
static float birdRot;
static int birdFlapFrame;
static int birdFlapTimer;

static FbPipe pipes[FB_MAX_PIPES];
static FbParticle particles[FB_MAX_PARTICLES];
static FbPopup popups[FB_MAX_POPUPS];

static FbStats fbStats;
static FbAchievement fbAch[6] = {
  {"FIRST FLIGHT",     false},
  {"COMBO MASTER",     false},
  {"COIN HOARDER",     false},
  {"NEAR DEATH x10",   false},
  {"PERFECT 5",        false},
  {"CENTURY CLUB",     false}
};

static float worldX;          // scroll position, drives parallax + pipe spawn
static float cloudX[3];       // 3 parallax cloud layers
static int dayPhase;          // 0..2399, cycles day->sunset->night->sunrise
static FbWeather weather;
static int weatherTimer;
static float fogAlpha;

static int fbShakeFrames;
static int fbShakeX, fbShakeY;
static int fbFlashFrames;

// Renamed with fb prefix — smo.ino already owns globals named
// lastUp/lastDown/lastBack at file scope; reusing those names here
// caused "redefinition" compile errors. (See note #2 at top of file.)
static bool fbLastUp, fbLastDown, fbLastSel, fbLastBack, fbLastL, fbLastR;

static int titleMenuIndex;     // 0=PLAY 1=STATS 2=ACHIEVEMENTS
static int titleAnimTick;

static unsigned long fbPlayStartMillis;
static bool fbNeedsRedraw;
static bool fbDirtyFrameReady;
static int fbPrevBirdY;
static int fbPrevPipeX[FB_MAX_PIPES];
static int fbPrevPipeGapY[FB_MAX_PIPES];
static int fbPrevPipeGapH[FB_MAX_PIPES];
static int fbLastDrawnScore;
static int fbLastDrawnCoins;
static int fbLastDrawnCombo;
static uint16_t fbPlaySkyCol;

// ---------------- HELPERS ----------------

static uint16_t fbLerpColor(uint16_t a, uint16_t b, float t) {
  if (t < 0) t = 0;
  if (t > 1) t = 1;
  uint8_t ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
  uint8_t br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
  uint8_t r = ar + (br - ar) * t;
  uint8_t g = ag + (bg - ag) * t;
  uint8_t bl = ab + (bb - ab) * t;
  return (r << 11) | (g << 5) | bl;
}

// Self-contained text centering (kept local rather than reusing
// Pong's pongCenterTextX, so this file has no cross-file dependency
// on whichever other game .ino files happen to be in the sketch).
static int fbCenterTextX(const char* text, int textSize) {
  int charW = 6 * textSize;
  return (FB_W - (int)strlen(text) * charW) / 2;
}

static uint16_t fbSkyColor() {
  // 0-799 day, 800-1199 sunset, 1200-1999 night, 2000-2399 sunrise
  if (dayPhase < 800) return FB_SKY_DAY;
  if (dayPhase < 1200) return fbLerpColor(FB_SKY_DAY, FB_SKY_SUNSET, (dayPhase - 800) / 400.0f);
  if (dayPhase < 1600) return fbLerpColor(FB_SKY_SUNSET, FB_SKY_NIGHT, (dayPhase - 1200) / 400.0f);
  if (dayPhase < 2000) return FB_SKY_NIGHT;
  return fbLerpColor(FB_SKY_NIGHT, FB_SKY_DAY, (dayPhase - 2000) / 400.0f);
}

static bool fbIsNight() { return dayPhase >= 1300 && dayPhase < 2100; }

static void fbSpawnParticle(float x, float y, uint16_t col, float spreadV) {
  for (int i = 0; i < FB_MAX_PARTICLES; i++) {
    if (particles[i].life <= 0) {
      particles[i].x = x; particles[i].y = y;
      particles[i].vx = ((random(0, 100) - 50) / 50.0f) * spreadV;
      particles[i].vy = ((random(0, 100) - 80) / 50.0f) * spreadV;
      particles[i].life = 18 + random(0, 12);
      particles[i].col = col;
      return;
    }
  }
}

static void fbSpawnPopup(float x, float y, const char* txt, uint16_t col) {
  for (int i = 0; i < FB_MAX_POPUPS; i++) {
    if (popups[i].life <= 0) {
      popups[i].x = x; popups[i].y = y;
      strncpy(popups[i].text, txt, 11);
      popups[i].text[11] = 0;
      popups[i].life = 30;
      popups[i].col = col;
      return;
    }
  }
}

static void fbTriggerShake(int frames) { fbShakeFrames = frames; }

// ---------------- DIFFICULTY ----------------

static float fbPipeSpeed() {
  return 1.6f + (fbDifficultyTier * 0.18f);
}

static int fbGapForTier() {
  int gap = FB_GAP_MAX - (fbDifficultyTier * 3);
  if (gap < FB_GAP_MIN) gap = FB_GAP_MIN;
  return gap;
}

static void fbUpdateDifficulty() {
  fbDifficultyTier = fbScore / 5;
  if (fbDifficultyTier > 18) fbDifficultyTier = 18;
}

// ---------------- PIPE SYSTEM ----------------

static void fbResetPipe(FbPipe &p, float x) {
  p.x = x;
  p.gapH = fbGapForTier();
  int margin = 28;
  p.gapY = random(FB_TOP + margin, FB_GROUND - margin - p.gapH);
  p.passed = false;
  p.bob = random(0, 628) / 100.0f;
  p.moving = false;
}

static void fbSpawnPipes() {
  float spacing = 150 - min(fbDifficultyTier * 3, 40);
  for (int i = 0; i < FB_MAX_PIPES; i++) {
    fbResetPipe(pipes[i], FB_W + i * spacing);
  }
}

static void fbUpdatePipes() {
  float speed = fbPipeSpeed();
  float spacing = 150 - min(fbDifficultyTier * 3, 40);

  for (int i = 0; i < FB_MAX_PIPES; i++) {
    pipes[i].x -= speed;
    pipes[i].bob += 0.05f;

    if (pipes[i].moving) {
      pipes[i].gapY += (int)(sinf(pipes[i].bob) * 1.4f);
      if (pipes[i].gapY < FB_TOP + 20) pipes[i].gapY = FB_TOP + 20;
      if (pipes[i].gapY > FB_GROUND - 20 - pipes[i].gapH) pipes[i].gapY = FB_GROUND - 20 - pipes[i].gapH;
    }

    if (pipes[i].x < -FB_PIPE_W) {
      float maxX = pipes[0].x;
      for (int j = 1; j < FB_MAX_PIPES; j++) if (pipes[j].x > maxX) maxX = pipes[j].x;
      fbResetPipe(pipes[i], maxX + spacing);
    }
  }
}

// ---------------- PHYSICS ----------------

static void fbApplyFlap(bool strong) {
  birdVY = strong ? -5.4f : -4.3f;
  birdFlapFrame = 0;
  birdFlapTimer = 6;
}

static void fbUpdateBirdPhysics() {
  birdVY += 0.30f; // gravity
  if (birdVY > 7.5f) birdVY = 7.5f;
  birdY += birdVY;

  birdRot = birdVY * 4.0f;
  if (birdRot > 80) birdRot = 80;
  if (birdRot < -35) birdRot = -35;

  if (birdFlapTimer > 0) {
    birdFlapTimer--;
    if (birdFlapTimer == 0) birdFlapFrame = (birdFlapFrame + 1) % 3;
  }
}

// ---------------- COLLISION ----------------

static bool fbCheckCollision() {
  if (birdY <= FB_TOP || birdY + FB_BIRD_SIZE >= FB_GROUND) return true;

  for (int i = 0; i < FB_MAX_PIPES; i++) {
    FbPipe &p = pipes[i];
    bool overlapX = (FB_BIRD_X + FB_BIRD_SIZE > p.x) && (FB_BIRD_X < p.x + FB_PIPE_W);
    if (!overlapX) continue;

    bool inGap = (birdY > p.gapY) && (birdY + FB_BIRD_SIZE < p.gapY + p.gapH);
    if (!inGap) return true;

    // near-miss bonus: bird is inside gap but close to an edge
    int distTop = birdY - p.gapY;
    int distBot = (p.gapY + p.gapH) - (birdY + FB_BIRD_SIZE);
    if (!p.passed && (distTop < 6 || distBot < 6)) {
      fbStats.nearMisses++;
    }
  }
  return false;
}

static void fbCheckPassedPipes() {
  for (int i = 0; i < FB_MAX_PIPES; i++) {
    FbPipe &p = pipes[i];
    if (!p.passed && p.x + FB_PIPE_W < FB_BIRD_X) {
      p.passed = true;
      fbScore++;
      fbCombo++;
      fbComboTimer = 45;

      int distTop = birdY - p.gapY;
      int distBot = (p.gapY + p.gapH) - (birdY + FB_BIRD_SIZE);
      bool centered = (abs(distTop - distBot) < 5);

      if (centered) {
        fbStats.perfectGaps++;
        fbScore++; // bonus point for perfect gap
      }

      if (centered || fbScore % 5 == 0) {
        fbCoins++;
        fbStats.coinsCollected++;
      }

      fbUpdateDifficulty();
    }
  }
}

// ---------------- ACHIEVEMENTS ----------------

static void fbCheckAchievements() {
  if (fbStats.totalGames >= 1) fbAch[0].unlocked = true;
  if (fbCombo >= 10) fbAch[1].unlocked = true;
  if (fbStats.coinsCollected >= 20) fbAch[2].unlocked = true;
  if (fbStats.nearMisses >= 10) fbAch[3].unlocked = true;
  if (fbStats.perfectGaps >= 5) fbAch[4].unlocked = true;
  if (fbScore >= 100) fbAch[5].unlocked = true;
}

static FbMedal fbMedalFor(int score) {
  if (score >= 80) return MEDAL_LEGEND;
  if (score >= 50) return MEDAL_PLAT;
  if (score >= 30) return MEDAL_GOLD;
  if (score >= 15) return MEDAL_SILVER;
  if (score >= 5)  return MEDAL_BRONZE;
  return MEDAL_NONE;
}

static uint16_t fbMedalColor(FbMedal m) {
  switch (m) {
    case MEDAL_BRONZE:  return 0xC247;
    case MEDAL_SILVER:  return 0xC618;
    case MEDAL_GOLD:    return FB_COIN_COL;
    case MEDAL_PLAT:    return 0x87FF;
    case MEDAL_LEGEND:  return FB_NEON_PINK;
    default: return COLOR_WHITE;
  }
}

static const char* fbMedalName(FbMedal m) {
  switch (m) {
    case MEDAL_BRONZE: return "BRONZE";
    case MEDAL_SILVER: return "SILVER";
    case MEDAL_GOLD:   return "GOLD";
    case MEDAL_PLAT:   return "PLATINUM";
    case MEDAL_LEGEND: return "LEGENDARY";
    default: return "NONE";
  }
}

// ---------------- WEATHER / ATMOSPHERE ----------------

static void fbUpdateAtmosphere() {
  dayPhase = (dayPhase + 1) % 2400;

  weatherTimer--;
  if (weatherTimer <= 0) {
    int r = random(0, 100);
    if (r < 55) weather = FB_CLEAR;
    else if (r < 75) weather = FB_RAIN;
    else if (r < 90) weather = FB_SNOW;
    else weather = FB_FOG;
    weatherTimer = 300 + random(0, 300);
  }
  fogAlpha = (weather == FB_FOG) ? 0.35f : 0.0f;

  if (weather == FB_RAIN && random(0, 100) < 40)
    fbSpawnParticle(random(0, FB_W), FB_TOP, 0x33BF, 0.1f);
  if (weather == FB_SNOW && random(0, 100) < 25)
    fbSpawnParticle(random(0, FB_W), FB_TOP, COLOR_WHITE, 0.05f);
}

// ---------------- RENDERING ----------------

static void fbDrawSky() {
  tft.fillRect(0, 0, FB_W, FB_GROUND, fbSkyColor());

  if (fbIsNight()) {
    for (int i = 0; i < 14; i++) {
      int sx = (i * 53 + 17) % FB_W;
      int sy = (i * 31 + 5) % (FB_GROUND - FB_TOP - 10) + FB_TOP;
      tft.drawPixel(sx, sy, COLOR_WHITE);
    }
  }
}

static void fbDrawClouds() {
  uint16_t cloudCol = fbIsNight() ? 0x3186 : COLOR_WHITE;
  for (int layer = 0; layer < 3; layer++) {
    int y = FB_TOP + 14 + layer * 22;
    int cx = ((int)cloudX[layer]) % (FB_W + 60) - 60;
    for (int n = 0; n < 2; n++) {
      int x = cx + n * (FB_W / 2 + 30);
      tft.fillCircle(x, y, 8 - layer * 2, cloudCol);
      tft.fillCircle(x + 10, y - 3, 7 - layer * 2, cloudCol);
      tft.fillCircle(x + 20, y, 8 - layer * 2, cloudCol);
    }
  }
}

static void fbDrawGround() {
  tft.fillRect(0, FB_GROUND, FB_W, FB_H - FB_GROUND, FB_GROUND_COL);
  int offset = ((int)worldX) % 16;
  for (int x = -offset; x < FB_W; x += 16)
    tft.fillRect(x, FB_GROUND, 8, 4, FB_GROUND_DARK);
}

static void fbDrawPipe(FbPipe &p) {
  uint16_t col = FB_NEON_CYAN;
  uint16_t glow = FB_NEON_PINK;
  int x = (int)p.x;

  // top pipe
  tft.fillRect(x, FB_TOP, FB_PIPE_W, p.gapY - FB_TOP, col);
  tft.drawRect(x, FB_TOP, FB_PIPE_W, p.gapY - FB_TOP, glow);
  tft.drawFastVLine(x + 4, FB_TOP, p.gapY - FB_TOP, glow);
  tft.fillRect(x - 2, p.gapY - 10, FB_PIPE_W + 4, 10, glow);

  // bottom pipe
  int by = p.gapY + p.gapH;
  tft.fillRect(x, by, FB_PIPE_W, FB_GROUND - by, col);
  tft.drawRect(x, by, FB_PIPE_W, FB_GROUND - by, glow);
  tft.drawFastVLine(x + 4, by, FB_GROUND - by, glow);
  tft.fillRect(x - 2, by, FB_PIPE_W + 4, 10, glow);

}

static void fbDrawBird() {
  int bx = FB_BIRD_X, by = (int)birdY;
  uint16_t body = 0xFE60; // warm yellow-orange

  // shadow on ground (flattened rounded rect — Adafruit_GFX has no
  // fillEllipse, so a wide, short rounded rect reads as a soft shadow)
  int shadowY = FB_GROUND - 3;
  int shadowW = max(4, 16 - (shadowY - by) / 8);
  tft.fillRoundRect(bx + 6 - shadowW / 2, shadowY, shadowW, 3, 1, FB_GROUND_DARK);

  tft.fillCircle(bx + 6, by + 7, 7, body);
  tft.fillCircle(bx + 9, by + 4, 2, COLOR_WHITE);
  tft.fillTriangle(bx + 12, by + 6, bx + 17, by + 5, bx + 12, by + 9, FB_COIN_COL);

  int wingOffsets[3] = { 2, -2, 4 };
  int wy = by + 7 + wingOffsets[birdFlapFrame];
  tft.fillTriangle(bx + 2, by + 7, bx - 4, wy, bx + 4, by + 10, 0xFC00);
}

static void fbDrawParticlesAndPopups() {
  for (int i = 0; i < FB_MAX_PARTICLES; i++) {
    if (particles[i].life > 0) {
      particles[i].x += particles[i].vx;
      particles[i].y += particles[i].vy;
      particles[i].vy += 0.06f;
      particles[i].life--;
      tft.fillRect((int)particles[i].x, (int)particles[i].y, 2, 2, particles[i].col);
    }
  }
  for (int i = 0; i < FB_MAX_POPUPS; i++) {
    if (popups[i].life > 0) {
      popups[i].y -= 0.5f;
      popups[i].life--;
      tft.setTextSize(1);
      tft.setTextColor(popups[i].col);
      tft.setCursor((int)popups[i].x, (int)popups[i].y);
      tft.print(popups[i].text);
    }
  }
}

static void fbDrawFog() {
  if (weather != FB_FOG) return;
  int drift = ((int)worldX / 2) % 24;
  for (int y = FB_TOP + 8; y < FB_GROUND; y += 18) {
    int x = (y * 3 + drift) % 48 - 24;
    tft.drawFastHLine(x, y, 70, 0xCE79);
    tft.drawFastHLine(x + 90, y + 4, 60, 0xCE79);
  }
}

static void fbDrawHUD() {
  tft.fillRect(0, 0, FB_W, FB_TOP, COLOR_HEADER);
  tft.setTextSize(2);
  tft.setTextColor(COLOR_WHITE);
  char buf[16];
  snprintf(buf, sizeof(buf), "%d", fbScore);
  tft.setCursor(8, 4);
  tft.print(buf);

  tft.setTextSize(1);
  tft.setTextColor(FB_COIN_COL);
  snprintf(buf, sizeof(buf), "COINS %d", fbCoins);
  tft.setCursor(FB_W - 70, 6);
  tft.print(buf);

  if (fbCombo > 1) {
    tft.setTextColor(FB_NEON_PINK);
    snprintf(buf, sizeof(buf), "x%d COMBO", fbCombo);
    tft.setCursor(FB_W / 2 - 24, 18);
    tft.print(buf);
  }
}

static void fbApplyShakeOffset() {
  if (fbShakeFrames > 0) {
    fbShakeX = random(-3, 4);
    fbShakeY = random(-3, 4);
    fbShakeFrames--;
  } else {
    fbShakeX = 0; fbShakeY = 0;
  }
}

static void fbFillPlayBackgroundRect(int x, int y, int w, int h) {
  int x1 = max(0, x);
  int y1 = max(0, y);
  int x2 = min(FB_W, x + w);
  int y2 = min(FB_H, y + h);
  if (x1 >= x2 || y1 >= y2) return;

  if (y1 < FB_TOP) {
    int hTop = min(y2, FB_TOP) - y1;
    if (hTop > 0) tft.fillRect(x1, y1, x2 - x1, hTop, COLOR_HEADER);
  }

  if (y2 > FB_TOP && y1 < FB_GROUND) {
    int sy = max(y1, FB_TOP);
    int ey = min(y2, FB_GROUND);
    if (ey > sy) tft.fillRect(x1, sy, x2 - x1, ey - sy, fbPlaySkyCol);
  }

  if (y2 > FB_GROUND) {
    int gy = max(y1, FB_GROUND);
    if (y2 > gy) tft.fillRect(x1, gy, x2 - x1, y2 - gy, FB_GROUND_COL);
  }
}

static void fbErasePipeAt(int x, int gapY, int gapH) {
  fbFillPlayBackgroundRect(x - 3, FB_TOP, FB_PIPE_W + 8, gapY - FB_TOP);
  fbFillPlayBackgroundRect(x - 3, gapY - 11, FB_PIPE_W + 8, 12);
  fbFillPlayBackgroundRect(x - 3, gapY + gapH, FB_PIPE_W + 8, FB_GROUND - (gapY + gapH));
}

static void fbErasePipeTrail(int oldX, int newX, int gapY, int gapH) {
  int oldRight = oldX + FB_PIPE_W + 3;
  int newRight = newX + FB_PIPE_W + 3;
  int stripW = oldRight - newRight;
  if (stripW <= 0 || stripW > 8) {
    fbErasePipeAt(oldX, gapY, gapH);
    return;
  }
  fbFillPlayBackgroundRect(newRight, FB_TOP, stripW, gapY - FB_TOP);
  fbFillPlayBackgroundRect(newRight, gapY - 11, stripW, 12);
  fbFillPlayBackgroundRect(newRight, gapY + gapH, stripW, FB_GROUND - (gapY + gapH));
}

static void fbCaptureDirtyFrame() {
  fbPrevBirdY = (int)birdY;
  for (int i = 0; i < FB_MAX_PIPES; i++) {
    fbPrevPipeX[i] = (int)pipes[i].x;
    fbPrevPipeGapY[i] = pipes[i].gapY;
    fbPrevPipeGapH[i] = pipes[i].gapH;
  }
  fbLastDrawnScore = fbScore;
  fbLastDrawnCoins = fbCoins;
  fbLastDrawnCombo = fbCombo;
  fbDirtyFrameReady = true;
}

static void fbDrawFullPlayfield() {
  fbPlaySkyCol = fbSkyColor();
  tft.fillRect(0, 0, FB_W, FB_GROUND, fbPlaySkyCol);
  for (int i = 0; i < FB_MAX_PIPES; i++) fbDrawPipe(pipes[i]);
  fbDrawGround();
  fbDrawBird();
  fbDrawHUD();
  fbCaptureDirtyFrame();
}

static void fbRenderFrame() {
  fbApplyShakeOffset();

  tft.startWrite();
  if (!fbDirtyFrameReady) {
    fbDrawFullPlayfield();
    tft.endWrite();
    return;
  }

  fbFillPlayBackgroundRect(FB_BIRD_X - 8, fbPrevBirdY - 8, FB_BIRD_SIZE + 22, FB_BIRD_SIZE + 18);
  for (int i = 0; i < FB_MAX_PIPES; i++) {
    int newX = (int)pipes[i].x;
    bool sameGap = fbPrevPipeGapY[i] == pipes[i].gapY && fbPrevPipeGapH[i] == pipes[i].gapH;
    if (sameGap && newX < fbPrevPipeX[i]) {
      fbErasePipeTrail(fbPrevPipeX[i], newX, fbPrevPipeGapY[i], fbPrevPipeGapH[i]);
    } else {
      fbErasePipeAt(fbPrevPipeX[i], fbPrevPipeGapY[i], fbPrevPipeGapH[i]);
    }
  }

  for (int i = 0; i < FB_MAX_PIPES; i++) fbDrawPipe(pipes[i]);
  fbDrawBird();

  if (fbScore != fbLastDrawnScore || fbCoins != fbLastDrawnCoins || fbCombo != fbLastDrawnCombo) {
    fbDrawHUD();
  }

  if (fbFlashFrames > 0) {
    tft.drawRect(0, 0, FB_W, FB_H, COLOR_WHITE);
    fbFlashFrames--;
  }
  fbCaptureDirtyFrame();
  tft.endWrite();
}

// ---------------- TITLE SCREEN ----------------

static const char* fbTitleMenuItems[3] = { "PLAY", "STATISTICS", "ACHIEVEMENTS" };

static void fbDrawTitleScreen() {
  tft.startWrite();
  tft.fillScreen(fbSkyColor());
  fbDrawClouds();

  tft.setTextSize(3);
  uint16_t pulse = ((titleAnimTick / 15) % 2 == 0) ? FB_NEON_CYAN : FB_NEON_PINK;
  tft.setTextColor(pulse);
  tft.setCursor(28, 50);
  tft.print("FLAPPY");
  tft.setCursor(48, 80);
  tft.print("SMO");

  int by = 120 + (int)(sinf(titleAnimTick / 10.0f) * 6);
  tft.fillCircle(FB_BIRD_X + 90, by, 9, 0xFE60);
  tft.fillTriangle(FB_BIRD_X + 96, by - 1, FB_BIRD_X + 102, by - 2, FB_BIRD_X + 96, by + 2, FB_COIN_COL);

  for (int i = 0; i < 3; i++) {
    int y = 165 + i * 22;
    bool sel = (i == titleMenuIndex);
    if (sel) tft.fillRoundRect(40, y - 4, 160, 20, 6, FB_NEON_CYAN);
    else tft.drawRoundRect(40, y - 4, 160, 20, 6, FB_NEON_CYAN);
    tft.setTextColor(sel ? COLOR_SELECTED : COLOR_WHITE);
    tft.setTextSize(1);
    tft.setCursor(60, y + 2);
    tft.print(fbTitleMenuItems[i]);
  }

  tft.setTextColor(COLOR_WHITE);
  tft.setCursor(50, 226);
  tft.print("SELECT: CHOOSE  BACK: EXIT");
  tft.endWrite();
}

static void fbDrawTitleMenuOnly() {
  tft.startWrite();
  tft.fillRect(36, 158, 170, 72, fbSkyColor());
  for (int i = 0; i < 3; i++) {
    int y = 165 + i * 22;
    bool sel = (i == titleMenuIndex);
    if (sel) tft.fillRoundRect(40, y - 4, 160, 20, 6, FB_NEON_CYAN);
    else tft.drawRoundRect(40, y - 4, 160, 20, 6, FB_NEON_CYAN);
    tft.setTextColor(sel ? COLOR_SELECTED : COLOR_WHITE);
    tft.setTextSize(1);
    tft.setCursor(60, y + 2);
    tft.print(fbTitleMenuItems[i]);
  }
  tft.setTextColor(COLOR_WHITE);
  tft.setCursor(50, 226);
  tft.print("SELECT: CHOOSE  BACK: EXIT");
  tft.endWrite();
}

static void fbDrawStatsScreen() {
  tft.startWrite();
  tft.fillScreen(COLOR_BG);
  drawHeader("STATISTICS");
  tft.setTextSize(1);
  tft.setTextColor(COLOR_WHITE);
  char buf[40];
  int y = 50;
  uint32_t avg = fbStats.totalGames ? (fbStats.totalScoreSum / fbStats.totalGames) : 0;

  snprintf(buf, sizeof(buf), "TOTAL GAMES:    %lu", fbStats.totalGames);
  tft.setCursor(20, y); tft.print(buf); y += 16;
  snprintf(buf, sizeof(buf), "HIGH SCORE:     %lu", fbStats.highScore);
  tft.setCursor(20, y); tft.print(buf); y += 16;
  snprintf(buf, sizeof(buf), "AVERAGE SCORE:  %lu", avg);
  tft.setCursor(20, y); tft.print(buf); y += 16;
  snprintf(buf, sizeof(buf), "COINS:          %lu", fbStats.coinsCollected);
  tft.setCursor(20, y); tft.print(buf); y += 16;
  snprintf(buf, sizeof(buf), "DISTANCE:       %lu", fbStats.totalDistance);
  tft.setCursor(20, y); tft.print(buf); y += 16;
  snprintf(buf, sizeof(buf), "NEAR MISSES:    %lu", fbStats.nearMisses);
  tft.setCursor(20, y); tft.print(buf); y += 16;
  snprintf(buf, sizeof(buf), "PERFECT GAPS:   %lu", fbStats.perfectGaps);
  tft.setCursor(20, y); tft.print(buf); y += 16;

  unsigned long secs = (millis() - fbPlayStartMillis) / 1000;
  snprintf(buf, sizeof(buf), "SESSION TIME:   %lus", secs);
  tft.setCursor(20, y); tft.print(buf);

  tft.setTextColor(FB_NEON_CYAN);
  tft.setCursor(50, 224);
  tft.print("BACK: RETURN");
  tft.endWrite();
}

static void fbDrawAchieveScreen() {
  tft.startWrite();
  tft.fillScreen(COLOR_BG);
  drawHeader("ACHIEVEMENTS");
  tft.setTextSize(1);
  for (int i = 0; i < 6; i++) {
    int y = 45 + i * 26;
    uint16_t col = fbAch[i].unlocked ? FB_COIN_COL : 0x5AEB;
    tft.drawRoundRect(15, y, 210, 20, 5, col);
    tft.setTextColor(col);
    tft.setCursor(24, y + 6);
    tft.print(fbAch[i].unlocked ? "[X] " : "[ ] ");
    tft.print(fbAch[i].name);
  }
  tft.setTextColor(FB_NEON_CYAN);
  tft.setCursor(50, 224);
  tft.print("BACK: RETURN");
  tft.endWrite();
}

// ---------------- GAME OVER ----------------

static void fbEndGame() {
  fbState = FB_DEAD;
  fbTriggerShake(10);
  fbFlashFrames = 3;
  fbNeedsRedraw = true;

  fbStats.totalGames++;
  fbStats.totalScoreSum += fbScore;
  fbStats.totalDistance += (uint32_t)worldX;
  if ((uint32_t)fbScore > fbStats.highScore) fbStats.highScore = fbScore;

  fbCheckAchievements();
}

static void fbDrawGameOverScreen() {
  fbRenderFrame(); // freeze-frame of the crash behind the panel

  tft.startWrite();
  tft.fillRoundRect(20, 60, 200, 130, 10, COLOR_BG);
  tft.drawRoundRect(20, 60, 200, 130, 10, FB_NEON_PINK);

  FbMedal medal = fbMedalFor(fbScore);
  uint16_t mcol = fbMedalColor(medal);

  tft.setTextSize(2);
  tft.setTextColor(FB_NEON_PINK);
  tft.setCursor(60, 70);
  tft.print("GAME OVER");

  tft.setTextSize(1);
  tft.setTextColor(COLOR_WHITE);
  char buf[24];
  snprintf(buf, sizeof(buf), "SCORE: %d", fbScore);
  tft.setCursor(80, 100);
  tft.print(buf);

  snprintf(buf, sizeof(buf), "BEST: %lu", fbStats.highScore);
  tft.setCursor(80, 114);
  tft.print(buf);

  tft.fillCircle(120, 145, 14, mcol);
  tft.setTextColor(COLOR_SELECTED);
  tft.setCursor(112, 140);
  tft.print(medal == MEDAL_NONE ? "-" : "*");

  tft.setTextColor(mcol);
  tft.setCursor(fbCenterTextX(fbMedalName(medal), 1), 162);
  tft.print(fbMedalName(medal));

  tft.setTextColor(COLOR_WHITE);
  tft.setCursor(35, 178);
  tft.print("SELECT: RETRY  BACK: MENU");
  tft.endWrite();
}

// ---------------- RESET / INIT ----------------

static void fbResetRun() {
  birdY = FB_H / 2;
  birdVY = 0;
  birdRot = 0;
  birdFlapFrame = 0;
  birdFlapTimer = 0;

  fbScore = 0;
  fbCoins = 0;
  fbCombo = 0;
  fbComboTimer = 0;
  fbDifficultyTier = 0;

  worldX = 0;
  for (int i = 0; i < 3; i++) cloudX[i] = random(0, FB_W);
  dayPhase = random(0, 800);
  weather = FB_CLEAR;
  weatherTimer = 200;

  for (int i = 0; i < FB_MAX_PARTICLES; i++) particles[i].life = 0;
  for (int i = 0; i < FB_MAX_POPUPS; i++) popups[i].life = 0;

  fbShakeFrames = 0;
  fbFlashFrames = 0;
  fbDirtyFrameReady = false;
  fbLastDrawnScore = -1;
  fbLastDrawnCoins = -1;
  fbLastDrawnCombo = -1;

  fbSpawnPipes();
  fbPlayStartMillis = millis();
}

// ---------------- INPUT ----------------

static void fbReadButtonsRaw(bool &up, bool &down, bool &sel, bool &back, bool &l, bool &r) {
  up = digitalRead(BTN_UP);
  down = digitalRead(BTN_DOWN);
  sel = digitalRead(BTN_SELECT);
  back = digitalRead(BTN_BACK);
  l = digitalRead(BTN_L);
  r = digitalRead(BTN_R);
}

// ---------------- MAIN STEP ----------------
// NOTE: this game does NOT use runLoop(runFlappyBird), because
// runLoop() treats any BTN_BACK press as "exit the whole app" — it
// has no concept of sub-screens (title / stats / achievements /
// playing / dead). If runLoop owned the loop, BACK from the stats
// screen would kick all the way out to the SMO main menu instead of
// returning to the Flappy title screen. So runFlappyBird() runs its
// own internal loop and only returns (handing control back to
// handleGamesMenu) once the player presses BACK while already on
// the title screen.

static bool fbStepOnce() {
  bool up, down, sel, back, l, r;
  fbReadButtonsRaw(up, down, sel, back, l, r);
  bool exitToSmoMenu = false;

  if (fbState == FB_TITLE) {
    bool titleChanged = false;

    if (fbLastUp == HIGH && up == LOW) {
      titleMenuIndex = (titleMenuIndex + 2) % 3;
      titleChanged = true;
    }
    if (fbLastDown == HIGH && down == LOW) {
      titleMenuIndex = (titleMenuIndex + 1) % 3;
      titleChanged = true;
    }

    if (fbLastSel == HIGH && sel == LOW) {
      waitRelease(BTN_SELECT);
      if (titleMenuIndex == 0) {
        fbResetRun();
        fbState = FB_PLAYING;
      } else if (titleMenuIndex == 1) {
        fbState = FB_STATS;
        fbNeedsRedraw = true;
      } else {
        fbState = FB_ACHIEVE;
        fbNeedsRedraw = true;
      }
    }

    if (fbLastBack == HIGH && back == LOW) {
      waitRelease(BTN_BACK);
      exitToSmoMenu = true; // BACK on title = leave the game entirely
    }

    if (fbState == FB_TITLE && !exitToSmoMenu &&
        (fbNeedsRedraw || titleChanged)) {
      if (fbNeedsRedraw) fbDrawTitleScreen();
      else fbDrawTitleMenuOnly();
      fbNeedsRedraw = false;
    }
  }

  else if (fbState == FB_STATS || fbState == FB_ACHIEVE) {
    if (fbNeedsRedraw) {
      if (fbState == FB_STATS) fbDrawStatsScreen(); else fbDrawAchieveScreen();
      fbNeedsRedraw = false;
    }
    if (fbLastBack == HIGH && back == LOW) {
      waitRelease(BTN_BACK);
      fbState = FB_TITLE;
      fbNeedsRedraw = true;
    }
  }

  else if (fbState == FB_PLAYING) {
    if (fbLastSel == HIGH && sel == LOW) {
      bool strong = (l == LOW || r == LOW);
      fbApplyFlap(strong);
    }
    // BACK while playing ends the run and returns to title
    if (fbLastBack == HIGH && back == LOW) {
      waitRelease(BTN_BACK);
      fbEndGame();
      fbState = FB_TITLE;
      fbNeedsRedraw = true;
    } else {
      fbUpdateBirdPhysics();
      fbUpdatePipes();
      worldX += fbPipeSpeed();

      if (fbComboTimer > 0) {
        fbComboTimer--;
        if (fbComboTimer == 0) fbCombo = 0;
      }

      fbCheckPassedPipes();

      if (fbCheckCollision()) fbEndGame();
      else fbRenderFrame();
    }
  }

  else if (fbState == FB_DEAD) {
    if (fbNeedsRedraw) {
      fbDrawGameOverScreen();
      fbNeedsRedraw = false;
    }

    if (fbLastSel == HIGH && sel == LOW) {
      waitRelease(BTN_SELECT);
      fbResetRun();
      fbState = FB_PLAYING;
    }
    if (fbLastBack == HIGH && back == LOW) {
      waitRelease(BTN_BACK);
      fbState = FB_TITLE;
      fbNeedsRedraw = true;
    }
  }

  fbLastUp = up; fbLastDown = down; fbLastSel = sel; fbLastBack = back;
  fbLastL = l; fbLastR = r;

  return exitToSmoMenu;
}

// ---------------- ENTRY POINT ----------------
// Call directly from the games switch as: runFlappyBird();
// (NOT runLoop(runFlappyBird) — see note above fbStepOnce().)

void runFlappyBird() {
  waitRelease(BTN_SELECT);
  waitRelease(BTN_BACK);

  dayPhase = random(0, 800);
  for (int i = 0; i < 3; i++) cloudX[i] = random(0, FB_W);
  weather = FB_CLEAR;
  weatherTimer = 200;
  fbState = FB_TITLE;
  titleMenuIndex = 0;
  titleAnimTick = 0;
  fbNeedsRedraw = true;

  fbLastUp = HIGH; fbLastDown = HIGH; fbLastSel = HIGH;
  fbLastBack = HIGH; fbLastL = HIGH; fbLastR = HIGH;

  bool exitRequested = false;
  while (!exitRequested) {
    unsigned long frameStart = millis();
    exitRequested = fbStepOnce();
    unsigned long frameElapsed = millis() - frameStart;
    if (frameElapsed < 20) delay(20 - frameElapsed);
    else delay(1);
  }

  waitRelease(BTN_BACK);
  tft.fillScreen(COLOR_BG);
}











// REACTION TEST GAME

bool reactionWaiting = true;
bool reactionFinished = false;

unsigned long reactionStartTime = 0;
unsigned long reactionTargetTime = 0;
unsigned long reactionResult = 0;

void runReactionTest()
{
    reactionWaiting = true;
    reactionFinished = false;

    reactionTargetTime =
        millis() + random(2000, 6000);

    tft.fillScreen(COLOR_BG);

    drawHeader("REACTION");

    tft.setTextColor(COLOR_WHITE);
    tft.setTextSize(2);

    tft.setCursor(20, 90);
    tft.print("Wait for GREEN");

    tft.setCursor(20, 120);
    tft.print("Then press SELECT");

    while(true)
    {
        if(digitalRead(BTN_BACK) == LOW)
        {
            waitRelease(BTN_BACK);
            return;
        }

        if(reactionWaiting)
        {
            if(millis() >= reactionTargetTime)
            {
                reactionWaiting = false;

                tft.fillScreen(ST77XX_GREEN);

                tft.setTextColor(ST77XX_BLACK);
                tft.setTextSize(3);

                tft.setCursor(55,100);
                tft.print("GO!");

                reactionStartTime = millis();
            }

            if(digitalRead(BTN_SELECT) == LOW)
            {
                waitRelease(BTN_SELECT);

                tft.fillScreen(ST77XX_RED);

                tft.setTextColor(COLOR_WHITE);
                tft.setTextSize(2);

                tft.setCursor(30,100);
                tft.print("Too Early!");

                delay(1500);
                return;
            }
        }
        else if(!reactionFinished)
        {
            if(digitalRead(BTN_SELECT) == LOW)
            {
                waitRelease(BTN_SELECT);

                reactionFinished = true;

                reactionResult =
                    millis() - reactionStartTime;

                tft.fillScreen(COLOR_BG);

                drawHeader("RESULT");

                tft.setTextColor(COLOR_ACCENT);
                tft.setTextSize(3);

                tft.setCursor(40,90);
                tft.print(reactionResult);

                tft.print(" ms");

                tft.setTextSize(1);

                tft.setCursor(50,170);
                tft.print("BACK TO EXIT");
            }
        }

        delay(5);
    }
}




////////////////2048 game


uint16_t board2048[4][4];

void addRandomTile()
{
    int empty[16][2];
    int count = 0;

    for(int r=0;r<4;r++)
    {
        for(int c=0;c<4;c++)
        {
            if(board2048[r][c] == 0)
            {
                empty[count][0] = r;
                empty[count][1] = c;
                count++;
            }
        }
    }

    if(count == 0) return;

    int pick = random(count);

    int r = empty[pick][0];
    int c = empty[pick][1];

    board2048[r][c] =
        (random(10) < 9) ? 2 : 4;
}

bool boardFull()
{
    for(int r=0;r<4;r++)
    {
        for(int c=0;c<4;c++)
        {
            if(board2048[r][c] == 0)
                return false;
        }
    }

    return true;
}

bool canMove()
{
    if(!boardFull())
        return true;

    for(int r=0;r<4;r++)
    {
        for(int c=0;c<4;c++)
        {
            if(r<3 &&
               board2048[r][c] ==
               board2048[r+1][c])
                return true;

            if(c<3 &&
               board2048[r][c] ==
               board2048[r][c+1])
                return true;
        }
    }

    return false;
}

void draw2048()
{
    tft.fillScreen(COLOR_BG);

    drawHeader("2048");

    int cellSize = 50;
    int startX = 18;
    int startY = 45;

    for(int r=0;r<4;r++)
    {
        for(int c=0;c<4;c++)
        {
            int x = startX + c*52;
            int y = startY + r*45;

            tft.drawRoundRect(
                x,
                y,
                cellSize,
                40,
                5,
                COLOR_ACCENT
            );

            if(board2048[r][c] != 0)
            {
                tft.setTextColor(COLOR_WHITE);
                tft.setTextSize(2);

                tft.setCursor(
                    x+8,
                    y+12
                );

                tft.print(board2048[r][c]);
            }
        }
    }
}


bool moveLeft()
{
    bool moved = false;

    for(int r=0;r<4;r++)
    {
        uint16_t temp[4] = {0};

        int idx = 0;

        for(int c=0;c<4;c++)
        {
            if(board2048[r][c] != 0)
            {
                temp[idx++] =
                    board2048[r][c];
            }
        }

        for(int i=0;i<3;i++)
        {
            if(temp[i] &&
               temp[i] == temp[i+1])
            {
                temp[i] *= 2;

                for(int j=i+1;j<3;j++)
                    temp[j] = temp[j+1];

                temp[3] = 0;
            }
        }

        for(int c=0;c<4;c++)
        {
            if(board2048[r][c] != temp[c])
                moved = true;

            board2048[r][c] = temp[c];
        }
    }

    return moved;
}

bool moveRight()
{
    bool moved = false;

    for(int r=0;r<4;r++)
    {
        uint16_t temp[4] = {0};

        int idx = 3;

        for(int c=3;c>=0;c--)
        {
            if(board2048[r][c] != 0)
            {
                temp[idx--] =
                    board2048[r][c];
            }
        }

        for(int i=3;i>0;i--)
        {
            if(temp[i] &&
               temp[i] == temp[i-1])
            {
                temp[i] *= 2;

                for(int j=i-1;j>0;j--)
                    temp[j] = temp[j-1];

                temp[0] = 0;
            }
        }

        for(int c=0;c<4;c++)
        {
            if(board2048[r][c] != temp[c])
                moved = true;

            board2048[r][c] = temp[c];
        }
    }

    return moved;
}

bool moveUp()
{
    bool moved = false;

    for(int c=0;c<4;c++)
    {
        uint16_t temp[4] = {0};

        int idx = 0;

        for(int r=0;r<4;r++)
        {
            if(board2048[r][c])
                temp[idx++] =
                board2048[r][c];
        }

        for(int i=0;i<3;i++)
        {
            if(temp[i] &&
               temp[i] == temp[i+1])
            {
                temp[i] *= 2;

                for(int j=i+1;j<3;j++)
                    temp[j] = temp[j+1];

                temp[3] = 0;
            }
        }

        for(int r=0;r<4;r++)
        {
            if(board2048[r][c] != temp[r])
                moved = true;

            board2048[r][c] = temp[r];
        }
    }

    return moved;
}

bool moveDown()
{
    bool moved = false;

    for(int c=0;c<4;c++)
    {
        uint16_t temp[4] = {0};

        int idx = 3;

        for(int r=3;r>=0;r--)
        {
            if(board2048[r][c])
                temp[idx--] =
                board2048[r][c];
        }

        for(int i=3;i>0;i--)
        {
            if(temp[i] &&
               temp[i] == temp[i-1])
            {
                temp[i] *= 2;

                for(int j=i-1;j>0;j--)
                    temp[j] = temp[j-1];

                temp[0] = 0;
            }
        }

        for(int r=0;r<4;r++)
        {
            if(board2048[r][c] != temp[r])
                moved = true;

            board2048[r][c] = temp[r];
        }
    }

    return moved;
}

void run2048()
{
    memset(board2048,0,sizeof(board2048));

    addRandomTile();
    addRandomTile();

    draw2048();

    while(true)
    {
        bool moved = false;

        if(digitalRead(BTN_BACK)==LOW)
        {
            waitRelease(BTN_BACK);
            return;
        }

        if(digitalRead(BTN_L)==LOW)
        {
            waitRelease(BTN_L);
            moved = moveLeft();
        }

        if(digitalRead(BTN_R)==LOW)
        {
            waitRelease(BTN_R);
            moved = moveRight();
        }

        if(digitalRead(BTN_UP)==LOW)
        {
            waitRelease(BTN_UP);
            moved = moveUp();
        }

        if(digitalRead(BTN_DOWN)==LOW)
        {
            waitRelease(BTN_DOWN);
            moved = moveDown();
        }

        if(moved)
        {
            addRandomTile();
            draw2048();
        }

        if(!canMove())
        {
            tft.fillScreen(COLOR_BG);

            drawHeader("GAME OVER");

            tft.setTextColor(COLOR_WHITE);
            tft.setTextSize(2);

            tft.setCursor(50,110);
            tft.print("No Moves!");

            delay(2000);

            return;
        }

        delay(20);
    }
}

const char* gameItems[] =
{
    "Snake",
    "Pong",
    "Flappy Bird",
    "2048",
    "Reaction Test",
    "DOOM"
};

const int GAME_COUNT =
sizeof(gameItems) / sizeof(gameItems[0]);



// ================= GAMES MENU LOGIC =================

void handleGamesMenu()
{
    static int selectedGame = 0;
    static int gameTop = 0;

    static int previousGame = -1;
    static int previousTop = -1;

    bool changed = false;


    // UP

    if(digitalRead(BTN_UP) == LOW)
    {
        waitRelease(BTN_UP);

        selectedGame--;

        if(selectedGame < 0)
            selectedGame = GAME_COUNT - 1;

        if(selectedGame < gameTop)
            gameTop = selectedGame;

        if(selectedGame >= gameTop + 3)
            gameTop = selectedGame - 2;

        changed = true;
    }


    // DOWN

    if(digitalRead(BTN_DOWN) == LOW)
    {
        waitRelease(BTN_DOWN);

        selectedGame++;

        if(selectedGame >= GAME_COUNT)
            selectedGame = 0;

        if(selectedGame < gameTop)
            gameTop = selectedGame;

        if(selectedGame >= gameTop + 3)
            gameTop = selectedGame - 2;

        changed = true;
    }


    // SELECT

    if(digitalRead(BTN_SELECT) == LOW)
    {
        waitRelease(BTN_SELECT);

        switch(selectedGame)
        {
            case 0:
                waitRelease(BTN_SELECT);
                runLoop(runSnake);
                break;

            case 1:
                waitRelease(BTN_SELECT);
                runLoop(runPong);
                break;

            case 2:
                waitRelease(BTN_SELECT);
                runFlappyBird();
                break;

            case 3:
                run2048();
                break;

            case 4:
                waitRelease(BTN_SELECT);
                runLoop(runReactionTest);

                break;

            case 5:
                waitRelease(BTN_SELECT);
                runLoop(handleDoomLite);
                break;
        }

        changed = true;
    }


    if(gameTop > GAME_COUNT - 3)
        gameTop = max(0, GAME_COUNT - 3);


    if(
        changed ||
        selectedGame != previousGame ||
        gameTop != previousTop
    )
    {
        drawGamesMenu(
            selectedGame,
            gameTop,
            previousGame,
            previousTop
        );

        previousGame = selectedGame;
        previousTop = gameTop;
    }
}





// ================= GAMES MENU DRAW =================

void drawGamesMenu(
    int selectedGame,
    int gameTop,
    int previousGame,
    int previousTop
)
{
    (void)previousGame;
    (void)previousTop;

    tft.fillScreen(COLOR_BG);

    drawHeader("GAMES");


    for(int i = 0; i < 3; i++)
    {
        int index =
        gameTop + i;

        if(index >= GAME_COUNT)
            break;

        int y =
        50 + i * 55;


        if(index == selectedGame)
        {
            tft.fillRoundRect(
                15,
                y,
                190,
                42,
                8,
                COLOR_ACCENT
            );

            tft.setTextColor(
                COLOR_SELECTED
            );
        }
        else
        {
            tft.drawRoundRect(
                15,
                y,
                190,
                42,
                8,
                COLOR_ACCENT
            );

            tft.setTextColor(
                COLOR_WHITE
            );
        }


        tft.setTextSize(2);

        tft.setCursor(
            28,
            y + 12
        );

        tft.print(
            gameItems[index]
        );
    }


    // scrollbar track

    tft.drawRoundRect(
        220,
        45,
        10,
        170,
        4,
        COLOR_ACCENT
    );


    int barHeight =
    max(
        20,
        170 / GAME_COUNT
    );


    int barPos =
    map(
        selectedGame,
        0,
        GAME_COUNT - 1,
        45,
        45 + 170 - barHeight
    );


    tft.fillRoundRect(
        221,
        barPos,
        8,
        barHeight,
        4,
        COLOR_ACCENT
    );
}
