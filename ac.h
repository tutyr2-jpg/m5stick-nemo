// AC-B-Gone + Air Conditioner IR control
// Ported from Bruce (IRremoteESP8266 IRac) to Nemo's single-file sketch style.
// Interactive power/temp/mode/fan control + B-Gone brute-force OFF.
// Brands: Samsung, LG, Sharp, Panasonic, Mitsubishi, Daikin, Toshiba, Philips.

#include <IRac.h>

#define AC_STATE_MIN 16
#define AC_STATE_MAX 30

// AC modes list order must match IRac decode mapping
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
  { "Philips",    decode_type_t::PHILCO_AC },
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

void ac_set_state_by_brand(std_ac_state_t* st, decode_type_t proto) {
  // only safe mode/temp defaults; brand already picked
  st->protocol = proto;
  st->power = ac_power;
  st->celsius = true;
  st->degrees = ac_temp;
  st->mode = std_mode_t::kAuto;
  switch (ac_mode) {
    case 0: st->mode = std_mode_t::kAuto; break;
    case 1: st->mode = std_mode_t::kCool; break;
    case 2: st->mode = std_mode_t::kDry;  break;
    case 3: st->mode = std_mode_t::kFan;  break;
    case 4: st->mode = std_mode_t::kHeat; break;
  }
  switch (ac_fan) {
    case 0: st->fanspeed = std_fanspeed_t::kAuto; break;
    case 1: st->fanspeed = std_fanspeed_t::kMin;  break;
    case 2: st->fanspeed = std_fanspeed_t::kMedium; break;
    case 3: st->fanspeed = std_fanspeed_t::kMax;  break;
  }
}

void ac_send_current() {
  ac_init();
  std_ac_state_t st;
  ac_set_state_by_brand(&st, ac_brands[ac_brand_idx].protocol);
  ac_irac->send(&st);
  digitalWrite(IRLED, M5LED_OFF);
}

// ---- drawing ----
void ac_draw() {
  DISP.fillScreen(BGCOLOR);
  DISP.setTextSize(SMALL_TEXT);
  DISP.setCursor(0, 0);
  DISP.println("A/C Remote");
  DISP.setTextSize(TINY_TEXT);
  DISP.println(" ");

  DISP.print("Marca: "); DISP.println(ac_brands[ac_brand_idx].name);
  DISP.print("Ligado: "); DISP.println(ac_power ? "ON" : "OFF");
  DISP.print("Temp: "); DISP.print(ac_temp); DISP.println("C");
  DISP.print("Modo: "); DISP.println(ac_modes[ac_mode]);
  DISP.print("Fan:  "); DISP.println(ac_fans[ac_fan]);
  DISP.println(" ");
  DISP.println("[OK] Enviar  [MENU] ajusta");
}

// ---- menu handling ----
// The MENU-driven selector entry in the main menu jumps to command 30.
// This loop alternates: next cycles fields, select edits/sends.

enum ACField { AC_BRAND, AC_POWER, AC_TEMP, AC_MODE, AC_FAN, AC_SEND, AC_FIELDS };
static uint8_t ac_field = AC_BRAND;

void ac_menu_draw() {
  DISP.fillScreen(BGCOLOR);
  DISP.setTextSize(SMALL_TEXT);
  DISP.setCursor(0, 0);
  DISP.println("A/C Remote");
  DISP.setTextSize(TINY_TEXT);
  const char* fields[] = {
    "Marca", "Ligado", "Temp", "Modo", "Fan", ">> ENVIAR <<"
  };
  const char* values[] = {
    ac_brands[ac_brand_idx].name,
    ac_power ? "ON" : "OFF",
    "",
    ac_modes[ac_mode],
    ac_fans[ac_fan],
    ""
  };
  char tempbuf[8];
  snprintf(tempbuf, sizeof(tempbuf), "%dC", ac_temp);
  values[2] = tempbuf;

  for (uint8_t i = 0; i < AC_FIELDS; i++) {
    if (i == ac_field) DISP.setTextColor(TFT_GREENYELLOW, BGCOLOR);
    else DISP.setTextColor(FGCOLOR, BGCOLOR);
    DISP.print(fields[i]);
    if (values[i][0]) { DISP.print(": "); DISP.println(values[i]); }
    else DISP.println("");
  }
  DISP.setTextColor(FGCOLOR, BGCOLOR);
}

void ac_bgone() {
  // Brute-force OFF across brands
  ac_init();
  DISP.fillScreen(BGCOLOR);
  DISP.setTextSize(SMALL_TEXT);
  DISP.setCursor(0, 0);
  DISP.println("AC-B-Gone");
  DISP.setTextSize(TINY_TEXT);
  DISP.println("Enviando OFF em todas as marcas...");
  for (uint8_t i = 0; i < ac_brands_count; i++) {
    DISP.print(ac_brands[i].name);
    DISP.println(" OFF");
    std_ac_state_t st;
    st.protocol = ac_brands[i].protocol;
    st.power = false;
    st.celsius = true;
    st.degrees = 24;
    st.mode = std_mode_t::kAuto;
    st.fanspeed = std_fanspeed_t::kAuto;
    ac_irac->send(&st);
    digitalWrite(IRLED, M5LED_OFF);
    delay(300);
  }
  DISP.println("Feito. [OK] voltar.");
  while (true) {
    if (check_select_press()) {
      rstOverride = false;
      isSwitching = true;
      current_proc = 1;
      return;
    }
    delay(50);
  }
}

void ac_setup() {
  ac_field = AC_BRAND;
  ac_menu_draw();
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
        DISP.fillScreen(BGCOLOR);
        DISP.setTextSize(SMALL_TEXT);
        DISP.setCursor(0, 0);
        DISP.println("Enviado!");
        delay(400);
        break;
    }
    ac_menu_draw();
    delay(150);
  }
}
