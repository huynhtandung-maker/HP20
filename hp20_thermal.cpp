#include "hp20_thermal.h"

#include <cmath>
#include "model.h"

namespace hp20 { namespace thermal {
namespace {

// ============================================================================
// HP20 THERMAL ENGINE - MAINTAINABLE BASELINE
// ============================================================================
// File responsibility:
//   1) Convert calibrated Temperature + RH into FEEL/Heat Index.
//   2) Smooth FEEL only for OLED/UI stability.
//   3) Map FEEL into local indoor thermal bands.
//   4) Return human-readable labels, meanings, actions and chart target range.
//
// IMPORTANT MAINTENANCE RULES
// ---------------------------------------------------------------------------
// - Sensor correction belongs in hp20_sensor.*
// - Heat Index formula belongs in model::heatIndex(...)
// - FEEL thresholds, band meaning and advice belong HERE.
// - OLED drawing/layout belongs in hp20_ui.*
// - Do NOT compensate a bad sensor by changing these FEEL thresholds.
//
// HOW TO TUNE FOR VIETNAM / SAI GON
// ---------------------------------------------------------------------------
// Only edit values inside the "LOCAL FEEL THRESHOLD TUNING AREA" below.
// Keep thresholds strictly increasing from low -> high.
// Change ONE boundary at a time, test for multiple days, then document the
// result in SCIENCE.md / VALIDATION.md / CHANGELOG.md.
// ============================================================================

State current;

// ---------------------------------------------------------------------------
// UI SMOOTHING - DISPLAY ONLY
// ---------------------------------------------------------------------------
// 22% newest FEEL + 78% previous displayed FEEL.
// This reduces screen jitter when DHT22 changes by a few tenths.
// It does NOT change cloud FEEL or reminder/buzzer FEEL because feelRaw stays
// untouched.
constexpr float UI_SMOOTH_NEW = 0.22f;
constexpr float UI_SMOOTH_OLD = 0.78f;

// ============================================================================
// LOCAL FEEL THRESHOLD TUNING AREA
// Profile intent: indoor study / desk work in hot-humid Southern Vietnam.
// Unit: degrees Celsius of calculated FEEL / Heat Index, NOT raw air temp.
//
// RULE FOR READING EACH LINE:
//   "FEEL < X" belongs to the band named in the comment.
//   When FEEL reaches X, it moves to the NEXT band.
//
// Example:
//   FEEL_COMFORT_MAX = 31.0
//   -> 28.0 <= FEEL < 31.0  => Comfort
//   -> FEEL >= 31.0         => WarmComfort
//
// These are interpretation thresholds, NOT medical diagnostic limits.
// ============================================================================

// FEEL below 25.0 C -> Cool.
// Meaning: noticeably cool relative to the local indoor working profile.
constexpr float FEEL_COOL_MAX = 25.0f;

// 25.0 <= FEEL < 28.0 C -> MildCool.
// Meaning: cool / fresh, usually easy to tolerate for desk work.
constexpr float FEEL_MILD_COOL_MAX = 28.0f;

// 28.0 <= FEEL < 31.0 C -> Comfort.
// Meaning: current default "target comfort" region starts here.
constexpr float FEEL_COMFORT_MAX = 31.0f;

// 31.0 <= FEEL < 33.0 C -> WarmComfort.
// Meaning: warm but still potentially comfortable for heat-acclimatized users.
constexpr float FEEL_WARM_COMFORT_MAX = 33.0f;

// 33.0 <= FEEL < 35.0 C -> SlightStuffy.
// Meaning: first noticeable transition from warm to slightly stuffy.
constexpr float FEEL_SLIGHT_STUFFY_MAX = 35.0f;

// 35.0 <= FEEL < 37.0 C -> MildStuffy.
// Meaning: mild stuffiness; airflow begins to matter more.
constexpr float FEEL_MILD_STUFFY_MAX = 37.0f;

// 37.0 <= FEEL < 39.0 C -> Stuffy.
// Meaning: clearly stuffy; prolonged desk work may feel less comfortable.
constexpr float FEEL_STUFFY_MAX = 39.0f;

// 39.0 <= FEEL < 41.0 C -> StuffyHot.
// Meaning: stuffy + hot; active cooling starts becoming more valuable.
constexpr float FEEL_STUFFY_HOT_MAX = 41.0f;

// 41.0 <= FEEL < 44.0 C -> Hot.
// Meaning: hot enough that the room is no longer considered good for long,
// focused desk work under this local profile.
constexpr float FEEL_HOT_MAX = 44.0f;

// 44.0 <= FEEL < 47.0 C -> HighHot.
// Meaning: high heat perception; cooling should become a priority.
constexpr float FEEL_HIGH_HOT_MAX = 47.0f;

// 47.0 <= FEEL < 50.0 C -> VeryHot.
// Meaning: very hot; prolonged exposure is increasingly undesirable.
constexpr float FEEL_VERY_HOT_MAX = 50.0f;

// 50.0 <= FEEL < 54.0 C -> HeatLoad.
// Meaning: high thermal load; cool the space and verify sensor placement if
// the reading conflicts strongly with real-world perception.
constexpr float FEEL_HEAT_LOAD_MAX = 54.0f;

// FEEL >= 54.0 C -> HighLoad.
// No upper constant is required because this is the final catch-all band.

// ---------------------------------------------------------------------------
// TARGET RANGE SHOWN ON OLED CHART
// ---------------------------------------------------------------------------
// This is a visual target, not a hard safety boundary.
// Keep LOW < HIGH and place both values inside the range you want the OLED
// chart to highlight as the preferred work/study zone.
constexpr float TARGET_FEEL_LOW  = 28.0f;
constexpr float TARGET_FEEL_HIGH = 33.0f;

// ---------------------------------------------------------------------------
// HUMIDITY / RAW-TEMPERATURE MODIFIERS FOR ADVICE TEXT ONLY
// ---------------------------------------------------------------------------
// These values DO NOT change the FEEL band.
// They only help explain WHY the room feels uncomfortable and choose a more
// useful action such as airflow vs dehumidification.
constexpr float DRIVER_TEMP_HIGH_C       = 33.0f; // raw/calibrated room temp
constexpr float RH_MODERATELY_HIGH       = 65.0f; // starts to feel humid
constexpr float RH_HIGH                  = 72.0f; // humidity clearly matters
constexpr float RH_VERY_HIGH             = 80.0f; // strong humidity burden
constexpr float RH_ACTION_DRY_PRIORITY   = 75.0f; // prefer airflow + drying
constexpr float TEMP_SENSOR_CHECK_HIGH_C = 34.0f; // prompt reference check

// Compile-time guardrails: if a future edit makes thresholds overlap or move
// backwards, compilation fails instead of silently creating broken bands.
static_assert(FEEL_COOL_MAX < FEEL_MILD_COOL_MAX, "FEEL thresholds must increase");
static_assert(FEEL_MILD_COOL_MAX < FEEL_COMFORT_MAX, "FEEL thresholds must increase");
static_assert(FEEL_COMFORT_MAX < FEEL_WARM_COMFORT_MAX, "FEEL thresholds must increase");
static_assert(FEEL_WARM_COMFORT_MAX < FEEL_SLIGHT_STUFFY_MAX, "FEEL thresholds must increase");
static_assert(FEEL_SLIGHT_STUFFY_MAX < FEEL_MILD_STUFFY_MAX, "FEEL thresholds must increase");
static_assert(FEEL_MILD_STUFFY_MAX < FEEL_STUFFY_MAX, "FEEL thresholds must increase");
static_assert(FEEL_STUFFY_MAX < FEEL_STUFFY_HOT_MAX, "FEEL thresholds must increase");
static_assert(FEEL_STUFFY_HOT_MAX < FEEL_HOT_MAX, "FEEL thresholds must increase");
static_assert(FEEL_HOT_MAX < FEEL_HIGH_HOT_MAX, "FEEL thresholds must increase");
static_assert(FEEL_HIGH_HOT_MAX < FEEL_VERY_HOT_MAX, "FEEL thresholds must increase");
static_assert(FEEL_VERY_HOT_MAX < FEEL_HEAT_LOAD_MAX, "FEEL thresholds must increase");
static_assert(TARGET_FEEL_LOW < TARGET_FEEL_HIGH, "Target FEEL range must be valid");

}  // namespace

// ============================================================================
// STATE UPDATE
// ============================================================================

void reset() {
  current = State{};
}

void update(const sensor::Reading& env) {
  // Invalid sensor data means the entire thermal state is invalid.
  if (!env.valid || !std::isfinite(env.tempC) || !std::isfinite(env.rh)) {
    reset();
    return;
  }

  // CORE FORMULA: preserve the current project Heat Index implementation.
  // Input is already calibrated tempC/rh from hp20_sensor.*.
  current.feelRaw = model::heatIndex(env.tempC, env.rh);

  // Smooth only the value shown/interpreted by the UI layer.
  if (std::isfinite(current.feelRaw)) {
    if (!std::isfinite(current.feelUi)) {
      current.feelUi = float(current.feelRaw);
    } else {
      current.feelUi = UI_SMOOTH_NEW * float(current.feelRaw)
                     + UI_SMOOTH_OLD * current.feelUi;
    }
  } else {
    current.feelUi = NAN;
  }

  // Convert FEEL into exactly one semantic band.
  current.band = bandForFeel(current.feelUi);
}

const State& state() {
  return current;
}

// ============================================================================
// FEEL -> BAND MAPPING
// ============================================================================
// THIS is the only function that turns FEEL into a thermal state.
// To tune the local scale, edit the named constants near the top of this file,
// NOT the numbers inside this function.

Band bandForFeel(float hi) {
  if (!std::isfinite(hi)) return Band::NoData;

  // FEEL < 25.0 C -> Cool
  if      (hi < FEEL_COOL_MAX)          return Band::Cool;

  // 25.0 <= FEEL < 28.0 C -> MildCool
  else if (hi < FEEL_MILD_COOL_MAX)     return Band::MildCool;

  // 28.0 <= FEEL < 31.0 C -> Comfort
  else if (hi < FEEL_COMFORT_MAX)       return Band::Comfort;

  // 31.0 <= FEEL < 33.0 C -> WarmComfort
  else if (hi < FEEL_WARM_COMFORT_MAX)  return Band::WarmComfort;

  // 33.0 <= FEEL < 35.0 C -> SlightStuffy
  else if (hi < FEEL_SLIGHT_STUFFY_MAX) return Band::SlightStuffy;

  // 35.0 <= FEEL < 37.0 C -> MildStuffy
  else if (hi < FEEL_MILD_STUFFY_MAX)   return Band::MildStuffy;

  // 37.0 <= FEEL < 39.0 C -> Stuffy
  else if (hi < FEEL_STUFFY_MAX)        return Band::Stuffy;

  // 39.0 <= FEEL < 41.0 C -> StuffyHot
  else if (hi < FEEL_STUFFY_HOT_MAX)    return Band::StuffyHot;

  // 41.0 <= FEEL < 44.0 C -> Hot
  else if (hi < FEEL_HOT_MAX)           return Band::Hot;

  // 44.0 <= FEEL < 47.0 C -> HighHot
  else if (hi < FEEL_HIGH_HOT_MAX)      return Band::HighHot;

  // 47.0 <= FEEL < 50.0 C -> VeryHot
  else if (hi < FEEL_VERY_HOT_MAX)      return Band::VeryHot;

  // 50.0 <= FEEL < 54.0 C -> HeatLoad
  else if (hi < FEEL_HEAT_LOAD_MAX)     return Band::HeatLoad;

  // FEEL >= 54.0 C -> HighLoad
  else                                  return Band::HighLoad;
}

// ============================================================================
// BAND -> SHORT LABEL
// ============================================================================

const char* label(Band b) {
  switch (b) {
    case Band::Cool:          return "MAT";
    case Band::MildCool:      return "MAT DE CHIU";
    case Band::Comfort:       return "DE CHIU";
    case Band::WarmComfort:   return "AM DE CHIU";
    case Band::SlightStuffy:  return "HOI OI";
    case Band::MildStuffy:    return "OI NHE";
    case Band::Stuffy:        return "OI";
    case Band::StuffyHot:     return "OI NONG";
    case Band::Hot:           return "NONG";
    case Band::HighHot:       return "NONG CAO";
    case Band::VeryHot:       return "RAT NONG";
    case Band::HeatLoad:      return "TAI NHIET CAO";
    case Band::HighLoad:      return "TAI NHIET RAT CAO";
    default:                  return "CHO DU LIEU";
  }
}

// ============================================================================
// BAND -> HUMAN MEANING
// ============================================================================
// Keep this descriptive, not diagnostic. HP20 estimates environmental thermal
// perception; it does not measure body/core temperature.

const char* meaning(Band b) {
  switch (b) {
    case Band::Cool:
      return "Khong gian kha mat; neu ngoi lau co the thay lanh khi co gio AC truc tiep.";

    case Band::MildCool:
      return "Mat va de chiu; phu hop hoc tap, lam viec neu co the ban thay thoai mai.";

    case Band::Comfort:
      return "Vung de chiu cho cong viec tinh tai; chua co ly do de lam mat them.";

    case Band::WarmComfort:
      return "Am nhung van de chiu voi nhieu nguoi quen khi hau nong am Sai Gon.";

    case Band::SlightStuffy:
      return "Bat dau co cam giac oi; phong kin hoac it gio se lam cam giac ro hon.";

    case Band::MildStuffy:
      return "Oi nhe; lam viec lau co the nong lung, bi bi va giam thoai mai.";

    case Band::Stuffy:
      return "Oi ro; co the giam tap trung neu ngoi lau ma khong co luu thong khi.";

    case Band::StuffyHot:
      return "Oi nong; co the nhanh met hon, dac biet khi phong kin va do am cao.";

    case Band::Hot:
      return "Nong; dieu kien khong con toi uu cho hoc tap hay deep work keo dai.";

    case Band::HighHot:
      return "Nong cao; nen chu dong giam tai nhiet thay vi chi co chiu dung.";

    case Band::VeryHot:
      return "Rat nong; ngoi lau de met va kho duy tri hieu suat tap trung.";

    case Band::HeatLoad:
      return "Tai nhiet cao; can uu tien lam mat va doi chieu so do neu cam nhan khong khop.";

    case Band::HighLoad:
      return "Tai nhiet rat cao; can lam mat som va kiem tra vi tri DHT22 neu so do bat thuong.";

    default:
      return "Chua co du lieu tin cay de danh gia khong gian.";
  }
}

// ============================================================================
// RAW T/RH -> DRIVER EXPLANATION
// ============================================================================
// IMPORTANT: this function does NOT change the FEEL band.
// It only explains whether temperature, humidity, or both are likely driving
// the current sensation.

const char* driverText(float t, float rh) {
  if (!std::isfinite(t) || !std::isfinite(rh)) return "CHUA CO DU LIEU";

  // Both room temperature and humidity are high.
  if (t >= DRIVER_TEMP_HIGH_C && rh >= RH_ACTION_DRY_PRIORITY)
    return "NHIET + AM CUNG CAO";

  // Temperature is the dominant visible driver.
  if (t >= DRIVER_TEMP_HIGH_C)
    return "NHIET DO DANG CHI PHOI";

  // Humidity is extremely high.
  if (rh >= RH_VERY_HIGH)
    return "DO AM RAT CAO";

  // Humidity is clearly high.
  if (rh >= RH_HIGH)
    return "DO AM CAO";

  // Humidity is moderately elevated.
  if (rh >= RH_MODERATELY_HIGH)
    return "DO AM KHA CAO";

  // Neither T nor RH is individually extreme under this explanation layer.
  return "NHIET + AM O MUC VUA";
}

// ============================================================================
// BAND -> PRIMARY ACTION
// ============================================================================

const char* actionPrimary(Band b) {
  switch (b) {
    case Band::Cool:          return "GIU ON DINH";
    case Band::MildCool:      return "DUY TRI";
    case Band::Comfort:       return "DUY TRI";
    case Band::WarmComfort:   return "QUAT NHE NEU CAN";
    case Band::SlightStuffy:  return "TANG GIO NHE";
    case Band::MildStuffy:    return "TANG LUONG GIO";
    case Band::Stuffy:        return "TANG GIO + GIAM AM";
    case Band::StuffyHot:     return "LAM MAT NHE";
    case Band::Hot:           return "LAM MAT CHU DONG";
    case Band::HighHot:       return "UU TIEN LAM MAT";
    case Band::VeryHot:       return "LAM MAT SOM";
    case Band::HeatLoad:      return "LAM MAT + DOI CHIEU";
    case Band::HighLoad:      return "LAM MAT NGAY";
    default:                  return "KIEM TRA DHT22";
  }
}

// ============================================================================
// BAND + RAW T/RH -> DETAILED ACTION
// ============================================================================
// The band is still determined ONLY by FEEL.
// RH is used here only to choose the most useful response: airflow, drying,
// cooling, or a combination.

const char* actionDetail(Band b, float t, float rh) {
  if (!std::isfinite(t) || !std::isfinite(rh)) {
    return "Kiem tra DHT22, day noi, nguon cap va vi tri dat cam bien.";
  }

  switch (b) {
    case Band::Cool:
      return "Neu thay lanh, giam gio thoi truc tiep hoac tang setpoint AC mot chut.";

    case Band::MildCool:
      return "Giu dieu kien hien tai; khong can ha nhiet them neu dang thoai mai.";

    case Band::Comfort:
      return "Khong can can thiep; uu tien giu on dinh va tranh thay doi nhiet dot ngot.";

    case Band::WarmComfort:
      return rh >= RH_ACTION_DRY_PRIORITY
        ? "Neu bat dau oi, bat quat nhe hoac AC Dry truoc khi ha nhiet manh."
        : "Neu thay am, thu quat nhe; chua can bat AC manh.";

    case Band::SlightStuffy:
      return rh >= RH_ACTION_DRY_PRIORITY
        ? "Tang gio va giam am; AC Dry huu ich neu phong kin."
        : "Tang doi luu bang quat nhe hoac mo thong gio khi ben ngoai phu hop.";

    case Band::MildStuffy:
      return rh >= RH_ACTION_DRY_PRIORITY
        ? "Uu tien giam am + tang gio de bot bi va nong lung."
        : "Tang quat mot nac va giam cac nguon toa nhiet gan cho ngoi.";

    case Band::Stuffy:
      return rh >= RH_HIGH
        ? "Tang gio, giam am; xem lai phong kin, rem nang va nguon nhiet."
        : "Tang gio va lam mat nhe neu can lam viec lien tuc.";

    case Band::StuffyHot:
      return rh >= RH_HIGH
        ? "Dung quat ket hop AC Dry/lam mat; tranh de phong tich nhiet lau."
        : "Bat quat manh hon hoac AC de ha tai nhiet.";

    case Band::Hot:
      return "Chu dong lam mat; nghi ngan neu thay met, kho tap trung hoac nong buc.";

    case Band::HighHot:
      return "Uu tien vi tri mat hon, quat/AC; giam may moc va buc xa nhiet quanh cho ngoi.";

    case Band::VeryHot:
      return "Lam mat som; khong nen co chiu de hoc/lam viec sau trong thoi gian dai.";

    case Band::HeatLoad:
      return "Lam mat va doi chieu nhiet ke; neu phong van thay binh thuong, kiem tra vi tri DHT22.";

    case Band::HighLoad:
      return "Lam mat ngay; neu so do khong khop cam nhan, dat lai DHT22 xa ESP32/nguon nhiet.";

    default:
      return "Cho du lieu on dinh truoc khi dua ra khuyen nghi.";
  }
}

// ============================================================================
// SECONDARY TIP
// ============================================================================

const char* secondTip(Band b, float t, float rh) {
  if (!std::isfinite(t) || !std::isfinite(rh)) {
    return "DHT22 nen dat noi thoang, xa ESP32, nguon USB va luong gio AC truc tiep.";
  }

  // Very high measured room temperature -> verify against a reference sensor
  // if the number conflicts with what the room actually feels like.
  if (t >= TEMP_SENSOR_CHECK_HIGH_C) {
    return "T do duoc rat cao cho phong; neu cam giac khong khop, hay doi chieu nhiet ke tham chieu.";
  }

  // Very high humidity -> drying may be more useful than simply lowering AC.
  if (rh >= RH_VERY_HIGH) {
    return "RH rat cao: hut am/AC Dry co the huu ich hon chi ha setpoint nhiet do.";
  }

  // High humidity -> airflow + drying can improve comfort.
  if (rh >= RH_HIGH) {
    return "RH cao: tang luu thong khi va giam am thuong cho cam giac de chiu hon.";
  }

  switch (b) {
    case Band::Cool:
    case Band::MildCool:
      return "Muc tieu la de chiu va on dinh, khong phai cang lanh cang tot.";

    case Band::Comfort:
      return "Neu dang tap trung tot, khong can thay doi chi vi chi so dao dong nho.";

    case Band::WarmComfort:
      return "Nguoi quen khi hau Sai Gon co the chap nhan am hon khi co luong gio tot.";

    case Band::SlightStuffy:
    case Band::MildStuffy:
      return "Uu tien quat truoc; chi ha nhiet manh khi cam giac van oi hoac lam viec lau.";

    case Band::Stuffy:
    case Band::StuffyHot:
      return "Neu phong kin, kiem tra them rem nang, may tinh va cac nguon toa nhiet.";

    default:
      return "Theo doi cam nhan ca nhan cung voi xu huong T/RH, khong chi mot lan do.";
  }
}

// ============================================================================
// BAND -> RELATION TO OLED TARGET RANGE
// ============================================================================

const char* chartRelation(Band b) {
  switch (b) {
    case Band::Cool:
    case Band::MildCool:
      return "THAP HON MUC TIEU";

    case Band::Comfort:
      return "TRONG MUC TIEU";

    case Band::WarmComfort:
      return "SAT MEP MUC TIEU";

    case Band::SlightStuffy:
    case Band::MildStuffy:
      return "CAO HON MUC TIEU NHE";

    case Band::Stuffy:
    case Band::StuffyHot:
      return "CAO HON MUC TIEU";

    case Band::Hot:
    case Band::HighHot:
      return "CAO HON MUC TIEU RO";

    default:
      return "VUOT XA MUC TIEU";
  }
}

// ============================================================================
// OLED TARGET RANGE ACCESSORS
// ============================================================================
// UI reads these functions; UI should never hard-code the thermal target.

float optimalLowFeel() {
  return TARGET_FEEL_LOW;
}

float optimalHighFeel() {
  return TARGET_FEEL_HIGH;
}

} }  // namespace hp20::thermal
