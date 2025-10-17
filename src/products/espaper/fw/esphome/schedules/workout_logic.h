// Centralized helpers for workout schedule logic
#pragma once

struct ScheduleConfig {
  int c1_h;
  int c1_m;
  int c2_h;
  int c2_m;
  int slot_min;
};

inline void advance_fake_time(int &hour, int &minute, int &second, int add_seconds) {
  int total_seconds = second + add_seconds;
  int new_hour = hour + (total_seconds / 3600);
  int new_minute = minute + ((total_seconds % 3600) / 60);
  int new_second = total_seconds % 60;

  new_minute += new_second / 60;
  new_second %= 60;
  new_hour += new_minute / 60;
  new_minute %= 60;
  new_hour %= 24;

  hour = new_hour;
  minute = new_minute;
  second = new_second;
}

inline int get_circuit_idx(int hour, int minute, const ScheduleConfig &cfg) {
  const int total = hour * 60 + minute;
  const int c2_start = cfg.c2_h * 60 + cfg.c2_m;
  return total >= c2_start ? 1 : 0;
}

inline int get_workout_idx(int hour, int minute, int circuit_idx, const ScheduleConfig &cfg) {
  const int total = hour * 60 + minute;
  const int start = (circuit_idx == 0) ? (cfg.c1_h * 60 + cfg.c1_m) : (cfg.c2_h * 60 + cfg.c2_m);
  const int delta = total - start;
  if (delta < cfg.slot_min) return 0;
  if (delta < 2 * cfg.slot_min) return 1;
  return 2;
}

inline int minute_hand_value(int minute) { return minute; }
inline int hour_hand_value(int hour, int minute) { return (hour % 12) * 60 + minute; }

inline std::string build_title(int circuit_idx, int workout_idx) {
  char buf[32];
  snprintf(buf, sizeof(buf), "Circuit %d Workout %d", circuit_idx + 1, workout_idx + 1);
  return std::string(buf);
}

inline std::string build_date(int month, int day_of_month) {
  static const char *const mon_names[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                          "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  char buf[8];
  int m = month;
  if (m < 1) m = 1;
  if (m > 12) m = 12;
  snprintf(buf, sizeof(buf), "%s %2d", mon_names[m - 1], day_of_month);
  return std::string(buf);
}

inline std::string build_day(int dow) {
  static const char *const day_names[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  int d = dow;
  if (d < 1) d = 1;
  if (d > 7) d = 7;
  return std::string(day_names[d - 1]);
}

inline const char *get_workout_name(
    int circuit_idx, int idx,
    const char *c1w1, const char *c1w2, const char *c1w3,
    const char *c2w1, const char *c2w2, const char *c2w3) {
  if (circuit_idx == 0) {
    switch (idx % 3) {
      case 0: return c1w1;
      case 1: return c1w2;
      default: return c1w3;
    }
  } else {
    switch (idx % 3) {
      case 0: return c2w1;
      case 1: return c2w2;
      default: return c2w3;
    }
  }
}
