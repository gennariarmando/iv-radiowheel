#pragma once
#include "extensions/Screen.h"
#include "CViewport.h"
#include "Rage.h"
#include "CTimer.h"

#define FLASH_ITEM(on, off) (CTimer::m_snTimeInMilliseconds % on + off < on)

#define DEFAULT_SCREEN_WIDTH 1280.0f
#define DEFAULT_SCREEN_HEIGHT 720.0f
#define DEFAULT_SCREEN_ASPECT_RATIO (DEFAULT_SCREEN_WIDTH / DEFAULT_SCREEN_HEIGHT)

static float GetAspectRatio() {
#ifdef GTA3
    float as = *(float*)0x5F53C0;
#elif GTAVC
    float as = *(float*)0x94DD38;
#elif GTASA
    float as = CDraw::ms_fAspectRatio;
#elif GTAIV
    float as = TheViewport.FindAspectRatio(false);
#endif
    return as;
}

#define SCREEN_ASPECT_RATIO GetAspectRatio()

static float ScaleMult = 1.0f;

static void SetScaleMult(float val = 1.0f) {
    ScaleMult = val;
}

static float ScaleX(float x) {
    float f = ((x) * (float)SCREEN_WIDTH / DEFAULT_SCREEN_WIDTH) * DEFAULT_SCREEN_ASPECT_RATIO / SCREEN_ASPECT_RATIO;
    return f * ScaleMult;
}

static float ScaleXKeepCentered(float x) {
    float f = ((SCREEN_WIDTH == DEFAULT_SCREEN_WIDTH) ? (x) : (SCREEN_WIDTH - ScaleX(DEFAULT_SCREEN_WIDTH)) / 2 + ScaleX((x)));
    return f * ScaleMult;
}

static float ScaleY(float y) {
    float f = ((y) * (float)SCREEN_HEIGHT / DEFAULT_SCREEN_HEIGHT);
    return f * ScaleMult;
}

static float ScaleW(float w) {
    float f = ((w) * (float)SCREEN_WIDTH / DEFAULT_SCREEN_WIDTH) * DEFAULT_SCREEN_ASPECT_RATIO / SCREEN_ASPECT_RATIO;
    return f;
}

static float ScaleH(float h) {
    float f = ((h) * (float)SCREEN_HEIGHT / DEFAULT_SCREEN_HEIGHT);
    return f * ScaleMult;
}
