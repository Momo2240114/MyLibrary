#pragma once
#include "ImCurveEdit.h"

static ImVec2 operator+(const ImVec2& a, const ImVec2& b) { return ImVec2(a.x + b.x, a.y + b.y); }
static ImVec2 operator-(const ImVec2& a, const ImVec2& b) { return ImVec2(a.x - b.x, a.y - b.y); }
static ImVec2 operator*(const ImVec2& a, float b) { return ImVec2(a.x * b, a.y * b); }
static ImVec2 operator/(const ImVec2& a, float b) { return ImVec2(a.x / b, a.y / b); }
