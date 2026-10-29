
#pragma once
#include <Arduino.h>

enum Card : int8_t {
  CARD_UNKNOWN = -1,
  CARD_RED = 0,
  CARD_ORANGE,
  CARD_PINK,
  CARD_YELLOW,
  CARD_GREEN,
  CARD_BLUE,
  CARD_PURPLE,
  CARD_WHITE,
  CARD_BLACK,
  CARD_EMPTY = 9
};

constexpr uint8_t CARD_COUNT = 9;

inline const char* cardName(Card card) {
  switch (card) {
    case CARD_RED: return "RED";
    case CARD_ORANGE: return "ORANGE";
    case CARD_PINK: return "PINK";
    case CARD_YELLOW: return "YELLOW";
    case CARD_GREEN: return "GREEN";
    case CARD_BLUE: return "BLUE";
    case CARD_PURPLE: return "PURPLE";
    case CARD_WHITE: return "WHITE";
    case CARD_BLACK: return "BLACK";
    case CARD_EMPTY: return "EMPTY";
    default: return "UNKNOWN";
  }
}

enum RunState : uint8_t {
  ST_CALIBRATE,
  ST_BG_CONFIRM,
  ST_WAIT_EMPTY,
  ST_READY,
  ST_GRANTED,
  ST_DENIED,
  ST_STATIC,
  ST_AMBIENT
};

enum OutputKind : uint8_t {
  OUT_OFF,
  OUT_DENIED,
  OUT_GRANTED,
  OUT_STATIC,
  OUT_AMBIENT,
  OUT_STANDBY
};
