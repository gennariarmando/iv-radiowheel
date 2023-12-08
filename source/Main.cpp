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
    static inline std::unordered_map<int32_t, std::wstring> m_TracksMap = {};

    static inline void UpdateCursor() {
        int32_t x = rage::ioMouse::m_X;
        int32_t y = rage::ioMouse::m_Y;
        int32_t rsx = CPad::GetPad(0)->NewState.RightStickX;
        int32_t rsy = CPad::GetPad(0)->NewState.RightStickY;

        m_vPrevCursor = m_vCursor;
        if (CControl::m_UsingMouse) {
            float vel = 4.0f;
            m_vCursor.x += x * vel;
            m_vCursor.y += y * vel;
        }
        else {
            float vel = 4.0f;
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
        CFont::SetWrapx(0.375f, 0.775f);

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
                    int32_t hash = rage::atStringHash(buf);

                    stationName += L"~n~";
                    stationName += m_TracksMap[hash];
                }
            }

            CFont::PrintString(x, y, stationName.c_str());
        }

        CFont::DrawFonts();
    }

    static inline void DrawQuickSwitch() {
        if (CCutsceneMgr::IsRunning())
            return;

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
        //CSprite2d::DrawPolygon(m_vCursor.x, m_vCursor.y, m_vCursor.x + 2.0f, m_vCursor.y + 2.0f,
        //                centerPosition.x, centerPosition.y,
        //                centerPosition.x + 2.0f, centerPosition.y + 2.0f,
        //                0.0f,
        //                0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, CHudColours::Get(HUD_COLOUR_RED, 255));

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
            PopulateTracksMap();
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

    static void PopulateTracksMap() {
        m_TracksMap[0x76F24675] = L"MILES DAVIS~n~Move";
        m_TracksMap[0x91B57BFB] = L"ART BLAKEY AND THE JAZZ MESSENGERS~n~Moanin'";
        m_TracksMap[0x5A4E8D2E] = L"CHARLIE PARKER~n~Night & Day";
        m_TracksMap[0x6508A2A2] = L"DIZZY GILLESPIE~n~Whisper Not";
        m_TracksMap[0x3E34D4FB] = L"SONNY ROLLINS~n~St Thomas";
        m_TracksMap[0xD7F30879] = L"CHET BAKER~n~Let's Get Lost";
        m_TracksMap[0x1D779381] = L"JOHN COLTRANE~n~Giant Steps";
        m_TracksMap[0x2F45371C] = L"COUNT BASIE~n~April In Paris";
        m_TracksMap[0x5871A04E] = L"ROY HAYNES~n~Snap Crackle";
        m_TracksMap[0x6AA844BB] = L"DUKE ELLINGTON~n~Take the 'A' Train";
        m_TracksMap[0xB7D8DF03] = L"BILLY COBHAM~n~Stratus";
        m_TracksMap[0xA4A23896] = L"TOM SCOTT AND THE L.A. EXPRESS~n~Sneakin' In The Back";
        m_TracksMap[0xD2E99524] = L"DAVID AXELROD~n~Holy Thursday";
        m_TracksMap[0xC11FF191] = L"DAVID MCCALLUM~n~The Edge";
        m_TracksMap[0x6F734E3D] = L"ALEKSANDER MALISZEWSKI~n~Pokusa";
        m_TracksMap[0x5304155F] = L"RYO KAWASAKI~n~Raisins";
        m_TracksMap[0x8C000752] = L"GONG~n~Heavy Tune";
        m_TracksMap[0x79786247] = L"MARC MOULIN~n~Stomp";
        m_TracksMap[0x7CE95825] = L"GROVER WASHINGTON, JR~n~Knucklehead";
        m_TracksMap[0x8F1DFC8E] = L"ROY AYERS~n~Funk In The Hole";
        m_TracksMap[0x998C116A] = L"THE WHO~n~The Seeker";
        m_TracksMap[0xAB99B585] = L"PHILIP GLASS~n~Pruit Igoe";
        m_TracksMap[0x2805AE5F] = L"STYLES P~n~What's the problem";
        m_TracksMap[0x3148C0E5] = L"CREATIVE SOURCE~n~Who Is He And What Is He To You";
        m_TracksMap[0x43FA6648] = L"MANDRILL~n~Livin'It Up";
        m_TracksMap[0x6EAABBA8] = L"THE O'JAYS~n~Give The People What They Want";
        m_TracksMap[0x5D4598DA] = L"MANU DIBANGO~n~New Bell";
        m_TracksMap[0xFFD4DF1E] = L"RAMP~n~Daylight";
        m_TracksMap[0x0A86F482] = L"THE METERS~n~Just Kissed My Baby";
        m_TracksMap[0xA9D5B315] = L"FELA KUTI~n~Sorrow,Tears & Blood";
        m_TracksMap[0xD4638830] = L"FEMI KUTI~n~Truth Don Die";
        m_TracksMap[0xE6A22CAD] = L"LONNIE LISTON SMITH~n~A Chance For Peace";
        m_TracksMap[0x6288247B] = L"UNCLE MURDA~n~Anybody Can Get It";
        m_TracksMap[0x7D4959FD] = L"FELA KUTI~n~Zombie";
        m_TracksMap[0x8EDBFD22] = L"MAXIMUM PENALTY~n~All Your Boyz";
        m_TracksMap[0x9F969E97] = L"BAD BRAINS~n~Right Brigade";
        m_TracksMap[0x414F6326] = L"SHEER TERROR~n~Just Can't Hate Enough LIVE";
        m_TracksMap[0x6DD43C2F] = L"SICK OF IT ALL~n~Injustice System";
        m_TracksMap[0xFAB9D5F4] = L"UNDERDOG~n~Back To Back";
        m_TracksMap[0x14BE0A04] = L"AGNOSTIC FRONT~n~Victim In Pain";
        m_TracksMap[0x2708AE99] = L"MURPHY'S LAW~n~A Day In The Life";
        m_TracksMap[0x31A843D8] = L"KILLING TIME~n~Tell Tale";
        m_TracksMap[0xD3600749] = L"CERRONE~n~Supernature";
        m_TracksMap[0xC152632E] = L"GINO SOCCIO~n~Dancer";
        m_TracksMap[0x171B8FEB] = L"RAINBOW BROWN~n~Till You Surrender";
        m_TracksMap[0x0480EAAE] = L"HARRY THUMANN~n~Underwater";
        m_TracksMap[0x662F2E0D] = L"ELECTRIK FUNK~n~On A Journey";
        m_TracksMap[0x54708A90] = L"DON RAY~n~Standing In The Rain";
        m_TracksMap[0x47A87100] = L"PETER BROWN~n~Burning Love Breakdown";
        m_TracksMap[0x3E265DFC] = L"SKATT BROS.~n~Walk The Night";
        m_TracksMap[0xBB54D857] = L"SUZY Q~n~Get On Up And Do It Again";
        m_TracksMap[0x804FE24E] = L"JEAN MICHEL JARRE~n~Oxygene Pt.4";
        m_TracksMap[0x7709CFC2] = L"RAY LYNCH~n~The Oh Of Pleasure";
        m_TracksMap[0x3971559A] = L"STEVE ROACH~n~Arrival";
        m_TracksMap[0x85FE6EB3] = L"APHEX TWIN~n~Selected Ambient Works Vol. II CD2, TRK5";
        m_TracksMap[0x6F43413D] = L"TANGERINE DREAM~n~Remote Viewing";
        m_TracksMap[0x2DFE3EB4] = L"Global Communication~n~5:23";
        m_TracksMap[0x18281308] = L"BOB MARLEY & THE WAILERS AND DAMIAN MARLEY~n~Stand Up Jamrock";
        m_TracksMap[0x4A6BF78F] = L"BOB MARLEY & THE WAILERS~n~Wake Up And Live";
        m_TracksMap[0xC78271B6] = L"BOB MARLEY & THE WAILERS~n~Rebel Music (3 O'Clock Roadblock)";
        m_TracksMap[0xB9C1D635] = L"BOB MARLEY & THE WAILERS~n~So Much Trouble In The World";
        m_TracksMap[0x82FBE9C6] = L"STEPHEN MARLEY~n~Chase Dem";
        m_TracksMap[0x703DC44A] = L"BOB MARLEY & THE WAILERS~n~Rat Race";
        m_TracksMap[0x2BF13BB2] = L"BOB MARLEY & THE WAILERS~n~Pimper's Paradise";
        m_TracksMap[0x192F162E] = L"BOB MARLEY & THE WAILERS~n~Concrete Jungle";
        m_TracksMap[0x443D6C4A] = L"SOS BAND~n~Just Be Good To Me";
        m_TracksMap[0x35B3CF37] = L"RAHEEM DEVAUGHN~n~You";
        m_TracksMap[0xB0D7C579] = L"CASSIE~n~Me & U";
        m_TracksMap[0xDF19A1FC] = L"JODECI~n~Freek 'n You";
        m_TracksMap[0x0D49FE5C] = L"MINNIE RIPERTON~n~Inside My Love";
        m_TracksMap[0xFB8C5AE1] = L"DRU HILL~n~In My Bed (So So Def Mix)";
        m_TracksMap[0x0041E588] = L"THE ISLEY BROTHERS~n~Footsteps In The Dark";
        m_TracksMap[0xEEF942F7] = L"JILL SCOTT~n~Golden";
        m_TracksMap[0xE5372F73] = L"FREDDIE JACKSON~n~Have You Ever Loved Somebody";
        m_TracksMap[0xD34D8BA0] = L"ALEXANDER O'NEAL~n~Criticize";
        m_TracksMap[0xC7407386] = L"MARVIN GAYE~n~Inner City Blues (Make Me Wanna Holler)";
        m_TracksMap[0xB581D009] = L"GINUWINE~n~Pony";
        m_TracksMap[0xABCBBC9D] = L"BARRY WHITE~n~It's Only Love Doing Its Thing";
        m_TracksMap[0x6E46C194] = L"LOOSE ENDS~n~Hangin' On A String (Contemplating)";
        m_TracksMap[0x2C7C3E00] = L"SERYOGA~n~The Invasion";
        m_TracksMap[0x40F5E80F] = L"CJ~n~I Want You";
        m_TracksMap[0xCF2F047B] = L"R. KELLY~n~Bump N' Grind";
        m_TracksMap[0xD86C96E2] = L"LCD SOUNDSYSTEM~n~Get Innocuous!";
        m_TracksMap[0x49837916] = L"THE BOGGS~n~Arm In Arm (Shy Child Remix)";
        m_TracksMap[0x3BD2DDB5] = L"HUMMINGBIRD~n~You Can't Hide";
        m_TracksMap[0x0D0B8027] = L"LES SAVY FAV~n~Raging In The Plague Age";
        m_TracksMap[0x9131886D] = L"BUSTA RHYMES~n~Where's My Money";
        m_TracksMap[0x033FEC88] = L"UNKLE~n~Mayday (Featuring Duke Spirit)";
        m_TracksMap[0xF4AD4F63] = L"TOM VEK~n~One Horse Race";
        m_TracksMap[0xE6F633F5] = L"CHEESEBURGER~n~Cocaine";
        m_TracksMap[0xB5B3F666] = L"THE PISTOLAS~n~Take It With A Kiss";
        m_TracksMap[0xDD3EC55F] = L"WHITEY~n~Wrap It Up";
        m_TracksMap[0x9A1D3F39] = L"Yadnus (Still Going To The Roadhouse Mix)";
        m_TracksMap[0xD0A9AC39] = L"QUEEN~n~One Vision";
        m_TracksMap[0x179BBA1C] = L"HEART~n~Straight On";
        m_TracksMap[0x79D77E96] = L"JOE WALSH~n~Rocky Mountain Way";
        m_TracksMap[0x67A9DA3B] = L"ELTON JOHN~n~Street Kids";
        m_TracksMap[0x4EB1284A] = L"THE BLACK CROWES~n~Remedy";
        m_TracksMap[0xF767F891] = L"IGGY POP~n~I Wanna Be Your Dog";
        m_TracksMap[0xE083CB01] = L"GENESIS~n~Mama";
        m_TracksMap[0xD355B0A5] = L"BOB SEGER & THE SILVER BULLET BAND~n~Her Strut";
        m_TracksMap[0x7C09820A] = L"ZZ TOP~n~Thug";
        m_TracksMap[0xEED3E7A1] = L"WAR~n~Galaxy";
        m_TracksMap[0x280359FF] = L"GODLEY & CREME~n~Cry";
        m_TracksMap[0x1A47BE88] = L"HELLO~n~New York Groove";
        m_TracksMap[0x189B7890] = L"THE SISTERS OF MERCY~n~Dominion / Mother Russia";
        m_TracksMap[0xFF4945EC] = L"Q LAZZARUS~n~Goodbye Horses";
        m_TracksMap[0x0BE9DF19] = L"STEVE MARRIOTT~n~Cocaine";
        m_TracksMap[0xC23DCBC2] = L"THIN LIZZY~n~Jailbreak";
        m_TracksMap[0xA7959672] = L"R.E.M.~n~Turn You Inside Out";
        m_TracksMap[0x62E00D08] = L"GOLDFRAPP~n~Ooh La La";
        m_TracksMap[0xD13669B3] = L"RICK JAMES~n~Come Into My Life";
        m_TracksMap[0x7E7BC43F] = L"MYSTIKAL~n~Shake Ya Ass";
        m_TracksMap[0x64F3100E] = L"DON OMAR~n~Salio El Sol";
        m_TracksMap[0xD7A77575] = L"HECTOR EL FATHER~n~Maldades";
        m_TracksMap[0x8251CACB] = L"TITO EL BAMBINO~n~Siente El Boom (Remix)";
        m_TracksMap[0xD22876D3] = L"WISIN Y YANDEL~n~Sexy Movimiento";
        m_TracksMap[0x77694162] = L"CALLE 13~n~Atrevete-te-te";
        m_TracksMap[0x8D376CFE] = L"DADDY YANKEE~n~Impacto";
        m_TracksMap[0xA2EE186B] = L"VOLTIO~n~Ponmela";
        m_TracksMap[0x56B27FF1] = L"LLOYD~n~Get It Shawty";
        m_TracksMap[0x246B9B64] = L"NE - YO~n~Because Of You";
        m_TracksMap[0x5FF48819] = L"GET SHAKES~n~Disneyland Part 1";
        m_TracksMap[0xD139EAA2] = L"DELUKA~n~Sleep Is Impossible";
        m_TracksMap[0x14DD71E8] = L"BOBBY KONDERS~n~Intro";
        m_TracksMap[0x02EB4E04] = L"BURRO BANTON~n~Badder Den Dem";
        m_TracksMap[0xB83C38A3] = L"CHOPPA CHOP~n~Set It Off";
        m_TracksMap[0xA66F1509] = L"MAVADO~n~Real Mckoy";
        m_TracksMap[0x4A165C59] = L"JABBA~n~Raise It Up";
        m_TracksMap[0x3868B8FE] = L"BUNJI GARLIN~n~Brrrt";
        m_TracksMap[0xF143AAB5] = L"RICHIE SPICE~n~Youth Dem Cold";
        m_TracksMap[0x2A32A02E] = L"CHUCK FENDA~n~All About Da Weed";
        m_TracksMap[0x7C66C499] = L"CHEZIDEK~n~Call Pon Dem";
        m_TracksMap[0x6C6E24AC] = L"MAVADO~n~Last Night";
        m_TracksMap[0xBEB3C936] = L"SPRAGGA BENZ~n~Da Order";
        m_TracksMap[0xD364F298] = L"BOUNTY KILLER~n~Bullet Proof Skin";
        m_TracksMap[0xA530162F] = L"SHAGGY~n~Church Heathen";
        m_TracksMap[0xB599B702] = L"MUNGA~n~No Fraid A";
        m_TracksMap[0x074F5A60] = L"BUJU BANTON~n~Driver A";
        m_TracksMap[0x1825FC0D] = L"PADDED CELL~n~Signal Failure (GTA Version)";
        m_TracksMap[0xEBDDA37D] = L"BLACK DEVIL DISCO CLUB~n~The Devil In Us (Dub)";
        m_TracksMap[0x53B6F216] = L"ONE + ONE~n~No Pressure (Deadmau5 Remix)";
        m_TracksMap[0x295C9D62] = L"ALEX GOPHER~n~Brain Leech (Bugged Mind Remix)";
        m_TracksMap[0x371238CD] = L"K.I.M.~n~B.T.T.T.T.R.Y. (Bag Raiders Remix)";
        m_TracksMap[0x7CDFC46B] = L"SIMIAN MOBILE DISCO~n~Tits & Acid";
        m_TracksMap[0xAD0924B9] = L"NITZER EBB~n~Let Your Body Learn";
        m_TracksMap[0xFB8941CC] = L"KAVINSKY~n~Testarossa Overdrive(SebastiAn Remix)";
        m_TracksMap[0x10CC6C52] = L"CHRIS LAKE VS DEADMAU5~n~I Thought Inside Out (Original Mix)";
        m_TracksMap[0xDF0E88D7] = L"BOYS NOIZE~n~& Down";
        m_TracksMap[0xEF1928EC] = L"JUSTICE~n~Waters Of Nazareth";
        m_TracksMap[0x5662F77E] = L"KILLING JOKE~n~Turn To Red";
        m_TracksMap[0xB9FDB906] = L"PLAYGROUP~n~Make It Happen (Instrumental)";
        m_TracksMap[0x0C55DDB5] = L"LIQUID LIQUID~n~Optimo";
        m_TracksMap[0x7738B381] = L"BOB MARLEY & THE WAILERS~n~Satisfy My Soul";
        m_TracksMap[0xC783D416] = L"GREENSKEEPERS~n~Vagabond";
        m_TracksMap[0x82D64ABC] = L"JULIETTE AND THE LICKS~n~Inside The Cage (David Gilmour Girls Remix)";
        m_TracksMap[0x9517EF3F] = L"MAINO~n~Get Away Driver";
        m_TracksMap[0x1E95022B] = L"RED CAFÉ~n~Stick'm";
        m_TracksMap[0x30C9A694] = L"TRU LIFE~n~Wet 'em Up";
        m_TracksMap[0x4C78DDF2] = L"JOHNNY POLYGON~n~Price On Your Head";
        m_TracksMap[0x29E320AF] = L"GROUP HOME~n~Supa Star";
        m_TracksMap[0x4E73E9D0] = L"SPECIAL ED~n~I Got It Made";
        m_TracksMap[0x40224D2D] = L"JERU THE DAMAJA~n~D. Original";
        m_TracksMap[0x738A33E0] = L"MC LYTE~n~Cha Cha Cha";
        m_TracksMap[0x87CF5C6A] = L"AUDIO 2~n~Top Billin'";
        m_TracksMap[0x960DF8E7] = L"STETSASONIC~n~Go Stetsa 1";
        m_TracksMap[0x9D2C0723] = L"T LA ROCK & JAZZY JAY~n~It's Yours";
        m_TracksMap[0xAB71A3AE] = L"GANG STARR~n~Who's Gonna Take The Weight?";
        m_TracksMap[0x63A2E7FE] = L"MAIN SOURCE~n~Live At The Barbeque";
        m_TracksMap[0x51344321] = L"THE BLACK KEYS~n~Strange Times";
        m_TracksMap[0x46F62EA5] = L"TEENAGER~n~Pony";
        m_TracksMap[0xC6F1AEA2] = L"WHITE LIGHT PARADE~n~Riot In The City";
        m_TracksMap[0xB4C00A3F] = L"THE VIRGINS~n~One Week Of Danger";
        m_TracksMap[0x9867D18F] = L"SWIZZ BEATZ~n~Top Down";
        m_TracksMap[0x8E5DBD7B] = L"NAS~n~War Is Necessary";
        m_TracksMap[0xE1AA640F] = L"KANYE WEST FEAT. DWELE~n~Flashing Lights";
        m_TracksMap[0xD16BC392] = L"JOELL ORTIZ~n~Hip-Hop (Remix Feat. Jadakiss & Saigon)";
        m_TracksMap[0x41D0A552] = L"MOBB DEEP~n~Dirty New Yorker";
        m_TracksMap[0x72E787AF] = L"GHOSTFACE KILLAH FEAT. KID CAPRI~n~We Celebrate";
        m_TracksMap[0x6139E44C] = L"STYLES P~n~Blow Your Mind (Remix Feat. Jadakiss & Sheek Louch)";
        m_TracksMap[0x45EF2DB7] = L"QADIR~n~Nickname";
        m_TracksMap[0x6B19738F] = L"THE ROLLING STONES~n~Fingerprint";
        m_TracksMap[0x2AA27266] = L"THE RAPTURE~n~No Sex For Ben";
        m_TracksMap[0xFC9D965D] = L"CELEBRATION~n~Fly The Fly (Holy Fuck Remix)";
        m_TracksMap[0x6D0B3973] = L"ZiT! Has not spotted your track. You must be listening to some obscure shit.";
        m_TracksMap[0x338DC665] = L"ZiT! Has been unable to spot your track. Please try again with some decent music.";
        m_TracksMap[0x21D622F6] = L"ZiT! Regrets to inform you that your second rate track cannot be spotted.";
        m_TracksMap[0x17F80F3A] = L"ZiT! Does not have your track on record. Try spotting better music.";
        m_TracksMap[0x06396BBD] = L"ZiT! Is unable to spot this track for reasons of taste.";
        m_TracksMap[0x32F5DB8F] = L"PETER ILYICH TCHAIKOVSKY~n~Dance Of The Sugar Plum Fairy";
        m_TracksMap[0x2067B67B] = L"GREENSLEEVES";
        m_TracksMap[0x0E741294] = L"SCARBOROUGH";
        m_TracksMap[0x44CA7F40] = L"NIKOLAI RIMSKY- KORSAKOV~n~Flight Of The Bumblebee";
        m_TracksMap[0x32FBDBA3] = L"GTA IV Theme";
        m_TracksMap[0xC58400B1] = L"RICHARD WAGNER~n~Ride Of The Valkyries";
        m_TracksMap[0x7010A3BA] = L"ALICE COOPER~n~Go To Hell";
        m_TracksMap[0x5EE70167] = L"BLONDE ACID CULT~n~Calypso";
        m_TracksMap[0x90A9E4EC] = L"BLONDE ACID CULT~n~Shake It Loose";
        m_TracksMap[0x0B73DA82] = L"FREE BLOOD~n~Quick And Painful";
        m_TracksMap[0x3D35BE05] = L"THE CARPS~n~Porgie & Bess (Big Booty Girls)";
        m_TracksMap[0x281793C9] = L"THE CARPS~n~Veronica Belmont";
        m_TracksMap[0x59E0775A] = L"THE JANE SHERMANS~n~I Walk Alone";
        m_TracksMap[0xFD6A3DEB] = L"MAGIC DIRT~n~Get Ready To Die";
        m_TracksMap[0xCABA588C] = L"BRAZILIAN GIRLS~n~Nouveau Americain";
        m_TracksMap[0xD9D476C0] = L"TK WEBB & THE VISIONS~n~Shame";
        m_TracksMap[0xD0F9EB6F] = L"FREELAND~n~Borderline";
        m_TracksMap[0x4C57E229] = L"SCISSORS FOR LEFTY~n~Consumption Junction";
        m_TracksMap[0x3EA546C4] = L"THE SOFT PACK~n~Nightlife";
        m_TracksMap[0x2DCCA513] = L"JAPANTHER~n~Radical Businessman";
        m_TracksMap[0x282199BD] = L"FOXYLANE~n~Same Shirt";
        m_TracksMap[0xE3BA10F3] = L"FOXYLANE~n~Command";
        m_TracksMap[0x561A75A6] = L"MONOTONIX~n~Body Language";
        m_TracksMap[0x31422BF6] = L"STYX~n~Renegade";
        m_TracksMap[0x21948C9B] = L"LYNYRD SKYNYRD~n~Saturday Night Special";
        m_TracksMap[0x9E8E869D] = L"STEPPENWOLF~n~Ride With Me";
        m_TracksMap[0x2911A545] = L"THE JAMES GANG~n~Funk #49";
        m_TracksMap[0xF744C1B0] = L"AEROSMITH~n~Lord Of The Thighs";
        m_TracksMap[0x1582FE2C] = L"DEEP PURPLE~n~Highway Star";
        m_TracksMap[0xE3C11AA9] = L"FOGHAT~n~Drivin' Wheel";
        m_TracksMap[0x9C5B0BDE] = L"NAZARETH~n~Hair Of The Dog";
        m_TracksMap[0xAABDA8A3] = L"TUBEWAY ARMY~n~Bombers";
        m_TracksMap[0x96C280A9] = L"NAZARETH~n~Changin' Times";
        m_TracksMap[0xB509BD37] = L"BLACK OAK ARKANSAS~n~Hot and Nasty";
        m_TracksMap[0xEF892E9D] = L"BON JOVI~n~Wanted Dead Or Alive";
        m_TracksMap[0x02EF5569] = L"BREAKWATER~n~Release The Beast";
        m_TracksMap[0x12ECF564] = L"NEW YORK DOLLS~n~Private World";
        m_TracksMap[0x213891FF] = L"THE DOOBIE BROTHERS~n~Long Train Running";
        m_TracksMap[0x363ABC03] = L"THE EDGAR WINTER GROUP~n~Free Ride";
        m_TracksMap[0x448B58A4] = L"WARREN ZEVON~n~Lawyers, Guns and Money";
        m_TracksMap[0x3A3A43FE] = L"PYTHON LEE JACKSON Feat. ROD STEWART~n~In A Broken Dream";
        m_TracksMap[0x4F656E54] = L"RARE EARTH~n~(I Know) I'm Losing You";
        m_TracksMap[0x5DB40AF1] = L"ROBIN TROWER~n~Day Of The Eagle";
        m_TracksMap[0x710A319D] = L"BILLY SQUIER~n~The Stroke";
        m_TracksMap[0x830F56B3] = L"TED NUGENT~n~Hey Baby";
        m_TracksMap[0x9C648961] = L"THE CULT~n~Born To Be Wild";
        m_TracksMap[0x678B9FAC] = L"LOVE AND ROCKETS~n~Motorcycle";
        m_TracksMap[0x70D5B240] = L"MOTORHEAD~n~Ace Of Spades";
        m_TracksMap[0x1DC60C22] = L"IRON MAIDEN~n~The Trooper";
        m_TracksMap[0x281420BE] = L"MOTLEY CRUE~n~Wild Side";
        m_TracksMap[0x10DC724F] = L"MOTLEY CRUE~n~Kickstart My Heart";
        m_TracksMap[0x225C154A] = L"SAXON~n~Wheels Of Steel";
        m_TracksMap[0x519C7982] = L"JUDAS PRIEST~n~Breaking The Law";
        m_TracksMap[0x26E62416] = L"THE DOOBIE BROTHERS~n~China Grove";
        m_TracksMap[0xF3EEBE2C] = L"KUDU~n~Give Me Your Head";
        m_TracksMap[0xE3821D53] = L"TAME IMPALA~n~Half Full Glass Of Wine";
        m_TracksMap[0x10647717] = L"THE BRONX~n~Knifeman";
        m_TracksMap[0xFED2D3F4] = L"GAME REBELLION~n~Dance Girl (GTA MIX)";
        m_TracksMap[0x0860670B] = L"THE YELLING~n~Blood On The Steps";
        m_TracksMap[0xF6ACC3A4] = L"LOU REED~n~Vicious";
        m_TracksMap[0xC3D15DEE] = L"TERMANOLOGY~n~Here In Liberty City";
        m_TracksMap[0xB100B84D] = L"FREEWAY~n~Car Jack";
        m_TracksMap[0x2DE82A62] = L"SAIGON~n~Spit";
        m_TracksMap[0x201E8ECF] = L"SKYZOO~n~The Chase Is On";
        m_TracksMap[0x615B1147] = L"CONSEQUENCE~n~I Hear Footsteps";
        m_TracksMap[0x539075B2] = L"TALIB KWELI~n~My Favorite Song";
        m_TracksMap[0x84C3D818] = L"AT THE GATES~n~Slaughter Of The Soul";
        m_TracksMap[0x7275B37C] = L"DRIVE BY AUDIO~n~Jailbait";
        m_TracksMap[0x9C9E87D1] = L"CELTIC FROST~n~Inner Sanctum";
        m_TracksMap[0x95DB7A47] = L"ENTOMBED~n~Drowned";
        m_TracksMap[0x1B0E849F] = L"SEPULTURA~n~Dead Embryonic Cells";
        m_TracksMap[0x15B579F9] = L"DEICIDE~n~Dead By Dawn";
        m_TracksMap[0x385BBC29] = L"CANNIBAL CORPSE~n~I Cum Blood";
        m_TracksMap[0x94D8F522] = L"KREATOR~n~Awakening Of The Gods";
        m_TracksMap[0x85A456B9] = L"TERRORIZER~n~Fear Of Napalm";
        m_TracksMap[0xBB774262] = L"ROD STEWART~n~Every Picture Tells A Story";
        m_TracksMap[0xE7D71B25] = L"KILL MEMORY CRASH~n~Hell On Wheels";
        m_TracksMap[0x0C9564A1] = L"KREEPS~n~The Hunger (Blood In My Mouth)";
        m_TracksMap[0x0D9E675B] = L"BUSTA RHYMES FEAT. RON BROWZ~n~Arab Money";
        m_TracksMap[0xFBD3C3C6] = L"BUSTA RHYMES FEAT. YOUNG JEEZY & JADAKISS~n~Conglomerate";
        m_TracksMap[0xF1AEAF7C] = L"T.I. FEAT. SWIZZ BEATZ~n~Swing Ya Rag";
        m_TracksMap[0xDF690AF1] = L"RON BROWZ~n~Jumping Out The Window";
        m_TracksMap[0x435FD2D1] = L"DJ KHALED FEAT. KANYE WEST & T-PAIN~n~Go Hard";
        m_TracksMap[0x26CE19AE] = L"KARDINAL OFFISHALL FEAT. AKON & SEAN PAUL~n~Dangerous (Remix)";
        m_TracksMap[0x540C742A] = L"JOHN LEGEND FEAT ANDRE 3000~n~Green Light";
        m_TracksMap[0x5F888B2E] = L"KANYE WEST~n~Love Lockdown";
        m_TracksMap[0x8D46E6AA] = L"B.O.B~n~Auto-Tune";
        m_TracksMap[0xF803C23A] = L"SERYOGA~n~Dobav' Skorost";
        m_TracksMap[0xE6299E86] = L"RIFFMASTER~n~Begu (Rancho Song)";
        m_TracksMap[0xAB22A879] = L"SERYOGA~n~Chiki";
        m_TracksMap[0x79E745FF] = L"DELICE~n~Goryacheye Leto";
        m_TracksMap[0xD0767320] = L"ALEKSEY BOLSHOY~n~YA Nenavizhu Karaoke";
        m_TracksMap[0x9D3E0CB0] = L"SERYOGA~n~Mon Ami (ft. Maks Lorens)";
        m_TracksMap[0x26629EFB] = L"ZHENYA FOKIN~n~Noch'ju";
        m_TracksMap[0x34273A84] = L"KIEVELEKTRO~n~Gulyaj, Slavyane!! (ft. Alyona Vinnitskaya)";
        m_TracksMap[0x6B05283F] = L"AYVENGO~n~Reprezenty";
        m_TracksMap[0x39BFC5B5] = L"RIFFMASTER~n~Riffmaster Tony";
        m_TracksMap[0xAAE57957] = L"AYVENGO~n~Underground";
        m_TracksMap[0x9CAEDCEA] = L"VIBE PLACEHOLDER MIX";
        m_TracksMap[0xC7EA3358] = L"ELECTRO CHOC PLACEHOLDER MIX";
        m_TracksMap[0xDD825E8C] = L"SJS PLACEHOLDER MIX";
        m_TracksMap[0x0CCA3D1B] = L"THE STUDIO PLACEHOLDER MIX";
        m_TracksMap[0xF73011E7] = L"CLUB NOUVEAU~n~Lean On Me";
        m_TracksMap[0x2CC6FBE4] = L"CULTURE CLUB~n~Time (Clock Of The Heart)";
        m_TracksMap[0xAB61F728] = L"HAMILTON BOHANNON~n~Let's Start The Dance";
        m_TracksMap[0xB83F90FB] = L"PEACHES & HERB~n~Funtime";
        m_TracksMap[0xCF85BF87] = L"CANDI STATON~n~Young Hearts Run Free";
        m_TracksMap[0xDCB259E0] = L"THE O'JAYS~n~I Love Music";
        m_TracksMap[0xF1F38462] = L"VICKI SUE ROBINSON~n~Turn The Beat Around";
        m_TracksMap[0xED317ACA] = L"A TASTE OF HONEY~n~Boogie Oogie Oogie";
        m_TracksMap[0xAA3B58DB] = L"GLORIA GAYNOR~n~Never Can Say Goodbye";
        m_TracksMap[0xDF5EC321] = L"SISTER SLEDGE~n~He's The Greatest Dancer";
        m_TracksMap[0x918AA77A] = L"CHANGE~n~A Lover's Holiday";
        m_TracksMap[0x2597CFAE] = L"THE FATBACK BAND~n~(Are You Ready) Do The Bus Stop";
        m_TracksMap[0xCF4C2318] = L"SYLVESTER~n~I Need You";
        m_TracksMap[0x080C1497] = L"STEPHANIE MILLS~n~Put Your Body In It";
        m_TracksMap[0x32BC69F7] = L"PATRICK COWLEY~n~Menergy";
        m_TracksMap[0x6ABD59F8] = L"THE TRAMMPS~n~Disco Inferno";
        m_TracksMap[0x1C583D2F] = L"DAN HARTMAN~n~Relight My Fire";
        m_TracksMap[0x5103A685] = L"CHIC~n~Everybody Dance";
        m_TracksMap[0xEABDD8E3] = L"CHIC~n~My Forbidden Lover";
        m_TracksMap[0x3261E82E] = L"PEACHES & HERB~n~Shake Your Groove Thing";
        m_TracksMap[0x5899349C] = L"ALICIA BRIDGES~n~I Love The Nightlife (Disco Round)";
        m_TracksMap[0x0F3321D1] = L"GQ~n~Disco Nights (Rock-Freak)";
        m_TracksMap[0x612F4484] = L"MICHAEL ZAGER BAND~n~Let's All Chant";
        m_TracksMap[0xB79B715B] = L"ELVIS CRESPO~n~Suavemente";
        m_TracksMap[0x9498AA0E] = L"FULANITO~n~Guallando";
        m_TracksMap[0x87030EE3] = L"DON OMAR~n~Virtual Diva";
        m_TracksMap[0xA35DC798] = L"WISIN Y YANDEL~n~Me Estás Tentando";
        m_TracksMap[0x1ECC45CF] = L"MACHINE~n~There But For The Grace Of God Go I";
        m_TracksMap[0x101C286B] = L"CREME D' COCOA~n~Doin' The Dog";
        m_TracksMap[0x2254CCDC] = L"RUFUS & CHAKA KHAN~n~Any Love";
        m_TracksMap[0xDF06C7FC] = L"DAVID MORALES WITH LEA-LORIEN~n~How Would U Feel";
        m_TracksMap[0xD09FAB2E] = L"STEVE MAC VS. MOSQUITO FEAT. STEVE SMITH~n~Lovin' You More (That Big Track) (Freemasons Vocal Club Mix)";
        m_TracksMap[0x8550139C] = L"STONEBRIDGE FEAT. THERESE~n~Put 'Em High (JJ's Club Mix)";
        m_TracksMap[0x950FB317] = L"MARLY~n~You Never Know (Morjac Extended Mix)";
        m_TracksMap[0x86B9166A] = L"SHAPE UK & CHIC~n~Lola's Theme";
        m_TracksMap[0xF902FAFC] = L"FREEMASONS FEAT. AMANDA WILSON~n~Love On My Mind";
        m_TracksMap[0xDC35C162] = L"SOULSEARCHER~n~Can't Get Enough";
        m_TracksMap[0xCC492189] = L"MICHAEL GRAY~n~The Weekend";
        m_TracksMap[0x31446B82] = L"J MAJIK & WICKAMAN~n~Crazy World (Fonzerelli Remix)";
        m_TracksMap[0x21FACCEF] = L"BOOTY LUV~n~Boogie 2Nite (Seamus Haji Big Love Mix)";
        m_TracksMap[0x0ABD25D0] = L"HOOK N SLING~n~The Best Thing";
        m_TracksMap[0x1890C177] = L"ERIC PRYDZ~n~Pjanoo (Club Mix)";
        m_TracksMap[0x3C4D88F0] = L"MAJOR LAZER FEAT. LEFTSIDE & SUPAHYPE~n~Jump Up";
        m_TracksMap[0xBD3F0AD9] = L"DANIEL HAAKSMAN FEAT. MC MILTINHO~n~Kid Conga";
        m_TracksMap[0xD3653725] = L"BOY 8-BIT~n~A City Under Siege";
        m_TracksMap[0xE0C2D1E0] = L"CROOKERS FEAT. KARDINALL OFFISHALL & CARLA-MARIE~n~Put Your Hands On Me (A Capella)";
        m_TracksMap[0xF6EFFE3A] = L"THE CHEMICAL BROTHERS~n~Nude Night";
        m_TracksMap[0x070B1DF0] = L"CROOKERS FEAT. SOLO~n~Bad Men";
        m_TracksMap[0x19334240] = L"MIIKE SNOW~n~Animal (A Capella)";
        m_TracksMap[0xBA1682E0] = L"JAHCOOZI~n~Watching You (Oliver $ Remix)";
        m_TracksMap[0xCD54A95C] = L"CROOKERS~n~Boxer";
        m_TracksMap[0x70656F7F] = L"SONICC~n~Stickin'";
        m_TracksMap[0x7E910BDA] = L"BLACK NOISE~n~Knock You Out (Andy George Remix)";
        m_TracksMap[0xA2AF5416] = L"MIXHELL FEAT. JEN LASHER & OH SNAP~n~Boom Da (Crookers Remix)";
        m_TracksMap[0xB261F37B] = L"CROOKERS FEAT. KELIS~n~No Security";
        m_TracksMap[0x00A79304] = L"Zit!";
        m_TracksMap[0x531A063B] = L"抱歉，ZiT无法识别当前播放的曲目。";
        m_TracksMap[0x95F88BF3] = L"MILES DAVIS~n~Move";
        m_TracksMap[0xA78D2F1C] = L"ART BLAKEY AND THE JAZZ MESSENGERS~n~Moanin'";
        m_TracksMap[0x715AC2B8] = L"CHARLIE PARKER~n~Night & Day";
        m_TracksMap[0x83286653] = L"DIZZY GILLESPIE~n~Whisper Not";
        m_TracksMap[0xDCC21985] = L"SONNY ROLLINS~n~St Thomas";
        m_TracksMap[0xEE94BD2A] = L"CHET BAKER~n~Let's Get Lost";
        m_TracksMap[0xB85650AE] = L"JOHN COLTRANE~n~Giant Steps";
        m_TracksMap[0xC9C5F38D] = L"COUNT BASIE~n~April In Paris";
        m_TracksMap[0xB7BE1831] = L"ROY HAYNES~n~Snap Crackle";
        m_TracksMap[0x1E17E4E7] = L"DUKE ELLINGTON~n~Take the 'A' Train";
        m_TracksMap[0x8291ADD5] = L"BILLY COBHAM~n~Stratus";
        m_TracksMap[0x6FE30878] = L"TOM SCOTT AND THE L.A. EXPRESS~n~Sneakin' In The Back";
        m_TracksMap[0xD6D1D654] = L"DAVID AXELROD~n~Holy Thursday";
        m_TracksMap[0xC50832C1] = L"DAVID MCCALLUM~n~The Edge";
        m_TracksMap[0x494A3B4B] = L"ALEKSANDER MALISZEWSKI~n~Pokusa";
        m_TracksMap[0x2EBF0635] = L"RYO KAWASAKI~n~Raisins";
        m_TracksMap[0x9DBA6426] = L"GONG~n~Heavy Tune";
        m_TracksMap[0x8C04C0BB] = L"MARC MOULIN~n~Stomp";
        m_TracksMap[0xC13BB70D] = L"GROVER WASHINGTON, JR~n~Knucklehead";
        m_TracksMap[0x1B046A9D] = L"ROY AYERS~n~Funk In The Hole";
        m_TracksMap[0x33111ABA] = L"THE WHO~n~The Seeker";
        m_TracksMap[0x44D0BE39] = L"PHILIP GLASS~n~Pruit Igoe";
        m_TracksMap[0x4893C5BF] = L"STYLES P~n~What's the problem";
        m_TracksMap[0x6235F903] = L"CREATIVE SOURCE~n~Who Is He And What Is He To You";
        m_TracksMap[0x3A702980] = L"MANDRILL~n~Livin'It Up";
        m_TracksMap[0x8BB24C03] = L"THE O'JAYS~n~Give The People What They Want";
        m_TracksMap[0x61A377E6] = L"MANU DIBANGO~n~New Bell";
        m_TracksMap[0x1AB8E6BE] = L"RAMP~n~Daylight";
        m_TracksMap[0x2875823B] = L"THE METERS~n~Just Kissed My Baby";
        m_TracksMap[0x94845A57] = L"FELA KUTI~n~Sorrow,Tears & Blood";
        m_TracksMap[0x6F9E908C] = L"FEMI KUTI~n~Truth Don Die";
        m_TracksMap[0x8558BC00] = L"LONNIE LISTON SMITH~n~A Chance For Peace";
        m_TracksMap[0x49704420] = L"UNCLE MURDA~n~Anybody Can Get It";
        m_TracksMap[0x5C3669AC] = L"FELA KUTI~n~Zombie";
        m_TracksMap[0x361C9D79] = L"MAXIMUM PENALTY~n~All Your Boyz";
        m_TracksMap[0x38DBA2F7] = L"BAD BRAINS~n~Right Brigade";
        m_TracksMap[0xBA1C21E6] = L"SHEER TERROR~n~Just Can't Hate Enough LIVE";
        m_TracksMap[0x752197EE] = L"SICK OF IT ALL~n~Injustice System";
        m_TracksMap[0x98855EB5] = L"UNDERDOG~n~Back To Back";
        m_TracksMap[0xA2C2732F] = L"AGNOSTIC FRONT~n~Victim In Pain";
        m_TracksMap[0x4639BA1F] = L"MURPHY'S LAW~n~A Day In The Life";
        m_TracksMap[0x1857DE58] = L"KILLING TIME~n~Tell Tale";
        m_TracksMap[0x4F8BCCC3] = L"CERRONE~n~Supernature";
        m_TracksMap[0x5C62E671] = L"GINO SOCCIO~n~Dancer";
        m_TracksMap[0xD697E0BD] = L"RAINBOW BROWN~n~Till You Surrender";
        m_TracksMap[0xC8494420] = L"HARRY THUMANN~n~Underwater";
        m_TracksMap[0x2EE89161] = L"ELECTRIK FUNK~n~On A Journey";
        m_TracksMap[0x60427414] = L"DON RAY~n~Standing In The Rain";
        m_TracksMap[0x1360DA4E] = L"PETER BROWN~n~Burning Love Breakdown";
        m_TracksMap[0x452DBDEB] = L"SKATT BROS.~n~Walk The Night";
        m_TracksMap[0x67E40357] = L"SUZY Q~n~Get On Up And Do It Again";
        m_TracksMap[0x4C584C40] = L"JEAN MICHEL JARRE~n~Oxygene Pt.4";
        m_TracksMap[0x7E232FD5] = L"RAY LYNCH~n~The Oh Of Pleasure";
        m_TracksMap[0x44AB4256] = L"STEVE ROACH~n~Arrival";
        m_TracksMap[0xF06719BF] = L"APHEX TWIN~n~Selected Ambient Works Vol. II CD2, TRK5";
        m_TracksMap[0x62667DCC] = L"TANGERINE DREAM~n~Remote Viewing";
        m_TracksMap[0xFDAB3447] = L"Global Communication~n~5:23";
        m_TracksMap[0xABEC10CA] = L"BOB MARLEY & THE WAILERS AND DAMIAN MARLEY~n~Stand Up Jamrock";
        m_TracksMap[0x1DA87441] = L"BOB MARLEY & THE WAILERS~n~Wake Up And Live";
        m_TracksMap[0xD78467FA] = L"BOB MARLEY & THE WAILERS~n~Rebel Music (3 O'Clock Roadblock)";
        m_TracksMap[0xC9294B44] = L"BOB MARLEY & THE WAILERS~n~So Much Trouble In The World";
        m_TracksMap[0xB2D69B57] = L"STEPHEN MARLEY~n~Chase Dem";
        m_TracksMap[0xC88DC6C5] = L"BOB MARLEY & THE WAILERS~n~Rat Race";
        m_TracksMap[0x0E37D218] = L"BOB MARLEY & THE WAILERS~n~Pimper's Paradise";
        m_TracksMap[0xC1E9397C] = L"BOB MARLEY & THE WAILERS~n~Concrete Jungle";
        m_TracksMap[0xEF9C94E2] = L"SOS BAND~n~Just Be Good To Me";
        m_TracksMap[0x1D677077] = L"RAHEEM DEVAUGHN~n~You";
        m_TracksMap[0x476D4486] = L"CASSIE~n~Me & U";
        m_TracksMap[0xF5352013] = L"JODECI~n~Freek 'n You";
        m_TracksMap[0x4507BFBB] = L"MINNIE RIPERTON~n~Inside My Love";
        m_TracksMap[0x32951AD6] = L"DRU HILL~n~In My Bed (So So Def Mix)";
        m_TracksMap[0x5ADF67EE] = L"THE ISLEY BROTHERS~n~Footsteps In The Dark";
        m_TracksMap[0x28AE838D] = L"JILL SCOTT~n~Golden";
        m_TracksMap[0x8289B742] = L"FREDDIE JACKSON~n~Have You Ever Loved Somebody";
        m_TracksMap[0x50C2D3B5] = L"ALEXANDER O'NEAL~n~Criticize";
        m_TracksMap[0xA6157E59] = L"MARVIN GAYE~n~Inner City Blues (Make Me Wanna Holler)";
        m_TracksMap[0x74619AF2] = L"GINUWINE~n~Pony";
        m_TracksMap[0xA7130048] = L"BARRY WHITE~n~It's Only Love Doing Its Thing";
        m_TracksMap[0xCC88CB33] = L"LOOSE ENDS~n~Hangin' On A String (Contemplating)";
        m_TracksMap[0xD943E4A9] = L"SERYOGA~n~The Invasion";
        m_TracksMap[0x72491C7D] = L"CJ~n~I Want You";
        m_TracksMap[0x85DA439F] = L"R. KELLY~n~Bump N' Grind";
        m_TracksMap[0xD51361F0] = L"LCD SOUNDSYSTEM~n~Get Innocuous!";
        m_TracksMap[0xC2C43D52] = L"THE BOGGS~n~Arm In Arm (Shy Child Remix)";
        m_TracksMap[0xB1839AD1] = L"HUMMINGBIRD~n~You Can't Hide";
        m_TracksMap[0x1FD67775] = L"LES SAVY FAV~n~Raging In The Plague Age";
        m_TracksMap[0x0FE8D79A] = L"BUSTA RHYMES~n~Where's My Money";
        m_TracksMap[0xFA21AC0C] = L"UNKLE~n~Mayday (Featuring Duke Spirit)";
        m_TracksMap[0xE84F0867] = L"TOM VEK~n~One Horse Race";
        m_TracksMap[0x56ABE523] = L"CHEESEBURGER~n~Cocaine";
        m_TracksMap[0xAAF1B324] = L"THE PISTOLAS~n~Take It With A Kiss";
        m_TracksMap[0x80875E50] = L"WHITEY~n~Wrap It Up";
        m_TracksMap[0x8EC6FACF] = L"Yadnus (Still Going To The Roadhouse Mix)";
        m_TracksMap[0x6F923C66] = L"QUEEN~n~One Vision";
        m_TracksMap[0x3E4059C3] = L"HEART~n~Straight On";
        m_TracksMap[0x126B821A] = L"JOE WALSH~n~Rocky Mountain Way";
        m_TracksMap[0x2896AE70] = L"ELTON JOHN~n~Street Kids";
        m_TracksMap[0xF758CBF5] = L"THE BLACK CROWES~n~Remedy";
        m_TracksMap[0x5008857F] = L"IGGY POP~n~I Wanna Be Your Dog";
        m_TracksMap[0xB42DCDC8] = L"GENESIS~n~Mama";
        m_TracksMap[0xC5046F75] = L"BOB SEGER & THE SILVER BULLET BAND~n~Her Strut";
        m_TracksMap[0x92C28AF2] = L"ZZ TOP~n~Thug";
        m_TracksMap[0xF8FE5750] = L"WAR~n~Galaxy";
        m_TracksMap[0x0563F01B] = L"GODLEY & CRÈME~n~Cry";
        m_TracksMap[0x90118390] = L"HELLO~n~New York Groove";
        m_TracksMap[0xACC62055] = L"THE SISTERS OF MERCY~n~Dominion / Mother Russia";
        m_TracksMap[0xAA791BBB] = L"Q LAZZARUS~n~Goodbye Horses";
        m_TracksMap[0xBD8AC1DE] = L"STEVE MARRIOTT'S SCRUBBERS~n~Cocaine";
        m_TracksMap[0x72AF2C28] = L"THIN LIZZY~n~Jailbreak";
        m_TracksMap[0x610588D5] = L"R.E.M.~n~Turn You Inside Out";
        m_TracksMap[0x2A7E9BC8] = L"GOLDFRAPP~n~Ooh La La";
        m_TracksMap[0x98D1F86D] = L"RICK JAMES~n~Come Into My Life";
        m_TracksMap[0x3E1342F1] = L"MYSTIKAL~n~Shake Ya Ass";
        m_TracksMap[0xB618B442] = L"DON OMAR~n~Salio El Sol";
        m_TracksMap[0x006848E4] = L"HECTOR EL FATHER~n~Maldades";
        m_TracksMap[0x1251ECB7] = L"TITO EL BAMBINO~n~Siente El Boom (Remix)";
        m_TracksMap[0x8422563A] = L"WISIN Y YANDEL~n~Sexy Movimiento";
        m_TracksMap[0xB664BABE] = L"CALLE 13~n~Atrevete-te-te";
        m_TracksMap[0xCA64E2BE] = L"DADDY YANKEE~n~Impacto";
        m_TracksMap[0xBC874703] = L"VOLTIO~n~Ponmela";
        m_TracksMap[0xDD3A086C] = L"LLOYD~n~Get It Shawty";
        m_TracksMap[0x11387068] = L"NE - YO~n~Because Of You";
        m_TracksMap[0xBEBFCC54] = L"GET SHAKES~n~Disneyland Part 1";
        m_TracksMap[0xE28F13F6] = L"DELUKA~n~Sleep Is Impossible";
        m_TracksMap[0xF462B79D] = L"BOBBY KONDERS~n~Intro";
        m_TracksMap[0x04485768] = L"BURRO BANTON~n~Badder Den Dem";
        m_TracksMap[0x15F77AC6] = L"CHOPPA CHOP~n~Set It Off";
        m_TracksMap[0x27B61E43] = L"MAVADO~n~Real Mckoy";
        m_TracksMap[0x2B3E2553] = L"JABBA~n~Raise It Up";
        m_TracksMap[0x9889FFE1] = L"BUNJI GARLIN~n~Brrrt";
        m_TracksMap[0x86A85C1E] = L"RICHIE SPICE~n~Youth Dem Cold";
        m_TracksMap[0xDAB29C65] = L"CHUCK FENDA~n~All About Da Weed";
        m_TracksMap[0xDD7DA1FB] = L"CHEZIDEK~n~Call Pon Dem";
        m_TracksMap[0xCA1B7B2F] = L"MAVADO~n~Last Night";
        m_TracksMap[0x58C8188A] = L"SPRAGGA BENZ~n~Da Order";
        m_TracksMap[0xA59DB234] = L"BOUNTY KILLER~n~Bullet Proof Skin";
        m_TracksMap[0xB85457A1] = L"SHAGGY~n~Church Heathen";
        m_TracksMap[0x7D2B6150] = L"MUNGA~n~No Fraid A";
        m_TracksMap[0x0FF506E5] = L"BUJU BANTON~n~Driver";
        m_TracksMap[0x68A7B849] = L"PADDED CELL~n~Signal Failure";
        m_TracksMap[0x7B695DCC] = L"BLACK DEVIL DISCO CLUB~n~The Devil In Us (Dub)";
        m_TracksMap[0xAFD44541] = L"ONE + ONE~n~No Pressure (Deadmau5 Remix)";
        m_TracksMap[0x5E1C21D2] = L"ALEX GOPHER~n~Brain Leech (Bugged Mind Remix)";
        m_TracksMap[0xA7003399] = L"K.I.M.~n~B.T.T.T.T.R.Y. (Bag Raiders Remix)";
        m_TracksMap[0x9C3F1E17] = L"SIMIAN MOBILE DISCO~n~Tits & Acid";
        m_TracksMap[0x736D4C74] = L"NITZER EBB~n~Let Your Body Learn";
        m_TracksMap[0x22DA2B4F] = L"KAVINSKY~n~Testarossa (Sebastian Remix)";
        m_TracksMap[0x501C85D3] = L"CHRIS LAKE VS DEADMAU5~n~I Thought Inside Out (Original Mix)";
        m_TracksMap[0x7E0561A4] = L"BOYS NOIZE~n~& Down";
        m_TracksMap[0x3C8FDEBA] = L"JUSTICE~n~Waters Of Nazareth";
        m_TracksMap[0xCFD1053E] = L"KILLING JOKE~n~Turn To Red";
        m_TracksMap[0xD96B7C4B] = L"PLAYGROUP~n~Make It Happen (Instrumental)";
        m_TracksMap[0x67AC18CE] = L"LIQUID LIQUID~n~Optimo";
        m_TracksMap[0xA1710C57] = L"BOB MARLEY & THE WAILERS~n~Satisfy My Soul";
        m_TracksMap[0xAFDAA92A] = L"GREENSKEEPERS~n~Vagabond";
        m_TracksMap[0xFDD44520] = L"JULIETTE AND THE LICKS~n~Inside The Cage (David Gilmour Girls Remix)";
        m_TracksMap[0x0C2AE1CD] = L"MAINO~n~Get Away Driver";
        m_TracksMap[0xEA709E59] = L"RED CAFÉ~n~Stick'm";
        m_TracksMap[0xF8BE3AF4] = L"TRU LIFE~n~Wet 'em Up";
        m_TracksMap[0x470FD796] = L"JOHNNY POLYGON~n~Price On Your Head";
        m_TracksMap[0xA2B60FC1] = L"GROUP HOME~n~Supa Star";
        m_TracksMap[0xFE3946CA] = L"SPECIAL ED~n~I Got It Made";
        m_TracksMap[0xADCE25F1] = L"JERU THE DAMAJA~n~D. Original";
        m_TracksMap[0xE93E1CD4] = L"MC LYTE~n~Cha Cha Cha";
        m_TracksMap[0x4EE66823] = L"AUDIO 2~n~Top Billin'";
        m_TracksMap[0x46B257BB] = L"STETASONIC~n~Go Stetsa";
        m_TracksMap[0xC2884F71] = L"T LA ROCK & JAZZY JAY~n~It's Yours";
        m_TracksMap[0xCB3760CF] = L"GANG STARR~n~Who's Gonna Take The Weight?";
        m_TracksMap[0xC6AE2C41] = L"MAIN SOURCE~n~Live At The Barbeque";
        m_TracksMap[0xB4F488CE] = L"THE BLACK KEYS~n~Strange Times";
        m_TracksMap[0xA322652A] = L"TEENAGER~n~Pony";
        m_TracksMap[0x93C9C679] = L"WHITE LIGHT PARADE~n~Riot In The City";
        m_TracksMap[0x86CBAC7D] = L"THE VIRGINS~n~One Week Of Danger";
        m_TracksMap[0x74EE08C2] = L"SWIZZ BEATZ~n~Top Down";
        m_TracksMap[0x6347E576] = L"NAS~n~War Is Necessary";
        m_TracksMap[0x516341AD] = L"KAYNE WEST FEAT. DWELE~n~Flashing Lights";
        m_TracksMap[0x4D6239AB] = L"JOELL ORTIZ~n~Hip-Hop (Remix Feat. Jadakiss & Saigon)";
        m_TracksMap[0xFF3F9C13] = L"MOBB DEEP~n~Dirty New Yorker";
        m_TracksMap[0x09FDB18F] = L"GHOSTFACE KILLAH FEAT. KID CAPRI~n~We Celebrate";
        m_TracksMap[0xDBA3D4D4] = L"STYLES P~n~Blow Your Mind (Remix Feat. Jadakiss & Sheek Louch)";
        m_TracksMap[0xE8E6EF5A] = L"PAPOOSE~n~Stylin'";
        m_TracksMap[0x794C101E] = L"QADIR~n~Nickname";
        m_TracksMap[0xA9606D66] = L"THE ROLLING STONES~n~Fingerprint";
        m_TracksMap[0x76AE8803] = L"THE RAPTURE~n~No Sex For Ben";
        m_TracksMap[0xDCE15467] = L"CELEBRATION~n~Fly The Fly (Holy Fuck Remix)";
        m_TracksMap[0x27360817] = L"Zit! Has not spotted your track. You must be listening to some obscure shit.";
        m_TracksMap[0x3977AC9A] = L"Zit! Has been unable to spot your track. Please try again with some decent music.";
        m_TracksMap[0x8DC0D52B] = L"Zit! Regrets to inform you that your second rate track cannot be spotted.";
        m_TracksMap[0x9E8676B6] = L"Zit! Does not have your track on record. Try spotting better music.";
        m_TracksMap[0xA147FBDD] = L"Zit! Is unable to spot this track for reasons of taste.";
        m_TracksMap[0x1C5434D6] = L"BILLIE HOLIDAY~n~Fine and Mellow";
        m_TracksMap[0x0ACB91C5] = L"TESTERS~n~Body Music";
        m_TracksMap[0xB66C987E] = L"PETER ILYICH TCHAIKOVSKY~n~Dance Of The Sugar Plum Fairy";
        m_TracksMap[0xE98B7EBB] = L"GREENSLEEVES";
        m_TracksMap[0xD3AA52F9] = L"SCARBOROUGH";
        m_TracksMap[0x4EAE4903] = L"NIKOLAI RIMSKY- KORSAKOV~n~Flight Of The Bumblebee";
        m_TracksMap[0x00F62D94] = L"GTA IV Theme";
        m_TracksMap[0xEA400028] = L"RICHARD WAGNER~n~Ride Of The Valkyries";
        m_TracksMap[0xF2CC9D46] = L"ALICE COOPER~n~Go To Hell";
        m_TracksMap[0x012639F9] = L"BLONDE ACID CULT~n~Calypso";
        m_TracksMap[0x1167DA7C] = L"BLONDE ACID CULT~n~Shake It Loose";
        m_TracksMap[0xA78506B8] = L"FREE BLOOD~n~Quick And Painful";
        m_TracksMap[0x37FAA795] = L"THE CARPS~n~Porgie & Bess (Big Booty Girls)";
        m_TracksMap[0xA1AB7AF5] = L"THE CARPS~n~Veronica Belmont";
        m_TracksMap[0x54E7E16F] = L"THE JANE SHERMANS~n~I Walk Alone";
        m_TracksMap[0x470E45BC] = L"MAGIC DIRT~n~Get Ready To Die";
        m_TracksMap[0xFF0F35BB] = L"BRAZILIAN GIRLS~n~Nouveau Americain";
        m_TracksMap[0xE84D0837] = L"TK WEBB & THE VISIONS~n~Shame";
        m_TracksMap[0xB0771764] = L"FREELAND~n~Borderline";
        m_TracksMap[0x1EBE73F1] = L"SCISSORS FOR LEFTY~n~Consumption Junction";
        m_TracksMap[0xCAE3CC3D] = L"THE SOFT PACK~n~Nightlife";
        m_TracksMap[0xB92128B8] = L"JAPANTHER~n~Radical Businessman";
        m_TracksMap[0x671F84B2] = L"FOXYLANE~n~Same Shirt";
        m_TracksMap[0xD568E147] = L"FOXYLANE~n~Command";
        m_TracksMap[0x24D80030] = L"MONOTONIX~n~Body Language";
        m_TracksMap[0x9B92EDA4] = L"STYX~n~Renegade";
        m_TracksMap[0x6148F911] = L"LYNYRD SKYNYRD~n~Saturday Night Special";
        m_TracksMap[0x3801A683] = L"STEPPENWOLF~n~Ride With Me";
        m_TracksMap[0x149F7D73] = L"THE JAMES GANG~n~Funk #49";
        m_TracksMap[0x03C1DBB8] = L"AEROSMITH~n~Lord Of The Thighs";
        m_TracksMap[0xB103363C] = L"DEEP PURPLE~n~Highway Star";
        m_TracksMap[0x2C0E2C54] = L"FOGHAT~n~Drivin' Wheel";
        m_TracksMap[0x36DB41EE] = L"NAZARETH~n~Hair Of The Dog";
        m_TracksMap[0x74F9BE2A] = L"TUBEWAY ARMY~n~Bombers";
        m_TracksMap[0x63381AA7] = L"NAZARETH~n~Changin' Times";
        m_TracksMap[0x8E4F70D5] = L"BLACK OAK ARKANSAS~n~Hot and Nasty";
        m_TracksMap[0x9A9C0A96] = L"BON JOVI~n~Wanted Dead Or Alive";
        m_TracksMap[0x1C478DEF] = L"BREAKWATER~n~Release The Beast";
        m_TracksMap[0x4856E60D] = L"NEW YORK DOLLS~n~Private World";
        m_TracksMap[0x37C844F0] = L"THE DOOBIE BROTHERS~n~Long Train Running";
        m_TracksMap[0xF1E3B924] = L"THE EDGAR WINTER GROUP~n~Free Ride";
        m_TracksMap[0x6D7EB05C] = L"WARREN ZEVON~n~Lawyers, Guns and Money";
        m_TracksMap[0x5FD014FF] = L"PYTHON LEE JACKSON Feat. ROD STEWART~n~In A Broken Dream";
        m_TracksMap[0x7AFF4B5D] = L"RARE EARTH~n~(I Know) I'm Losing You";
        m_TracksMap[0x2D4D2FFA] = L"ROBIN TROWER~n~Day Of The Eagle";
        m_TracksMap[0x6875A642] = L"BILLY SQUIER~n~The Stroke";
        m_TracksMap[0x076E6525] = L"TED NUGENT~n~Hey Baby";
        m_TracksMap[0xF608C25A] = L"THE CULT~n~Born To Be Wild";
        m_TracksMap[0xECDAAFFE] = L"LOVE AND ROCKETS~n~Motorcycle";
        m_TracksMap[0xDA6A0B1D] = L"MOTORHEAD~n~Ace Of Spades";
        m_TracksMap[0xF69D439F] = L"IRON MAIDEN~n~The Trooper";
        m_TracksMap[0x06E7E434] = L"MOTLEY CRUE~n~Wild Side";
        m_TracksMap[0x21151892] = L"MOTLEY CRUE~n~Kickstart My Heart";
        m_TracksMap[0x338ABD7D] = L"SAXON~n~Wheels Of Steel";
        m_TracksMap[0x572885D4] = L"JUDAS PRIEST~n~Breaking The Law";
        m_TracksMap[0xD500018D] = L"THE DOOBIE BROTHERS~n~China Grove";
        m_TracksMap[0xEAAB2CE3] = L"KUDU~n~Give Me Your Head";
        m_TracksMap[0xFFF45775] = L"TAME IMPALA~n~Half Full Glass Of Wine";
        m_TracksMap[0x0E24F3D6] = L"THE BRONX~n~Knifeman";
        m_TracksMap[0x53837E8E] = L"GAME REBELLION~n~Dance Girl (GTA MIX)";
        m_TracksMap[0xE1421A09] = L"THE YELLING~n~Blood On The Steps";
        m_TracksMap[0xF6F3456B] = L"LOU REED~n~Vicious";
        m_TracksMap[0x04B5E0F0] = L"TERMANOLOGY~n~Here In Liberty City";
        m_TracksMap[0x8A78EC78] = L"FREEWAY~n~Car Jack";
        m_TracksMap[0x1C840878] = L"SAIGON~n~Spit";
        m_TracksMap[0x02DBD524] = L"SKYZOO~n~The Chase Is On";
        m_TracksMap[0xF2023371] = L"CONSEQUENCE~n~I Hear Footsteps";
        m_TracksMap[0xE6B91CDF] = L"TALIB KWELI~n~My Favorite Song";
        m_TracksMap[0x5C2B07C5] = L"AT THE GATES~n~Slaughter Of The Soul";
        m_TracksMap[0x4A036376] = L"DRIVE BY AUDIO~n~Jailbait";
        m_TracksMap[0x3F9D4EAA] = L"CELTIC FROST~n~Inner Sanctum";
        m_TracksMap[0x2D6EAA4D] = L"ENTOMBED~n~Drowned";
        m_TracksMap[0x8300556B] = L"SEPULTURA~n~Dead Embryonic Cells";
        m_TracksMap[0x70AAB0C0] = L"DEICIDE~n~Dead By Dawn";
        m_TracksMap[0xA34B972D] = L"CANNIBAL CORPSE~n~I Cum Blood";
        m_TracksMap[0x5EDC8E50] = L"KREATOR~n~Awakening Of The Gods";
        m_TracksMap[0x69AAA3EC] = L"TERRORIZER~n~Fear Of Napalm";
        m_TracksMap[0x8488D9A8] = L"ROD STEWART~n~Every Picture Tells A Story";
        m_TracksMap[0x3DA34BDE] = L"KILL MEMORY CRASH~n~Hell On Wheels";
        m_TracksMap[0x4F55EF43] = L"KREEPS~n~The Hunger (Blood In My Mouth)";
        m_TracksMap[0xF69DBF04] = L"BUSTA RHYMES FEAT. RON BROWZ~n~Arab Money";
        m_TracksMap[0xE5711CAB] = L"BUSTA RHYMES FEAT. YOUNG JEEZY & JADAKISS~n~Conglomerate";
        m_TracksMap[0x100B71DF] = L"T.I. FEAT. SWIZZ BEATZ~n~Swing Ya Rag";
        m_TracksMap[0x01D5D574] = L"RON BROWZ~n~Jumping Out The Window";
        m_TracksMap[0x1BCD0956] = L"DJ KHALED FEAT. KANYE WEST & T-PAIN~n~Go Hard";
        m_TracksMap[0x3728C00D] = L"KARDINAL OFFISHALL FEAT. AKON & SEAN PAUL~n~Dangerous (Remix)";
        m_TracksMap[0x4B15E7E7] = L"JOHN LEGEND FEAT ANDRE 3000~n~Green Light";
        m_TracksMap[0xE70C1FD1] = L"KANYE WEST~n~Love Lockdown";
        m_TracksMap[0xF57ABCAE] = L"B.O.B~n~Auto-Tune";
        m_TracksMap[0xE02B1337] = L"SERYOGA~n~Dobav' Skorost";
        m_TracksMap[0xFDD24E85] = L"RIFFMASTER~n~Begu(Rancho Song)";
        m_TracksMap[0x7A0446EF] = L"SERYOGA~n~Chiki";
        m_TracksMap[0xA794220A] = L"DELICE~n~Goryacheye Leto";
        m_TracksMap[0x974F8181] = L"ALEKSEY BOLSHOY~n~YA Nenavizhu Karaoke";
        m_TracksMap[0xD54AFD77] = L"SERYOGA~n~Mon Ami (ft. Maks Lorens)";
        m_TracksMap[0x31FCB6D5] = L"ZHENYA FOKIN~n~Noch'ju";
        m_TracksMap[0x20361348] = L"KIEVELEKTRO~n~Gulyaj, Slavyane!! (ft. Alyona Vinnitskaya)";
        m_TracksMap[0x3D044CE4] = L"AYVENGO~n~Reprezenty";
        m_TracksMap[0x6ABB2851] = L"RIFFMASTER~n~Riffmaster Tony";
        m_TracksMap[0xB65204A2] = L"AYVENGO~n~Underground";
        m_TracksMap[0xC810281E] = L"VIBE PLACEHOLDER MIX";
        m_TracksMap[0xDACDCD99] = L"ELECTRO CHOC PLACEHOLDER MIX";
        m_TracksMap[0xEC84F107] = L"SJS PLACEHOLDER MIX";
        m_TracksMap[0xC3E79FC5] = L"THE STUDIO PLACEHOLDER MIX";
        m_TracksMap[0xD620C437] = L"CLUB NOUVEAU~n~Lean On Me";
        m_TracksMap[0xC0B21612] = L"CULTURE CLUB~n~Time (Clock Of The Heart)";
        m_TracksMap[0xB51E012A] = L"HAMILTON BOHANNON~n~Let's Start The Dance";
        m_TracksMap[0xC753A595] = L"PEACHES & HERB~n~Funtime";
        m_TracksMap[0xDA81CBF1] = L"CANDI STATON~n~Young Hearts Run Free";
        m_TracksMap[0xEABE6C6A] = L"THE O'JAYS~n~I Love Music";
        m_TracksMap[0x8F72B5B8] = L"VICKI SUE ROBINSON~n~Turn The Beat Around";
        m_TracksMap[0xA13DD94E] = L"A TASTE OF HONEY~n~Boogie Oogie Oogie";
        m_TracksMap[0x5996CE6D] = L"GLORIA GAYNOR~n~Never Can Say Goodbye";
        m_TracksMap[0xFC2B138F] = L"SISTER SLEDGE~n~He's The Greatest Dancer";
        m_TracksMap[0x2E17F768] = L"CHANGE~n~A Lover's Holiday";
        m_TracksMap[0x1CCE54D5] = L"THE FATBACK BAND~n~(Are You Ready) Do The Bus Stop";
        m_TracksMap[0x0CC6B4AE] = L"SYLVESTER~n~I Need You";
        m_TracksMap[0x00009B22] = L"STEPHANIE MILLS~n~Put Your Body In It";
        m_TracksMap[0x326A7FF5] = L"PATRICK COWLEY~n~Menergy";
        m_TracksMap[0x13AD427B] = L"THE TRAMMPS~n~Disco Inferno";
        m_TracksMap[0xC79BAA59] = L"DAN HARTMAN~n~Relight My Fire";
        m_TracksMap[0xB9D90ED4] = L"CHIC~n~Everybody Dance";
        m_TracksMap[0x1C48D4D2] = L"CHIC~n~My Forbidden Lover";
        m_TracksMap[0x2A087051] = L"PEACHES & HERB~n~Shake Your Groove Thing";
        m_TracksMap[0x00CA1DD5] = L"ALICIA BRIDGES~n~I Love The Nightlife (Disco Round)";
        m_TracksMap[0x0E94B96A] = L"GQ~n~Disco Nights (Rock-Freak)";
        m_TracksMap[0x8C2AB71C] = L"MICHAEL ZAGER BAND~n~Let's All Chant";
        m_TracksMap[0x7A7513B1] = L"ELVIS CRESPO~n~Suavemente";
        m_TracksMap[0xBDA516D4] = L"FULANITO~n~Guallando";
        m_TracksMap[0xE6846892] = L"DON OMAR~n~Virtual Diva";
        m_TracksMap[0x5A284FE0] = L"WISIN Y YANDEL~n~Me Estás Tentando";
        m_TracksMap[0x615863AC] = L"MACHINE~n~There But For The Grace Of God Go I";
        m_TracksMap[0x5302C701] = L"CREME D' COCOA~n~Doin' The Dog";
        m_TracksMap[0x449FAA33] = L"RUFUS & CHAKA KHAN~n~Any Love";
        m_TracksMap[0x1E35BCAF] = L"DAVID MORALES WITH LEA-LORIEN~n~How Would U Feel";
        m_TracksMap[0x33C4E7CD] = L"STEVE MAC VS. MOSQUITO FEAT. STEVE SMITH~n~Lovin' You More (That Big Track) (Freemasons Vocal Club Mix)";
        m_TracksMap[0x51B52515] = L"STONEBRIDGE FEAT. THERESE~n~Put 'Em High (JJ's Club Mix)";
        m_TracksMap[0xD354284D] = L"MARLY~n~You Never Know (Morjac Extended Mix)";
        m_TracksMap[0xE594CCCE] = L"SHAPE UK & CHIC~n~Lola's Theme";
        m_TracksMap[0xB86D7280] = L"FREEMASONS FEAT. AMANDA WILSON~n~Love On My Mind";
        m_TracksMap[0xCAB91717] = L"SOULSEARCHER~n~Can't Get Enough";
        m_TracksMap[0x2DF9DD97] = L"MICHAEL GRAY~n~The Weekend";
        m_TracksMap[0xF18CE4BE] = L"J MAJIK & WICKAMAN~n~Crazy World (Fonzerelli Remix)";
        m_TracksMap[0x1C0D39BE] = L"BOOTY LUV~n~Boogie 2Nite (Seamus Haji Big Love Mix)";
        m_TracksMap[0x80096FB4] = L"HOOK N SLING~n~The Best Thing";
        m_TracksMap[0x8D480A31] = L"ERIC PRYDZ~n~Pjanoo (Club Mix)";
        m_TracksMap[0x69AA42F6] = L"MAJOR LAZER FEAT. LEFTSIDE & SUPAHYPE~n~Jump Up";
        m_TracksMap[0x30BC511F] = L"DANIEL HAAKSMAN FEAT. MC MILTINHO~n~Kid Conga";
        m_TracksMap[0x47067DB3] = L"BOY 8-BIT~n~A City Under Siege";
        m_TracksMap[0x0C8D88C2] = L"CROOKERS FEAT. KARDINALL OFFISHALL & CARLA-MARIE~n~Put Your Hands On Me (A Capella)";
        m_TracksMap[0x23E2B76C] = L"THE CHEMICAL BROTHERS~n~Nude Night";
        m_TracksMap[0x0A0703B1] = L"CROOKERS FEAT. SOLO~n~Bad Men";
        m_TracksMap[0x1F5D2E5D] = L"MIIKE SNOW~n~Animal (A Capella)";
        m_TracksMap[0x63103462] = L"JAHCOOZI~n~Watching You (Oliver $ Remix)";
        m_TracksMap[0x6D1C487A] = L"CROOKERS~n~Boxer";
        m_TracksMap[0x47D77DF5] = L"SONICC~n~Stickin'";
        m_TracksMap[0x51709123] = L"BLACK NOISE~n~Knock You Out (Andy George Remix)";
        m_TracksMap[0x0C4086C8] = L"MIXHELL FEAT. JEN LASHER & OH SNAP~n~Boom Da (Crookers Remix)";
        m_TracksMap[0x36B2DBAC] = L"CROOKERS FEAT. KELIS~n~No Security";
        m_TracksMap[0xE861E8B9] = L"BOY MEETS GIRL~n~Waiting For A Star To Fall";
        m_TracksMap[0x7AF08DDC] = L"SWING OUT SISTER~n~Breakout";
        m_TracksMap[0x8D50B29C] = L"PREFAB SPROUT~n~When Love Breaks Down";
        m_TracksMap[0x9F1F5639] = L"NU SHOOZ~n~I Can't Wait";
        m_TracksMap[0x0799A3E0] = L"MARILLION~n~Kayleigh";
        m_TracksMap[0xFBDE8C6A] = L"TEXAS~n~I Don't Want A Lover";
        m_TracksMap[0x2A39691F] = L"LISA STANSFIELD FEAT. COLDCUT~n~People Hold On";
        m_TracksMap[0xEEEDF289] = L"RE-FLEX~n~The Politics Of Dancing";
        m_TracksMap[0xE53BDF25] = L"ROACHFORD~n~Cuddly Toy";
        m_TracksMap[0x91E7B862] = L"NARADA MICHAEL WALDEN~n~Divine Emotions";
        m_TracksMap[0x7C1D0CCD] = L"T'PAU~n~Heart And Soul";
        m_TracksMap[0xA9E5E85E] = L"NENEH CHERRY~n~Buffalo Stance";
        m_TracksMap[0x4A12AE75] = L"BELOUIS SOME~n~IMAGINATION";
        m_TracksMap[0x93A14191] = L"BILLY OCEAN~n~Caribbean Queen (No More Love On The Run)";
        m_TracksMap[0xA695E77A] = L"CLIMIE FISHER~n~Love Changes (Everything)";
        m_TracksMap[0x904CBAE8] = L"CURIOSITY KILLED THE CAT~n~Misfit";
        m_TracksMap[0x9EC3D7CE] = L"FIVE STAR~n~Find The Time";
        m_TracksMap[0xA814EA70] = L"HUE & CRY~n~Labour Of Love";
        m_TracksMap[0x84802347] = L"TERENCE TRENT D'ARBY~n~Wishing Well";
        m_TracksMap[0xE513E46D] = L"'TIL TUESDAY~n~Voices Carry";
        m_TracksMap[0xFE139314] = L"WET WET WET~n~Wishing I Was Lucky";
        m_TracksMap[0x1057379B] = L"ROXETTE~n~The Look";
        m_TracksMap[0x33B7FE5C] = L"HALL & OATES~n~Maneater";
        m_TracksMap[0xDE9ED97F] = L"ALPHA WAVE MOVEMENT~n~Artifacts & Prophecies";
        m_TracksMap[0xB55486EB] = L"ERIK WOLLO~n~Monument";
        m_TracksMap[0xC3122266] = L"PETE NAMLOOK~n~1st Impression";
        m_TracksMap[0x99AECFA0] = L"AUTECHRE~n~Bike";
        m_TracksMap[0xA9B56FAD] = L"PETE NAMLOOK & KLAUS SCHULZE FEAT. BILL LASWELL~n~V/8 - Psychedelic Brunch";
        m_TracksMap[0x77060A4F] = L"CHILLED BY NATURE~n~Go Forward (Love Bubble Mix)";
        m_TracksMap[0xB0307F2F] = L"SPACETIME CONTINUUM~n~Voice of the Earth";
        m_TracksMap[0x9DC1DA52] = L"LARRY HEARD~n~Cosmology Myth";
        m_TracksMap[0x8C94B7F8] = L"ALUCIDNATION~n~Skygazer (3002 Remix)";
        m_TracksMap[0x7A231315] = L"COLDCUT~n~AutumnLeaves (Irresistable Force Mix Trip 2)";
        m_TracksMap[0xAFFE7EC3] = L"TOM MIDDLETON~n~Moonbathing";
        m_TracksMap[0xA0C7DD1E] = L"SYSTEM 7~n~Masato Eternity (Original Mix)";
        m_TracksMap[0x4A8C30A8] = L"MOBY~n~My Beautiful Sky";
        m_TracksMap[0x44EDAAD7] = L"THE ORB~n~A Huge Ever Growing Pulsating Brain That Rules From The Centre Of The Ultraworld (Live Mix MK 10)";
        m_TracksMap[0x369F0E3A] = L"JEFFREY OSBORNE~n~Stay With Me Tonight";
        m_TracksMap[0xE000E0FF] = L"MORRISSEY~n~Everyday Is Like Sunday";
        m_TracksMap[0xD4D34754] = L"LEVEL 42~n~Something About You";
        m_TracksMap[0x02F52397] = L"RODIGAN INTRO";
        m_TracksMap[0xCF3FBC2D] = L"BARRINGTON LEVY~n~Don't Fuss Nor Fight (aka Sweet Reggae Music)";
        m_TracksMap[0xBD9218D2] = L"INI KAMOZE~n~Outta Jamaica";
        m_TracksMap[0x1BDAD562] = L"DAMIAN 'JR GONG' MARLEY~n~Holiday";
        m_TracksMap[0x5131BFF7] = L"THE MORWELLS & PRINCE JAMMY~n~Jammin' For Survival";
        m_TracksMap[0xF6E58B78] = L"JOHN HOLT~n~Police In Helicopter";
        m_TracksMap[0x2C97F6DC] = L"SUGAR MINOTT~n~Hard Time Pressure";
        m_TracksMap[0x820E21AF] = L"DESMOND DEKKER & THE ACES~n~007 (Shanty Town)";
        m_TracksMap[0x76390A05] = L"MAJOR LAZER FEAT. TURBULENCE~n~Anything Goes";
        m_TracksMap[0xBE4EFCDF] = L"PRINCE JAMMY~n~Jammys A Shine";
        m_TracksMap[0xCBFC983A] = L"TOOTS & THE MAYTALS~n~54-46 Was My Number";
        m_TracksMap[0x9AC135C4] = L"FRANKIE PAUL~n~Worries In The Dance";
        m_TracksMap[0xA887D151] = L"MR. VEGAS~n~Mus Come a Road";
        m_TracksMap[0x292AF5DC] = L"BOY MEETS GIRL~n~Waiting For A Star To Fall";
        m_TracksMap[0x1405CB92] = L"SWING OUT SISTER~n~Breakout";
        m_TracksMap[0x45C7AF15] = L"PREFAB SPROUT~n~When Love Breaks Down";
        m_TracksMap[0x2FA982D9] = L"NU SHOOZ~n~I Can't Wait";
        m_TracksMap[0x0BEB3A81] = L"MARILLION~n~Kayleigh";
        m_TracksMap[0xF7C391DE] = L"TEXAS~n~I Don't Want A Lover";
        m_TracksMap[0x098E3573] = L"LISA STANSFIELD FEAT. COLDCUT~n~People Hold On";
        m_TracksMap[0x6683EF5D] = L"RE-FLEX~n~The Politics Of Dancing";
        m_TracksMap[0x302982A9] = L"ROACHFORD~n~Cuddly Toy";
        m_TracksMap[0x89EDB630] = L"NARADA MICHAEL WALDEN~n~Divine Emotions";
        m_TracksMap[0x53B049B6] = L"T'PAU~n~Heart And Soul";
        m_TracksMap[0xD13AC51D] = L"NENEH CHERRY~n~Buffalo Stance";
        m_TracksMap[0x2C24F98C] = L"BELOUIS SOME~n~IMAGINATION";
        m_TracksMap[0x306C021A] = L"BILLY OCEAN~n~Caribbean Queen (No More Love On The Run)";
        m_TracksMap[0x46D12EE4] = L"CLIMIE FISHER~n~Love Changes (Everything)";
        m_TracksMap[0x4960340E] = L"CURIOSITY KILLED THE CAT~n~Misfit";
        m_TracksMap[0x5783D055] = L"FIVE STAR~n~Find The Time";
        m_TracksMap[0x6E1DFD89] = L"HUE & CRY~n~Labour Of Love";
        m_TracksMap[0x82C9A6E0] = L"TERENCE TRENT D'ARBY~n~Wishing Well";
        m_TracksMap[0xA7596FFF] = L"'TIL TUESDAY~n~Voices Carry";
        m_TracksMap[0x7C901999] = L"WET WET WET~n~Wishing I Was Lucky";
        m_TracksMap[0x76238CAC] = L"ROXETTE~n~The Look";
        m_TracksMap[0x95A8CBB6] = L"HALL & OATES~n~Maneater";
        m_TracksMap[0x05790E5D] = L"ALPHA WAVE MOVEMENT~n~Artifacts & Prophecies";
        m_TracksMap[0x6D895E7C] = L"ERIK WOLLO~n~Monument";
        m_TracksMap[0x9BC03AE9] = L"PETE NAMLOOK~n~1st Impression";
        m_TracksMap[0x4A3097CB] = L"AUTECHRE~n~Bike";
        m_TracksMap[0x7C7AFC5F] = L"PETE NAMLOOK & KLAUS SCHULZE FEAT. BILL LASWELL~n~V/8 - Psychedelic Brunch";
        m_TracksMap[0x1208272F] = L"CHILLED BY NATURE~n~Go Forward (Love Bubble Mix)";
        m_TracksMap[0x3780F124] = L"SPACETIME CONTINUUM~n~Voice of the Earth";
        m_TracksMap[0x4E971F50] = L"LARRY HEARD~n~Cosmology Myth";
        m_TracksMap[0x5CE6BBEF] = L"ALUCIDNATION~n~Skygazer (3002 Remix)";
        m_TracksMap[0x4E9E1F62] = L"COLDCUT~n~AutumnLeaves (Irresistable Force Mix Trip 2)";
        m_TracksMap[0x5CC9BBB9] = L"TOM MIDDLETON~n~Moonbathing";
        m_TracksMap[0xE9955406] = L"SYSTEM 7~n~Masato Eternity (Original Mix)";
        m_TracksMap[0x149BAA16] = L"MOBY~n~My Beautiful Sky";
        m_TracksMap[0xC2340CA0] = L"THE ORB~n~A Huge Ever Growing Pulsating Brain That Rules From The Centre Of The Ultraworld (Live Mix MK 10)";
        m_TracksMap[0x0C72A11C] = L"JEFFREY OSBORNE~n~Stay With Me Tonight";
        m_TracksMap[0xA3CA4FC9] = L"MORRISSEY~n~Everyday Is Like Sunday";
        m_TracksMap[0x9206AADE] = L"LEVEL 42~n~Something About You";
        m_TracksMap[0xAFA9667F] = L"RODIGAN INTRO";
        m_TracksMap[0xA12B4983] = L"BARRINGTON LEVY~n~Don't Fuss Nor Fight (aka Sweet Reggae Music)";
        m_TracksMap[0x5BBE3EAA] = L"INI KAMOZE~n~Outta Jamaica";
        m_TracksMap[0x3DFB0324] = L"DAMIAN 'JR GONG' MARLEY~n~Holiday";
        m_TracksMap[0x7601F331] = L"THE MORWELLS & PRINCE JAMMY~n~Jammin' For Survival";
        m_TracksMap[0x684757BC] = L"JOHN HOLT~n~Police In Helicopter";
        m_TracksMap[0x22A0CC70] = L"SUGAR MINOTT~n~Hard Time Pressure";
        m_TracksMap[0x04E190F2] = L"DESMOND DEKKER & THE ACES~n~007 (Shanty Town)";
        m_TracksMap[0x07DF96AA] = L"MAJOR LAZER FEAT. TURBULENCE~n~Anything Goes";
        m_TracksMap[0x19243C32] = L"PRINCE JAMMY~n~Jammys A Shine";
        m_TracksMap[0x6B6EE0C6] = L"TOOTS & THE MAYTALS~n~54-46 Was My Number";
        m_TracksMap[0x5CB9435B] = L"FRANKIE PAUL~n~Worries In The Dance";
        m_TracksMap[0x4EE9A7BC] = L"MR. VEGAS~n~Mus Come a Road";
    }
    
} radioWheelIV;
