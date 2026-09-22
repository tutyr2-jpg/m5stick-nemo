// AC-B-Gone + Air Conditioner IR control
// Ported from Bruce (IRremoteESP8266 IRac) to Nemo's single-file sketch style.
// Interactive power/temp/mode/fan control + FULL BLAST + B-Gone brute-force OFF.
// Brands: Samsung, LG, Sharp, Panasonic, Mitsubishi, Daikin, Toshiba, Gree.

#include <IRac.h>

// Nemo's sketch defines these after the .h includes (Arduino prototype
// generation doesn't see .h-internal call sites), so forward-declare.
bool check_next_press();
bool check_select_press();

#define AC_STATE_MIN 16
#define AC_STATE_MAX 30

const char* ac_modes[] = { "Auto", "Cool", "Dry", "Fan", "Heat" };
const char* ac_fans[] = { "Auto", "Min", "Med", "Max" };

struct ACBrandEntry {
  const char* name;
  decode_type_t protocol;
};

const ACBrandEntry ac_brands[] = {
  { "Samsung",    decode_type_t::SAMSUNG_AC },
  { "LG",         decode_type_t::LG2 },
  { "Sharp",      decode_type_t::SHARP_AC },
  { "Panasonic",  decode_type_t::PANASONIC_AC },
  { "Mitsubishi", decode_type_t::MITSUBISHI_AC },
  { "Daikin",     decode_type_t::DAIKIN },
  { "Toshiba",    decode_type_t::TOSHIBA_AC },
  { "Gree",       decode_type_t::GREE },
};
const int ac_brands_count = sizeof(ac_brands) / sizeof(ac_brands[0]);

// Interactive state
static uint8_t  ac_brand_idx = 0;
static uint8_t  ac_temp = 24;
static uint8_t  ac_mode = 1;   // Cool
static uint8_t  ac_fan = 0;    // Auto
static bool     ac_power = true;
static IRac*    ac_irac = nullptr;

// ---- helpers ----
void ac_init() {
  if (!ac_irac) ac_irac = new IRac(IRLED);
}

// Wait until all buttons are released so the press that opened a
// screen isn't immediately re-read as input inside that screen.
void ac_wait_release() {
#if defined(KB)
  delay(300);  // keyboard path debounces in M5Cardputer.update()
#else
  while (digitalRead(M5_BUTTON_HOME) == LOW || digitalRead(M5_BUTTON_RST) == LOW) {
    delay(20);
  }
  delay(50);
#endif
}

void ac_set_state_by_brand(stdAc::state_t* st, decode_type_t proto) {
  st->protocol = proto;
  st->power = ac_power;
  st->celsius = true;
  st->degrees = ac_temp;
  switch (ac_mode) {
    case 0: st->mode = stdAc::opmode_t::kAuto; break;
    case 1: st->mode = stdAc::opmode_t::kCool; break;
    case 2: st->mode = stdAc::opmode_t::kDry;  break;
    case 3: st->mode = stdAc::opmode_t::kFan;  break;
    case 4: st->mode = stdAc::opmode_t::kHeat; break;
  }
  switch (ac_fan) {
    case 0: st->fanspeed = stdAc::fanspeed_t::kAuto;   break;
    case 1: st->fanspeed = stdAc::fanspeed_t::kMin;    break;
    case 2: st->fanspeed = stdAc::fanspeed_t::kMedium; break;
    case 3: st->fanspeed = stdAc::fanspeed_t::kMax;    break;
  }
}

void ac_send_current() {
  ac_init();
  stdAc::state_t st;
  IRac::initState(&st);
  ac_set_state_by_brand(&st, ac_brands[ac_brand_idx].protocol);
  ac_irac->sendAc(st);
  digitalWrite(IRLED, M5LED_OFF);
}

void ac_send_state(stdAc::state_t* st) {
  ac_init();
  ac_irac->sendAc(*st);
  digitalWrite(IRLED, M5LED_OFF);
}

// FULL BLAST: power ON, Cool, lowest temp, max fan
void ac_full_blast() {
  ac_power = true;
  ac_mode = 1;              // Cool
  ac_temp = AC_STATE_MIN;   // 16C
  ac_fan = 3;               // Max
  ac_send_current();
}

// ---- A/C Remote menu ----
enum ACField { AC_BRAND, AC_POWER, AC_TEMP, AC_MODE, AC_FAN, AC_SEND, AC_BLAST, AC_FIELDS };
static uint8_t ac_field = AC_BRAND;

void ac_menu_draw() {
  DISP.fillScreen(BGCOLOR);
  DISP.setTextSize(SMALL_TEXT);
  DISP.setCursor(0, 0);
  DISP.println("A/C Remote");
  DISP.setTextSize(TINY_TEXT);
  const char* fields[] = {
    "Marca", "Ligado", "Temp", "Modo", "Fan", ">> ENVIAR <<", "!! FULL BLAST !!"
  };
  char tempbuf[8];
  snprintf(tempbuf, sizeof(tempbuf), "%dC", ac_temp);
  const char* values[] = {
    ac_brands[ac_brand_idx].name,
    ac_power ? "ON" : "OFF",
    tempbuf,
    ac_modes[ac_mode],
    ac_fans[ac_fan],
    "",
    ""
  };

  for (uint8_t i = 0; i < AC_FIELDS; i++) {
    if (i == ac_field) DISP.setTextColor(TFT_GREENYELLOW, BGCOLOR);
    else DISP.setTextColor(FGCOLOR, BGCOLOR);
    DISP.print(fields[i]);
    if (values[i][0]) { DISP.print(": "); DISP.println(values[i]); }
    else DISP.println("");
  }
  DISP.setTextColor(FGCOLOR, BGCOLOR);
}

void ac_setup() {
  rstOverride = true;   // side button is navigation here, not "back to menu"
  ac_field = AC_BRAND;
  ac_menu_draw();
  ac_wait_release();
}

void ac_loop() {
  if (check_next_press()) {
    ac_field = (ac_field + 1) % AC_FIELDS;
    ac_menu_draw();
    delay(200);
  }
  if (check_select_press()) {
    switch (ac_field) {
      case AC_BRAND:
        ac_brand_idx = (ac_brand_idx + 1) % ac_brands_count;
        break;
      case AC_POWER:
        ac_power = !ac_power;
        break;
      case AC_TEMP:
        ac_temp++;
        if (ac_temp > AC_STATE_MAX) ac_temp = AC_STATE_MIN;
        break;
      case AC_MODE:
        ac_mode = (ac_mode + 1) % 5;
        break;
      case AC_FAN:
        ac_fan = (ac_fan + 1) % 4;
        break;
      case AC_SEND:
        ac_send_current();
        break;
      case AC_BLAST:
        ac_full_blast();
        break;
    }
    if (ac_field == AC_SEND || ac_field == AC_BLAST) {
      DISP.fillScreen(BGCOLOR);
      DISP.setTextSize(SMALL_TEXT);
      DISP.setCursor(0, 0);
      DISP.println("Enviado!");
      delay(400);
    }
    ac_menu_draw();
    delay(150);
  }
}

// ---- AC-B-Gone: non-blocking state machine ----
// setup draws the screen; loop sends one brand per tick so the UI
// (and the global menu-exit button) stays responsive the whole time.
static uint8_t  ac_bg_idx = 0;
static bool     ac_bg_done = false;
static uint32_t ac_bg_last = 0;

void ac_bgone_setup() {
  rstOverride = true;
  ac_bg_idx = 0;
  ac_bg_done = false;
  ac_bg_last = 0;
  DISP.fillScreen(BGCOLOR);
  DISP.setTextSize(SMALL_TEXT);
  DISP.setCursor(0, 0);
  DISP.println("AC-B-Gone");
  DISP.setTextSize(TINY_TEXT);
  DISP.println("Enviando OFF em");
  DISP.println("todas as marcas...");
  DISP.println(" ");
  ac_wait_release();
}

void ac_bgone_loop() {
  if (!ac_bg_done) {
    if (millis() - ac_bg_last > 400) {
      ac_bg_last = millis();
      DISP.print(ac_brands[ac_bg_idx].name);
      DISP.println(" OFF");
      stdAc::state_t st;
      IRac::initState(&st);
      st.protocol = ac_brands[ac_bg_idx].protocol;
      st.power = false;
      st.celsius = true;
      st.degrees = 24;
      st.mode = stdAc::opmode_t::kAuto;
      st.fanspeed = stdAc::fanspeed_t::kAuto;
      ac_send_state(&st);
      ac_bg_idx++;
      if (ac_bg_idx >= ac_brands_count) {
        ac_bg_done = true;
        DISP.println(" ");
        DISP.println("Feito. [OK] voltar.");
      }
    }
    return;
  }
  // done: front button exits (power/menu button exits via check_menu_press)
  if (check_select_press()) {
    rstOverride = false;
    isSwitching = true;
    current_proc = 1;
  }
}
