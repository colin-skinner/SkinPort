#pragma once

#include <Arduino.h>

#ifdef DEBUG_MODE
#define DEBUG Serial.print
#define DEBUGLN Serial.println
// #define DEBUG(val, encoding) Serial.print(val, encoding)
#define DEBUG_FLUSH() Serial.flush()
// #define DEBUGLN(val) Serial.println(val)
// #define DEBUGLN(val, encoding) Serial.println(val, encoding)
#else
#define DEBUG
#define DEBUG_FLUSH()
#define DEBUGLN(val)
#endif

enum class CoordFrame {
    Sensor,
    Body,
    Launch,
    Earth
};

#define RETURN_FALSE_IF_FALSE(condition) if (!(condition)) {return false;}

#define RETURN_WITH_CODE_IF_FALSE(code, condition) if (!(condition)) {return code;}

#define RETURN_IF_FALSE(condition) if (condition == false) {return false;}

#define DELETE_COPY_AND_ASSIGNMENT(Class) Class(const Class&) = delete; Class& operator=(const Class&) = delete;

#define DEG2RAD PI / 180.0f
#define RAD2DEG 180.0f / PI
