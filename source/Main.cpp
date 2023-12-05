#include "plugin.h"
#include "common.h"
#include "Rage.h"
#include "T_CB_Generic.h"
#include "CHud.h"
#include "CHudColours.h"
#include "CCutsceneMgr.h"
#include "CMenuManager.h"
#include "CFont.h"
#include "CText.h"
#include "CRadioHud.h"
#include "CPad.h"
#include "CControlMgr.h"
#include "CCamera.h"

#include "SpriteLoader.h"
#include "audRadioAudioEntity.h"

#include "Utility.h"

class __declspec(dllexport) RadioWheelIV {
public:
    static inline plugin::SpriteLoader m_SpriteLoader = {};
    static inline bool m_bShowRadioWheel = false;
    static inline rage::Vector2 m_vCursor = {};
    static inline rage::Vector2 m_vPrevCursor = {};
    static inline bool m_bRadioSelected = false;
    static inline float m_fCursorRadius = 0.0f;
    static inline uint32_t m_nTimePassedAfterKeyPress = 0;
    static inline uint32_t m_nTimePassedAfterQuickSwitch = 0;
    static inline uint32_t m_nTimePassedForDisplayingTrackNames = 0;
    static inline bool m_bDisableControls = false;

    static inline void UpdateCursor() {
        int32_t x, y;
        CPad::GetMouseInput(&x, &y);

        auto playa = FindPlayerPed(0);
        auto control = playa->GetControlFromPlayer();

        int32_t rsx = CPad::GetPad(0)->NewState.RightStickX;
        int32_t rsy = CPad::GetPad(0)->NewState.RightStickY;

        m_vPrevCursor = m_vCursor;
        if (CControl::m_UsingMouse) {
            float vel = 6.0f;
            m_vCursor.x += x * vel;
            m_vCursor.y += y * vel;
        }
        else {
            float vel = 6.0f;
            m_vCursor.x += rsx * vel;
            m_vCursor.y += rsy * vel;
        }

        m_vCursor = LimitPos(m_vCursor);
    }

    static inline void SetCursorToCurrentRadio(int32_t i, int32_t numIcons) {
        float x, y, s;
        GetIconCoords(i + 1, numIcons, x, y, s);
        m_vCursor = { x, y };
    }

    static inline rage::Vector2 GetCentreOfWheel() {
        rage::Vector2 res;
        res.x = SCREEN_WIDTH / 2;
        res.y = SCREEN_HEIGHT / 2;
        res.y -= ScaleY(32.0f);
        return res;
    }

    static inline rage::Vector2 LimitPos(rage::Vector2 pos) {
        rage::Vector2 centerPosition = GetCentreOfWheel();

        float dist = (pos - centerPosition).Magnitude();
        float angle = std::atan2(pos.y - centerPosition.y, pos.x - centerPosition.x);

        float radius = m_fCursorRadius;
        pos.x = centerPosition.x + radius * std::cos(angle);
        pos.y = centerPosition.y + radius * std::sin(angle);

        return pos;
    }

    static inline int32_t GetCurrentRadioStationFix() {
        int32_t currRadio = audRadioStation::ms_CurrRadioStationRoll;
        int32_t numStations = audRadioStation::GetNumStations() + 1;

        if (currRadio > numStations || currRadio < 0)
            currRadio = -1;

        return currRadio;
    }

    static inline void RetuneRadio(int32_t value) {
        if (GetCurrentRadioStationFix() != value) {
            if (value == -1) {
                g_RadioAudioEntity.TurnOff(0);
                g_RadioAudioEntity.m_PlayerVehicleRadioState = 3;
                audRadioAudioEntity::ms_IsMobilePhoneRadioActive = 0;
                g_RadioAudioEntity.m_PlayerVehicle->m_ForcePlayerStation = 1;
            }
            else {
                if (g_RadioAudioEntity.field_27 == 255 || !g_RadioAudioEntity.m_PlayerVehicleRadioState) {
                    if (!audRadioStation::ms_nState)
                        audRadioAudioEntity::TurnOn(0);

                    g_RadioAudioEntity.field_29 = 0;
                    g_RadioAudioEntity.field_27 = 0;

                    g_RadioAudioEntity.m_PlayerVehicleRadioState = 1;

                    g_RadioAudioEntity.m_PlayerVehicle->m_ForcePlayerStation = 0;
                }

                g_RadioAudioEntity.ReportSoundEvent("RADIO_RETUNE_BEEP", nullptr, -1, 0, 0);
                g_RadioAudioEntity.RetuneToStationIndex(value);
            }

            m_nTimePassedForDisplayingTrackNames = CTimer::GetTimeInMilliseconds() + 100;
        }

        audRadioStation::ms_CurrRadioStationRoll = value;
        audRadioStation::ms_CurrRadioStation = value;
    }

    static inline void GetIconCoords(int32_t i, int32_t numIcons, float& x, float& y, float& scale) {
        float s = 33.0f;
        float radius = (s / sin(M_PI / numIcons));
        float angle = (2 * M_PI / numIcons);

        scale = ScaleY(s);
        x = (SCREEN_WIDTH / 2) - ScaleX(radius * sinf((i * angle) - M_PI));
        y = (SCREEN_HEIGHT / 2) - ScaleY((radius * cosf((i * angle) - M_PI))) - scale;
        m_fCursorRadius = ScaleY(radius);
    }

    static bool CheckCollision(const rage::Vector2& point1, const rage::Vector2& point2, float radius) {
        float dist = (point2 - point1).Magnitude();
        return dist <= radius;
    }

    static inline void DrawIcon(const char* spriteName, int32_t pos, int32_t numIcons, rage::Color32 const& col, bool zoom) {
        float x, y, scale = 0.0f;
        GetIconCoords(pos + 1, numIcons, x, y, scale);

        if (zoom)
            scale += ScaleY(4.0f);
        else {
            if (!m_bRadioSelected && (m_vPrevCursor - m_vCursor).Magnitude() > 0.0f && CheckCollision(m_vCursor, { x, y }, ScaleY(24.0f))) {
                RetuneRadio(pos);
                m_bRadioSelected = true;
            }
        }

        auto sprite = m_SpriteLoader.GetSprite(spriteName);
        if (sprite) {
            sprite->SetRenderState();
            CSprite2d::Draw({ x - scale, y - scale, x + scale, y + scale }, col);
            CSprite2d::ClearRenderState();
        }
    }

    static inline void DrawRadioName(float x, float y, int32_t currRadio) {
        CFont::SetProportional(true);
        CFont::SetBackground(false, false);
        CFont::SetEdge(0.8f);
        CFont::SetFontStyle(FONT_HELVETICA);
        CFont::SetColor(CHudColours::Get(HUD_COLOUR_WHITE, 255));
        CFont::SetDropColor(CHudColours::Get(HUD_COLOUR_BLACK, 255));
        CFont::SetOrientation(ALIGN_CENTER);
        CFont::SetLineHeight(1.4f);
        CFont::SetScale(0.16f, 0.38f);
        CFont::SetWrapx(0.0f, 1.0f);

        if (currRadio == -1) {
            std::wstring str = plugin::ToLower(TheText.Get(0xA50CCD43, 0), 1);
            CFont::PrintString(x, y, str.c_str());
        }
        else {
            std::wstring stationName = TheText.Get(audRadioStation::GetName(currRadio, 1));

            if (m_nTimePassedForDisplayingTrackNames < CTimer::GetTimeInMilliseconds() && 
                audRadioStation::ms_CurrRadioStation == audRadioStation::ms_CurrRadioStationRoll) {
                int32_t id = g_RadioAudioEntity.GetAudibleMusicTrackTextId();
                if (id > 1) {
                    char buf[128];
                    sprintf(buf, "TM_82_%d", id);

                    std::wstring str = TheText.Get(buf);

                    // Remove ZiT! Spotted
                    int32_t pos = str.find(L" ");
                    if (pos != std::string::npos) {
                        pos = str.find(L" ", pos + 1);
                        if (pos != std::string::npos) {
                            str = str.erase(0, pos + 1);
                        }
                    }

                    // Replace ':' with ~n~
                    pos = str.find(L":");
                    if (pos != std::string::npos) {
                        str = str.replace(pos, 1, L"~n~");
                    }

                    stationName += L"~n~";
                    stationName += str;
                }
            }

            CFont::PrintString(x, y, stationName.c_str());
        }

        CFont::DrawFonts();
    }

    static inline void DrawQuickSwitch() {
        if (m_bShowRadioWheel)
            return;

        if (m_nTimePassedAfterQuickSwitch > CTimer::GetTimeInMilliseconds()) {
            int32_t currRadio = GetCurrentRadioStationFix();
            const char* name = nullptr;
            if (currRadio == -1)
                name = "radio_off";
            else
                name = audRadioStation::GetName(currRadio, 1);

            float x = SCREEN_WIDTH / 2;
            float y = ScaleY(152.5f);
            float scale = ScaleY(33.0f);
            auto sprite = m_SpriteLoader.GetSprite(name);
            if (sprite) {
                sprite->SetRenderState();
                CSprite2d::Draw({ x - scale, y - scale, x + scale, y + scale }, { 255, 255, 255, 255 });
                CSprite2d::ClearRenderState();
                sprite = nullptr;
            }

            scale += ScaleY(4.0f);
            sprite = m_SpriteLoader.GetSprite("radio_current");
            if (sprite) {
                sprite->SetRenderState();
                CSprite2d::Draw({ x - scale, y - scale, x + scale, y + scale }, GetColorForEpisode());
                CSprite2d::ClearRenderState();
            }

            DrawRadioName(0.5f, 0.288f, currRadio);
        }
    }

    static inline rage::Color32 GetColorForEpisode() {
        rage::Color32 col = CHudColours::Get(HUD_COLOUR_MENU_YELLOW_DARK, 255);
        switch (gGameEpisode) {
            case EPISODE_TBOGT:
                col = CHudColours::Get(HUD_COLOUR_MENU_BLUE, 255);
                break;
        }

        return col;
    }

    static inline void DrawWheel() {
        if (CCutsceneMgr::IsRunning())
            return;

        if (!m_bShowRadioWheel)
            return;

        int32_t currRadio = GetCurrentRadioStationFix();
        int32_t numStations = audRadioStation::GetNumStations() + 1;

        m_bRadioSelected = false;
        for (int32_t i = -1; i < numStations; i++) {
            const char* name = nullptr;
            if (i == -1)
                name = "radio_off";
            else
                name = audRadioStation::GetName(i, 1);

            DrawIcon(name, i, numStations, { 255, 255, 255, currRadio == i ? 255 : 50 }, false);
        }

        rage::Color32 col = GetColorForEpisode();
        DrawIcon("radio_current", currRadio, numStations, col, true);

        // Debug line
        //rage::Vector2 centerPosition;
        //centerPosition.x = SCREEN_WIDTH / 2;
        //centerPosition.y = SCREEN_HEIGHT / 2;
        //centerPosition.y -= ScaleY(32.0f);
        //CSprite2d::Draw(m_vCursor.x, m_vCursor.y, m_vCursor.x + 2.0f, m_vCursor.y + 2.0f,
        //                centerPosition.x, centerPosition.y,
        //                centerPosition.x + 2.0f, centerPosition.y + 2.0f,
        //                0.0f,
        //                0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, CHudColours::Get(HUD_COLOUR_RED, 255), 0);

        DrawRadioName(0.5f, 0.395f, currRadio);
    }

    static inline void __fastcall UpdatePlayerVehicleRadio(audRadioAudioEntity* _this, void*, int32_t timeInMs) {
        auto playa = FindPlayerPed(0);

        if (!playa)
            return;

        if (!audRadioAudioEntity::CanRetune() || !g_RadioAudioEntity.field_31 || !g_RadioAudioEntity.field_36)
            return;

        if (m_bDisableControls)
            return;

        auto control = playa->GetControlFromPlayer();
        bool keyOpenWheel = control->m_inputs[rage::INPUT_VEH_PREV_RADIO].m_nNewState;
        bool keyQuickSwitchNext = !control->m_inputs[rage::INPUT_VEH_NEXT_RADIO].m_nNewState && control->m_inputs[rage::INPUT_VEH_NEXT_RADIO].m_nOldState;
        bool keyQuickSwitchPrev = !control->m_inputs[rage::INPUT_VEH_PREV_RADIO].m_nNewState && control->m_inputs[rage::INPUT_VEH_PREV_RADIO].m_nOldState;

        if (keyOpenWheel) {
            if (m_nTimePassedAfterKeyPress > 250) {
                OpenCloseWheel(true);

                UpdateCursor();
            }
            else
                m_nTimePassedAfterKeyPress += (CTimer::GetTimeInMilliseconds() - CTimer::GetPreviousTimeInMilliseconds());
        }
        else {
            if (m_nTimePassedAfterKeyPress < 250) {
                int32_t curr = GetCurrentRadioStationFix();
                int32_t last = audRadioStation::GetNumStations() - 1;

                if (curr > last + 1)
                    curr = -1;

                if (keyQuickSwitchNext || keyQuickSwitchPrev) {
                    if (keyQuickSwitchNext)
                        curr++;

                    if (keyQuickSwitchPrev)
                        curr--;

                    if (curr > last)
                        curr = -1;

                    if (curr < -1)
                        curr = last;

                    if (curr != audRadioStation::ms_CurrRadioStationRoll) {
                        RetuneRadio(curr);
                        m_nTimePassedAfterQuickSwitch = 2000 + CTimer::GetTimeInMilliseconds();
                    }
                }
                m_nTimePassedAfterKeyPress = 0;
            }

            OpenCloseWheel(false);
        }
    }

    static inline void OpenCloseWheel(bool on) {
#pragma comment(linker, "/EXPORT:" __FUNCTION__"=" __FUNCDNAME__)

        if (m_bShowRadioWheel == on)
            return;

        if (on) {
            CTimer::ms_fTimeScale = 0.25f;
            auto cam = TheCamera.m_pCamGame;
            if (cam)
                cam->m_bDisableControls = true;

            audRadioStation::ms_CurrRadioStationRoll = audRadioStation::ms_CurrRadioStation;

            int32_t numStations = audRadioStation::GetNumStations() + 1;
            SetCursorToCurrentRadio(GetCurrentRadioStationFix(), numStations);
            m_bShowRadioWheel = true;
        }
        else {
            CTimer::ms_fTimeScale = 1.0f;
            auto cam = TheCamera.m_pCamGame;
            if (cam)
                cam->m_bDisableControls = false;

            m_bShowRadioWheel = false;
            m_bRadioSelected = false;

            m_nTimePassedAfterKeyPress = 0;
            m_nTimePassedAfterQuickSwitch = 0;
            m_nTimePassedForDisplayingTrackNames = 0;
        }
    }

    static inline bool IsDisplaying() {
#pragma comment(linker, "/EXPORT:" __FUNCTION__"=" __FUNCDNAME__)
        return m_bShowRadioWheel;
    }

    static inline void SetControlsOff(bool off) {
#pragma comment(linker, "/EXPORT:" __FUNCTION__"=" __FUNCDNAME__)
        m_bDisableControls = off;
    }

    static inline bool AreControlsDisabled() {
#pragma comment(linker, "/EXPORT:" __FUNCTION__"=" __FUNCDNAME__)
        return m_bDisableControls;
    }

    RadioWheelIV() {
        plugin::Events::initEngineEvent += []() {
            m_SpriteLoader.LoadAllSpritesFromTxd("platform:/textures/radiowheel");
        };
        
        plugin::Events::shutdownEngineEvent += []() {
            m_SpriteLoader.Clear();
        };

        plugin::Events::initGameEvent.before += []() {
            m_bShowRadioWheel = false;
        };

        plugin::Events::drawHudEvent.after += []() {
            auto dc = new T_CB_Generic_NoArgs(DrawWheel);
            dc->Init();

            dc = new T_CB_Generic_NoArgs(DrawQuickSwitch);
            dc->Init();
        };

        auto pattern = plugin::pattern::Get("E8 ? ? ? ? 83 C4 1C 83 BD");
        plugin::patch::Nop(pattern, 5); // No radio hud

        pattern = plugin::pattern::Get("83 EC 4C 56 8B F1 8A 86");
        plugin::patch::RedirectJump(pattern, UpdatePlayerVehicleRadio);
    }
} radioWheelIV;
