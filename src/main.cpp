#include <lua.hpp>

#include <Moss/Moss_stdinc.h>
#include <Moss/Moss_Platform.h>
#include <Moss/Moss_Audio.h>
#include <Moss/Moss_Renderer.h>
#include <Moss/Variants/Color.h>
#include <Moss/Variants/Quat.h>
#include <Moss/Variants/Rect.h>
#include <Moss/Variants/Vector/Vec2.h>
#include <Moss/Variants/Vector/Vec3.h>
#include <Moss/Variants/Vector/Vec4.h>

#include <cstdint>
#include <cstring>
#include <new>
#include <string>
#include <vector>

namespace {

// -----------------------------------------------------------------------------
// Generic Moss pointer userdata
// -----------------------------------------------------------------------------

template<typename T>
struct Handle
{
    T* value{};
};

template<typename T>
T* check_handle(
    lua_State* L,
    int index,
    const char* metatable)
{
    auto* handle =
        static_cast<Handle<T>*>(
            luaL_checkudata(
                L,
                index,
                metatable));

    if (!handle || !handle->value)
    {
        luaL_error(
            L,
            "invalid or destroyed Moss handle");
    }

    return handle->value;
}

template<typename T>
int push_handle(
    lua_State* L,
    T* value,
    const char* metatable)
{
    if (!value)
    {
        lua_pushnil(L);
        return 1;
    }

    auto* handle =
        static_cast<Handle<T>*>(
            lua_newuserdatauv(
                L,
                sizeof(Handle<T>),
                0));

    handle->value = value;

    luaL_setmetatable(
        L,
        metatable);

    return 1;
}

template<typename T>
int destroy_handle(
    lua_State* L,
    const char* metatable,
    void (*destroy_fn)(T*))
{
    auto* handle =
        static_cast<Handle<T>*>(
            luaL_checkudata(
                L,
                1,
                metatable));

    if (handle && handle->value)
    {
        if (destroy_fn)
            destroy_fn(handle->value);

        handle->value = nullptr;
    }

    return 0;
}


static lua_State* G = nullptr;


// -----------------------------------------------------------------------------
// Metatable names
// -----------------------------------------------------------------------------
// Platform
struct Window { Moss_Window* value{}; };
struct Monitor { Moss_Monitor* value{}; };
struct Gamepad { Moss_Gamepad* value{}; };
struct Haptic { Moss_Haptic* value{}; };
struct Camera { Moss_Camera* value{}; };
struct Storage { Moss_Storage* value{}; };

struct Curser { Moss_Curser* value{}; };
struct GamepadBinding { Moss_GamepadBinding* value{}; };
struct Capture { Moss_Capture* value{}; }; // Camera Device Dont use as the rendering camera
struct Storage { Moss_Storage* value{}; };
struct Surface { Moss_Surface* value{}; };
struct HapticDirection { Moss_HapticDirection* value{}; };
struct HapticConstant { Moss_HapticConstant* value{}; };
struct HapticPeriodic { Moss_HapticPeriodic* value{}; };
struct HapticCondition { Moss_HapticCondition* value{}; };
struct HapticRamp { Moss_HapticRamp* value{}; };
struct HapticLeftRight { Moss_HapticLeftRight* value{}; };
struct HapticCustom { Moss_HapticCustom* value{}; }; 
union HapticEffect { Moss_HapticEffect* value{}; };
struct Haptic { Moss_Haptic* value{}; };
struct Gamepad { Moss_Gamepad* value{}; };
struct GamepadAxis { Moss_GamepadAxis* value{}; };
struct CameraSpec { Moss_CameraSpec* value{}; };
struct GammaRamp { Moss_GammaRamp* value{}; };
struct VideoMode { Moss_VideoMode* value{}; };
struct Image { Moss_Image* value{}; };
struct Locale { Moss_Locale* value{}; };
struct PenAxisEvent { Moss_PenAxisEvent* value{}; };
struct PenButtonEven { Moss_PenButtonEvent* value{}; };
struct PenMotionEvent{ Moss_PenMotionEvent* value{}; };
struct PenProximityEvent { Moss_PenProximityEvent* value{}; };
struct PenTouchEvent { Moss_PenTouchEvent* value{}; };
struct Finger { Moss_Finger* value{}; };
struct HapticDirection { Moss_HapticDirection* value{}; };
struct HapticLeftRight { Moss_HapticLeftRight* value{}; };
struct HapticCustom { Moss_HapticCustom* value{}; };
struct HapticRamp { Moss_HapticRamp* value{}; };
struct HapticConstant { Moss_HapticConstant* value{}; };
struct HapticCondition { Moss_HapticCondition* value{}; };
struct HapticPeriodic { Moss_HapticPeriodic* value{}; };
union HapticEffect { Moss_HapticEffect* value{}; };
struct Haptic { Moss_Haptic* value{}; };
struct PathInfo { Moss_PathInfo* value{}; };
struct DialogFileFilter { Moss_DialogFileFilter* value{}; };

// Audio
struct { AudioEffect* value{}; };
struct { AudioStream* value{}; };
struct { AudioStream2D* value{}; };
struct { AudioStream3D* value{}; };
struct { AudioListener2D* value{}; };
struct { AudioListener3D* value{}; };
struct { RayAudioListener2D* value{}; };
struct { RayAudioListener3D* value{}; };
struct { Moss_AudioSource* value{}; };
struct { Moss_Microphone* value{}; };
struct { Wav* value{}; };
struct { Moss_AudioRayHit2D* value{}; };
struct { Moss_AudioRayHit3D* value{}; };
struct { Moss_AudioDecodedData* value{}; };
struct { Moss_MicrophoneDesc* value{}; };
struct { Moss_MicrophoneLevels* value{}; };
struct { Moss_AudioRayTraceResult* value{}; };
struct { Moss_AudioRayTrace2DDesc* value{}; };
struct { Moss_AudioRayTrace3DDesc* value{}; };

// GPU

// Renderer
struct Renderer { Moss_Renderer* value; };
struct { SkyBox* value; };
struct { Viewport* value; };
struct { SubViewport* value; };
struct { FogVolume* value; };
struct { SurfaceInstance* value; };
struct { Texture* value; };
struct { Moss_Mesh* value; };
struct { Moss_Font* value; };
struct { Moss_Model* value; };

struct Frustum2D { Frustum2D* value; };
struct Frustum3D { Frustum3D* value; };

// Navigation

// XR

// Network

constexpr const char* WindowName  = "moss.Window";
constexpr const char* MonitorName = "moss.Monitor";
constexpr const char* GamepadName = "moss.Gamepad";
constexpr const char* HapticName  = "moss.Haptic";
constexpr const char* CameraName  = "moss.Camera";
constexpr const char* StorageName = "moss.Storage";

constexpr const char* Vec2Name = "moss.Vec2";
constexpr const char* Vec3Name = "moss.Vec3";
constexpr const char* Vec4Name = "moss.Vec4";
constexpr const char* ColorName = "moss.Color";
constexpr const char* QuatName = "moss.Quat";
constexpr const char* RectName = "moss.Rect";
constexpr const char* RendererName = "moss.Renderer";


template<class T> int destroy(lua_State* L, const char* name) {
  check<T>(L, 1, name)->~T(); return 0;
}

static void push_path_info(lua_State* L, const Moss_PathInfo& x) {
    lua_newtable(L);
    lua_pushinteger(L,(lua_Integer)x.type); lua_setfield(L,-2,"type");
    lua_pushinteger(L,(lua_Integer)x.size); lua_setfield(L,-2,"size");
    lua_pushinteger(L,(lua_Integer)x.create_time); lua_setfield(L,-2,"create_time");
    lua_pushinteger(L,(lua_Integer)x.modify_time); lua_setfield(L,-2,"modify_time");
    lua_pushinteger(L,(lua_Integer)x.access_time); lua_setfield(L,-2,"access_time");
    lua_pushboolean(L,x.readable); lua_setfield(L,-2,"readable");
    lua_pushboolean(L,x.writable); lua_setfield(L,-2,"writable");
    lua_pushboolean(L,x.executable); lua_setfield(L,-2,"executable");
}

static void push_camera_spec(lua_State* L, const Moss_CameraSpec& x) {
    lua_newtable(L);
    lua_pushinteger(L,(lua_Integer)x.format); lua_setfield(L,-2,"format");
    lua_pushinteger(L,(lua_Integer)x.colorspace); lua_setfield(L,-2,"colorspace");
    lua_pushinteger(L,x.width); lua_setfield(L,-2,"width");
    lua_pushinteger(L,x.height); lua_setfield(L,-2,"height");
    lua_pushinteger(L,x.framerate_numerator); lua_setfield(L,-2,"framerate_numerator");
    lua_pushinteger(L,x.framerate_denominator); lua_setfield(L,-2,"framerate_denominator");
}

static void push_levels(lua_State* L, const Moss_MicrophoneLevels& x) {
    lua_newtable(L);
    lua_pushnumber(L,x.rms); lua_setfield(L,-2,"rms");
    lua_pushnumber(L,x.peak); lua_setfield(L,-2,"peak");
    lua_pushnumber(L,x.smoothed_volume); lua_setfield(L,-2,"smoothed_volume");
    lua_pushnumber(L,x.voice_activity); lua_setfield(L,-2,"voice_activity");
}

static void push_ray_result(lua_State* L, const Moss_AudioRayTraceResult& x) {
    lua_newtable(L);
    lua_pushboolean(L,x.audible); lua_setfield(L,-2,"audible");
    lua_pushboolean(L,x.occluded); lua_setfield(L,-2,"occluded");
    lua_pushnumber(L,x.distance); lua_setfield(L,-2,"distance");
    lua_pushnumber(L,x.attenuation); lua_setfield(L,-2,"attenuation");
    lua_pushnumber(L,x.occlusion); lua_setfield(L,-2,"occlusion");
    lua_pushnumber(L,x.transmission_gain); lua_setfield(L,-2,"transmission_gain");
    lua_pushnumber(L,x.lowpass); lua_setfield(L,-2,"lowpass");
    lua_pushnumber(L,x.reflection_gain); lua_setfield(L,-2,"reflection_gain");
    lua_pushnumber(L,x.reflection_delay_seconds); lua_setfield(L,-2,"reflection_delay_seconds");
    lua_pushnumber(L,x.delay_seconds); lua_setfield(L,-2,"delay_seconds");
}

template<class T> int vector_index(lua_State* L, const char* meta, int components) {
  auto* v = check<T>(L, 1, meta); const char* key = luaL_checkstring(L, 2);
  const char* keys[] = { "x", "y", "z", "w" };
  for (int i = 0; i < components; ++i) if (std::strcmp(key, keys[i]) == 0) { lua_pushnumber(L, (*v)[i]); return 1; }
  luaL_getmetatable(L, meta); lua_getfield(L, -1, key); return 1;
}
template<class T> int vector_newindex(lua_State* L, const char* meta, int components) {
  auto* v = check<T>(L, 1, meta); const char* key = luaL_checkstring(L, 2); float value = static_cast<float>(luaL_checknumber(L, 3));
  const char* keys[] = { "x", "y", "z", "w" };
  for (int i = 0; i < components; ++i) if (std::strcmp(key, keys[i]) == 0) { v->SetComponent(i, value); return 0; }
  return luaL_error(L, "unknown or read-only field '%s'", key);
}

static int push_bool(lua_State* L, bool b) { lua_pushboolean(L,b); return 1; }
static int push_str(lua_State* L, const char* s) { lua_pushstring(L,s?s:""); return 1; }



// -----------------------------------------------------------------------------
// stdinc / time
// -----------------------------------------------------------------------------
static int l_degrees_to_radians(lua_State*L){lua_pushnumber(L,DegreesToRadians((float)luaL_checknumber(L,1)));return 1;}
static int l_radians_to_degrees(lua_State*L){lua_pushnumber(L,RadiansToDegrees((float)luaL_checknumber(L,1)));return 1;}
static int l_center_angle(lua_State*L){lua_pushnumber(L,CenterAngleAroundZero((float)luaL_checknumber(L,1)));return 1;}
static int l_ctz(lua_State*L){lua_pushinteger(L,CountTrailingZeros((uint32)luaL_checkinteger(L,1)));return 1;}
static int l_clz(lua_State*L){lua_pushinteger(L,CountLeadingZeros((uint32)luaL_checkinteger(L,1)));return 1;}
static int l_count_bits(lua_State*L){lua_pushinteger(L,CountBits((uint32)luaL_checkinteger(L,1)));return 1;}
static int l_next_pow2(lua_State*L){lua_pushinteger(L,GetNextPowerOf2((uint32)luaL_checkinteger(L,1)));return 1;}
static int l_get_ticks(lua_State*L){lua_pushinteger(L,(lua_Integer)Moss_GetTicks());return 1;}
static int l_get_seconds(lua_State*L){lua_pushnumber(L,Moss_GetSeconds((Moss_Time)luaL_checkinteger(L,1)));return 1;}
static int l_get_ms(lua_State*L){lua_pushnumber(L,Moss_GetMilliseconds((Moss_Time)luaL_checkinteger(L,1)));return 1;}
static int l_get_ms_reset(lua_State*L){Moss_Time t=(Moss_Time)luaL_checkinteger(L,1);lua_pushnumber(L,Moss_GetMillisecondsAndReset(&t));lua_pushinteger(L,(lua_Integer)t);return 2;}
static int l_yield(lua_State*L){(void)L;Moss_Yield();return 0;}
static int l_delay(lua_State*L){lua_pushnumber(L,Moss_Delay(luaL_checknumber(L,1)));return 1;}
static int l_time_string(lua_State*L,const char*which){const char*s=nullptr;if(!std::strcmp(which,"local"))s=Moss_LocalTime();else if(!std::strcmp(which,"stamp"))s=Moss_TimeStamp();else if(!std::strcmp(which,"now"))s=Moss_TimeNow();else if(!std::strcmp(which,"ctime"))s=Moss_CTimeNow();else if(!std::strcmp(which,"et"))s=Moss_TimeStampET();else if(!std::strcmp(which,"ct"))s=Moss_TimeStampCT();else if(!std::strcmp(which,"mt"))s=Moss_TimeStampMT();else if(!std::strcmp(which,"pt"))s=Moss_TimeStampPT();return push_str(L,s);}
static int l_local(lua_State*L){return l_time_string(L,"local");} static int l_stamp(lua_State*L){return l_time_string(L,"stamp");} static int l_now(lua_State*L){return l_time_string(L,"now");} static int l_ctime(lua_State*L){return l_time_string(L,"ctime");} static int l_et(lua_State*L){return l_time_string(L,"et");} static int l_ct(lua_State*L){return l_time_string(L,"ct");} static int l_mt(lua_State*L){return l_time_string(L,"mt");} static int l_pt(lua_State*L){return l_time_string(L,"pt");}
static int l_format_time(lua_State*L){const char*s=Moss_FormatTime(luaL_checkstring(L,1));return push_str(L,s);}

// -----------------------------------------------------------------------------
// Platform
// -----------------------------------------------------------------------------
#define PTRFN0(NAME, CNAME, T, MT) static int NAME(lua_State*L){return push_ud(L,CNAME(),MT);}
#define VOIDPTR1(NAME,CNAME,T,MT) static int NAME(lua_State*L){CNAME(check_ud<T>(L,1,MT));return 0;}

static int l_window_new(lua_State*L){const char*t=luaL_checkstring(L,1);int w=(int)luaL_checkinteger(L,2);int h=(int)luaL_checkinteger(L,3);auto*p=Moss_CreateWindow(t,w,h,nullptr,nullptr);if(!p)return luaL_error(L,"Moss_CreateWindow failed");return push_ud(L,p,"moss.Window");}
static int l_window_gc(lua_State*L){auto**p=static_cast<Moss_Window**>(luaL_checkudata(L,1,"moss.Window"));if(*p){Moss_TerminateWindow(*p);*p=nullptr;}return 0;}
static int l_window_should_close(lua_State*L){return push_bool(L,Moss_ShouldWindowClose(check_ud<Moss_Window>(L,1,"moss.Window")));}
static int l_window_close(lua_State*L){Moss_CloseWindow(check_ud<Moss_Window>(L,1,"moss.Window"));return 0;}
static int l_window_title(lua_State*L){Moss_SetWindowTitle(check_ud<Moss_Window>(L,1,"moss.Window"),luaL_checkstring(L,2));return 0;}
static int l_create_message_box(lua_State*L){return push_bool(L,Moss_CreateMessageBox(luaL_checkstring(L,1),luaL_checkstring(L,2),(Moss_MessageBoxFlags)luaL_checkinteger(L,3),nullptr));}
static int l_poll(lua_State*L){(void)L;Moss_PollEvents();return 0;}
static int l_window_width(lua_State*L){lua_pushinteger(L,Moss_GetWindowWidth());lua_pushinteger(L,Moss_GetWindowHeight());return 2;}
static int l_monitor_primary(lua_State*L){return push_ud(L,Moss_MonitorGetPrimary(),"moss.Monitor");}
static int l_monitor_secondary(lua_State*L){return push_ud(L,Moss_MonitorGetSecondary(),"moss.Monitor");}
static int l_monitor_position(lua_State*L){auto*p=check_ud<Moss_Monitor>(L,1,"moss.Monitor");int x=0,y=0;Moss_MonitorGetPosition(p,&x,&y);lua_pushinteger(L,x);lua_pushinteger(L,y);return 2;}
static int l_monitor_physical(lua_State*L){auto*p=check_ud<Moss_Monitor>(L,1,"moss.Monitor");int x=0,y=0;Moss_MonitorGetPhysicalSize(p,&x,&y);lua_pushinteger(L,x);lua_pushinteger(L,y);return 2;}
static int l_monitor_scale(lua_State*L){auto*p=check_ud<Moss_Monitor>(L,1,"moss.Monitor");float x=0,y=0;Moss_MonitorGetContentScale(p,&x,&y);lua_pushnumber(L,x);lua_pushnumber(L,y);return 2;}
static int l_monitor_name(lua_State*L){return push_str(L,Moss_MonitorGetName(check_ud<Moss_Monitor>(L,1,"moss.Monitor")));}
static int l_monitor_gamma(lua_State*L){Moss_MonitorSetGamma(check_ud<Moss_Monitor>(L,1,"moss.Monitor"),(float)luaL_checknumber(L,2));return 0;}
static int l_key_pressed(lua_State*L){return push_bool(L,Moss_IsKeyPressed((Keyboard)luaL_checkinteger(L,1)));}
static int l_key_released(lua_State*L){return push_bool(L,Moss_IsReleased((Keyboard)luaL_checkinteger(L,1)));}
static int l_key_just_pressed(lua_State*L){return push_bool(L,Moss_IsKeyJustPressed((Keyboard)luaL_checkinteger(L,1)));}
static int l_key_just_released(lua_State*L){return push_bool(L,Moss_IsKeyJustReleased((Keyboard)luaL_checkinteger(L,1)));}
static int l_input_key(lua_State*L){lua_pushinteger(L,(lua_Integer)Moss_InputGetKey());return 1;}
static int l_mouse_pressed(lua_State*L){return push_bool(L,Moss_IsMousePressed((Mouse)luaL_checkinteger(L,1)));}
static int l_mouse_released(lua_State*L){return push_bool(L,Moss_IsMouseReleased((Mouse)luaL_checkinteger(L,1)));}
static int l_mouse_just_pressed(lua_State*L){return push_bool(L,Moss_IsMouseJustPressed((Mouse)luaL_checkinteger(L,1)));}
static int l_mouse_just_released(lua_State*L){return push_bool(L,Moss_IsMouseJustReleased((Mouse)luaL_checkinteger(L,1)));}
static int l_input_mouse(lua_State*L){lua_pushinteger(L,(lua_Integer)Moss_InputGetMouseButton());return 1;}
static int l_mouse_position(lua_State*L){int x=0,y=0;Moss_GetMousePosition(&x,&y);lua_pushinteger(L,x);lua_pushinteger(L,y);return 2;}
static int l_set_mouse_position(lua_State*L){Moss_SetMousePosition((int)luaL_checkinteger(L,1),(int)luaL_checkinteger(L,2));return 0;}
static int l_set_mouse_visible(lua_State*L){Moss_SetMouseVisible(lua_toboolean(L,1));return 0;}
static int l_num_gamepads(lua_State*L){lua_pushinteger(L,Moss_GetNumGamepads());return 1;}
static int l_gamepad_open(lua_State*L){return push_ud(L,Moss_OpenGamepad((Moss_GamepadID)luaL_checkinteger(L,1)),"moss.Gamepad");}
static int l_gamepad_gc(lua_State*L){auto**p=static_cast<Moss_Gamepad**>(luaL_checkudata(L,1,"moss.Gamepad"));if(*p){Moss_CloseGamepad(*p);*p=nullptr;}return 0;}
static int l_gamepad_bool(lua_State*L,int which){auto*p=check_ud<Moss_Gamepad>(L,1,"moss.Gamepad");auto b=(Moss_GamepadButton)luaL_checkinteger(L,2);bool v=which==0?Moss_IsGamepadButtonPressed(p,b):which==1?Moss_IsGamepadButtonJustPressed(p,b):Moss_IsGamepadButtonJustReleased(p,b);return push_bool(L,v);}
static int l_gamepad_pressed(lua_State*L){return l_gamepad_bool(L,0);} static int l_gamepad_just_pressed(lua_State*L){return l_gamepad_bool(L,1);} static int l_gamepad_just_released(lua_State*L){return l_gamepad_bool(L,2);}
static int l_gamepad_connected(lua_State*L){return push_bool(L,Moss_GamepadConnected(check_ud<Moss_Gamepad>(L,1,"moss.Gamepad")));}
static int l_gamepad_axis(lua_State*L){lua_pushnumber(L,Moss_GetGamepadAxis(check_ud<Moss_Gamepad>(L,1,"moss.Gamepad"),(GamepadAxis)luaL_checkinteger(L,2)));return 1;}
static int l_update_gamepads(lua_State*L){(void)L;Moss_UpdateGamepads();return 0;}
static int l_rumble(lua_State*L){auto*p=check_ud<Moss_Gamepad>(L,1,"moss.Gamepad");return push_bool(L,Moss_RumbleGamepad(p,(uint16_t)luaL_checkinteger(L,2),(uint16_t)luaL_checkinteger(L,3),(uint32_t)luaL_checkinteger(L,4)));}
static int l_rumble_triggers(lua_State*L){auto*p=check_ud<Moss_Gamepad>(L,1,"moss.Gamepad");return push_bool(L,Moss_RumbleGamepadTriggers(p,(uint16_t)luaL_checkinteger(L,2),(uint16_t)luaL_checkinteger(L,3),(uint32_t)luaL_checkinteger(L,4)));}
static int l_gamepad_led(lua_State*L){auto*p=check_ud<Moss_Gamepad>(L,1,"moss.Gamepad");return push_bool(L,Moss_SetGamepadLED(p,(uint8_t)luaL_checkinteger(L,2),(uint8_t)luaL_checkinteger(L,3),(uint8_t)luaL_checkinteger(L,4)));}
static int l_gamepad_name(lua_State*L){return push_str(L,Moss_GetGamepadName(check_ud<Moss_Gamepad>(L,1,"moss.Gamepad")));}
static int l_gamepad_id(lua_State*L){lua_pushinteger(L,Moss_GetGamepadID(check_ud<Moss_Gamepad>(L,1,"moss.Gamepad")));return 1;}
static int l_gamepad_player(lua_State*L){lua_pushinteger(L,Moss_GetGamepadPlayerIndex(check_ud<Moss_Gamepad>(L,1,"moss.Gamepad")));return 1;}
static int l_gamepad_power(lua_State*L){int p=0;auto s=Moss_GetGamepadPowerInfo(check_ud<Moss_Gamepad>(L,1,"moss.Gamepad"),&p);lua_pushinteger(L,(lua_Integer)s);lua_pushinteger(L,p);return 2;}
static int l_gamepad_mapping(lua_State*L){return push_str(L,Moss_GetGamepadMapping(check_ud<Moss_Gamepad>(L,1,"moss.Gamepad")));}
static int l_gamepad_set_mapping(lua_State*L){return push_bool(L,Moss_SetGamepadMapping(check_ud<Moss_Gamepad>(L,1,"moss.Gamepad"),luaL_checkstring(L,2)));}
static int l_reload_mappings(lua_State*L){(void)L;Moss_ReloadGamepadMappings();return 0;}
static int l_set_axis_deadzone(lua_State*L){Moss_SetGamepadAxisDeadzone((GamepadAxis)luaL_checkinteger(L,1),(float)luaL_checknumber(L,2));return 0;}
static int l_set_axis_inverted(lua_State*L){Moss_SetGamepadAxisInverted((GamepadAxis)luaL_checkinteger(L,1),lua_toboolean(L,2));return 0;}

// Haptics / system
static int l_open_haptic(lua_State*L){return push_ud(L,Moss_OpenHaptic((Moss_HapticID)luaL_checkinteger(L,1)),"moss.Haptic");}
static int l_haptic_gc(lua_State*L){auto**p=static_cast<Moss_Haptic**>(luaL_checkudata(L,1,"moss.Haptic"));if(*p){Moss_CloseHaptic(*p);*p=nullptr;}return 0;}
static int l_haptic_bool(lua_State*L,int op){auto*p=check_ud<Moss_Haptic>(L,1,"moss.Haptic");bool b=false;switch(op){case 0:b=Moss_GetHapticEffectStatus(p);break;case 1:b=Moss_HapticEffectSupported(p);break;case 2:b=Moss_HapticRumbleSupported(p);break;case 3:b=Moss_PauseHaptic(p);break;case 4:b=Moss_ResumeHaptic(p);break;case 5:b=Moss_StopHapticEffects(p);break;case 6:b=Moss_StopHapticRumble(p);break;}return push_bool(L,b);}
static int l_haptic_status(lua_State*L){return l_haptic_bool(L,0);} static int l_haptic_supported(lua_State*L){return l_haptic_bool(L,1);} static int l_haptic_rumble_supported(lua_State*L){return l_haptic_bool(L,2);} static int l_pause_haptic(lua_State*L){return l_haptic_bool(L,3);} static int l_resume_haptic(lua_State*L){return l_haptic_bool(L,4);} static int l_stop_haptic_effects(lua_State*L){return l_haptic_bool(L,5);} static int l_stop_haptic_rumble(lua_State*L){return l_haptic_bool(L,6);}
static int l_haptic_create_effect(lua_State*L){lua_pushinteger(L,Moss_CreateHapticEffect(check_ud<Moss_Haptic>(L,1,"moss.Haptic")));return 1;}
static int l_haptic_features(lua_State*L){lua_pushinteger(L,Moss_GetHapticFeatures(check_ud<Moss_Haptic>(L,1,"moss.Haptic")));return 1;}
static int l_haptic_name(lua_State*L){return push_str(L,Moss_GetHapticName(check_ud<Moss_Haptic>(L,1,"moss.Haptic")));}
static int l_haptic_name_id(lua_State*L){return push_str(L,Moss_GetHapticNameForID(check_ud<Moss_Haptic>(L,1,"moss.Haptic")));}
static int l_haptic_limits(lua_State*L){auto*p=check_ud<Moss_Haptic>(L,1,"moss.Haptic");lua_pushinteger(L,Moss_GetMaxHapticEffects(p));lua_pushinteger(L,Moss_GetMaxHapticEffectsPlaying(p));lua_pushinteger(L,Moss_GetNumHapticAxes(p));return 3;}
static int l_haptic_gain(lua_State*L){return push_bool(L,Moss_SetHapticGain(check_ud<Moss_Haptic>(L,1,"moss.Haptic"),(int)luaL_checkinteger(L,2)));}
static int l_haptic_autocenter(lua_State*L){return push_bool(L,Moss_SetHapticAutocenter(check_ud<Moss_Haptic>(L,1,"moss.Haptic"),(int)luaL_checkinteger(L,2)));}
static int l_haptic_rumble(lua_State*L){return push_bool(L,Moss_PlayHapticRumble(check_ud<Moss_Haptic>(L,1,"moss.Haptic"),(float)luaL_checknumber(L,2),(uint32_t)luaL_checkinteger(L,3)));}
static int l_run_haptic(lua_State*L){return push_bool(L,Moss_RunHapticEffect(check_ud<Moss_Haptic>(L,1,"moss.Haptic"),(uint32_t)luaL_checkinteger(L,2)));}
static int l_stop_haptic_effect(lua_State*L){return push_bool(L,Moss_StopHapticEffect(check_ud<Moss_Haptic>(L,1,"moss.Haptic"),(Moss_HapticEffectID)luaL_checkinteger(L,2)));}
static int l_destroy_haptic_effect(lua_State*L){Moss_DestroyHapticEffect(check_ud<Moss_Haptic>(L,1,"moss.Haptic"));return 0;}

static int l_cpu_count(lua_State*L){lua_pushinteger(L,Moss_GetAvailableCPUCores());return 1;} static int l_cpu_cache(lua_State*L){lua_pushinteger(L,Moss_GetCPUCacheLineSize());return 1;} static int l_ram(lua_State*L){lua_pushinteger(L,Moss_GetSystemRAM());return 1;}
static int l_open_url(lua_State*L){return push_bool(L,Moss_OpenURL(luaL_checkstring(L,1)));}
static int l_power_info(lua_State*L){int s=0,p=0;auto st=Moss_GetPowerInfo(&s,&p);lua_pushinteger(L,(lua_Integer)st);lua_pushinteger(L,s);lua_pushinteger(L,p);return 3;}
static int l_process_running(lua_State*L){return push_bool(L,Moss_IsProcessRunningByName(luaL_checkstring(L,1)));}
static int l_load_library(lua_State*L){lua_pushlightuserdata(L,Moss_LoadDynamicLibrary(luaL_checkstring(L,1)));return 1;}
static int l_get_symbol(lua_State*L){return push_ud(L,(void*)Moss_GetLibrarySymbol(lua_touserdata(L,1),luaL_checkstring(L,2)),"moss.RawPointer");}
static int l_unload_library(lua_State*L){Moss_UnloadDynamicLibrary(lua_touserdata(L,1));return 0;}

static int l_path_info(lua_State*L){Moss_PathInfo x{};if(!Moss_GetPathInfo(luaL_checkstring(L,1),&x)){lua_pushnil(L);return 1;}push_path_info(L,x);return 1;}
static int l_copy_file(lua_State*L){return push_bool(L,Moss_CopyFile(luaL_checkstring(L,1),luaL_checkstring(L,2),lua_toboolean(L,3)));}
static int l_create_dir(lua_State*L){return push_bool(L,Moss_CreateDirectory(luaL_checkstring(L,1),lua_toboolean(L,2)));}
static int l_remove_path(lua_State*L){return push_bool(L,Moss_RemovePath(luaL_checkstring(L,1),lua_toboolean(L,2)));}
static int l_rename_path(lua_State*L){return push_bool(L,Moss_RenamePath(luaL_checkstring(L,1),luaL_checkstring(L,2),lua_toboolean(L,3)));}
static int l_current_dir(lua_State*L){char b[4096]{};Moss_GetCurrentDirectory(b,sizeof(b));lua_pushstring(L,b);return 1;}
static int l_base_path(lua_State*L){char b[4096]{};Moss_GetBasePath(b,sizeof(b));lua_pushstring(L,b);return 1;}
static int l_user_folder(lua_State*L){char b[4096]{};Moss_GetUserFolder((Moss_UserFolder)luaL_checkinteger(L,1),b,sizeof(b));lua_pushstring(L,b);return 1;}
static int l_pref_path(lua_State*L){char b[4096]{};Moss_GetPrefPath(luaL_checkstring(L,1),luaL_checkstring(L,2),b,sizeof(b));lua_pushstring(L,b);return 1;}
static int l_open_storage(lua_State*L){return push_ud(L,Moss_OpenFileStorage(luaL_checkstring(L,1)),"moss.Storage");}
static int l_storage_gc(lua_State*L){auto**p=static_cast<Moss_Storage**>(luaL_checkudata(L,1,"moss.Storage"));if(*p){Moss_CloseStorage(*p);*p=nullptr;}return 0;}
static int l_storage_ready(lua_State*L){return push_bool(L,Moss_StorageReady(check_ud<Moss_Storage>(L,1,"moss.Storage")));}
static int l_storage_create_dir(lua_State*L){return push_bool(L,Moss_CreateStorageDirectory(check_ud<Moss_Storage>(L,1,"moss.Storage"),luaL_checkstring(L,2)));}
static int l_storage_copy(lua_State*L){return push_bool(L,Moss_CopyStorageFile(check_ud<Moss_Storage>(L,1,"moss.Storage"),luaL_checkstring(L,2),luaL_checkstring(L,3)));}
static int l_storage_remove(lua_State*L){return push_bool(L,Moss_RemoveStoragePath(check_ud<Moss_Storage>(L,1,"moss.Storage"),luaL_checkstring(L,2)));}
static int l_storage_rename(lua_State*L){return push_bool(L,Moss_RenameStoragePath(check_ud<Moss_Storage>(L,1,"moss.Storage"),luaL_checkstring(L,2),luaL_checkstring(L,3)));}
static int l_storage_size(lua_State*L){uint64 n=0;bool ok=Moss_GetStorageFileSize(check_ud<Moss_Storage>(L,1,"moss.Storage"),luaL_checkstring(L,2),&n);lua_pushboolean(L,ok);lua_pushinteger(L,(lua_Integer)n);return 2;}
static int l_storage_space(lua_State*L){lua_pushinteger(L,(lua_Integer)Moss_GetStorageSpaceRemaining(check_ud<Moss_Storage>(L,1,"moss.Storage")));return 1;}
static int l_storage_path_info(lua_State*L){Moss_PathInfo x{};if(!Moss_GetStoragePathInfo(check_ud<Moss_Storage>(L,1,"moss.Storage"),luaL_checkstring(L,2),&x)){lua_pushnil(L);return 1;}push_path_info(L,x);return 1;}
static int l_storage_read(lua_State*L){auto*p=check_ud<Moss_Storage>(L,1,"moss.Storage");const char*path=luaL_checkstring(L,2);size_t n=(size_t)luaL_checkinteger(L,3);std::string b(n,'\0');if(!Moss_ReadStorageFile(p,path,b.data(),n)){lua_pushnil(L);return 1;}lua_pushlstring(L,b.data(),b.size());return 1;}
static int l_storage_write(lua_State*L){size_t n=0;const char*b=luaL_checklstring(L,3,&n);return push_bool(L,Moss_WriteStorageFile(check_ud<Moss_Storage>(L,1,"moss.Storage"),luaL_checkstring(L,2),b,n));}

// -----------------------------------------------------------------------------
// Audio
// -----------------------------------------------------------------------------
static int l_audio_init(lua_State*L){lua_pushinteger(L,Moss_Init_Audio());return 1;} static int l_audio_term(lua_State*L){(void)L;Moss_Terminate_Audio();return 0;} static int l_audio_update(lua_State*L){Moss_AudioUpdate((float)luaL_checknumber(L,1));return 0;}
static int l_audio_load_wav(lua_State*L){return push_ud(L,Moss_AudioLoadWavFile(luaL_checkstring(L,1)),"moss.AudioSource");}
static int l_audio_load_wav_legacy(lua_State*L){return push_ud(L,Moss_AudioLoadWav(),"moss.AudioSource");}
static int l_audio_load_ogg(lua_State*L){return push_ud(L,Moss_AudioLoadOgg(luaL_checkstring(L,1),(AudioLoadType)luaL_checkinteger(L,2)),"moss.AudioSource");}
static int l_audio_load_mp3(lua_State*L){return push_ud(L,Moss_AudioLoadMP3(luaL_checkstring(L,1)),"moss.AudioSource");}
static int l_audio_source_gc(lua_State*L){auto**p=static_cast<Moss_AudioSource**>(luaL_checkudata(L,1,"moss.AudioSource"));if(*p){Moss_AudioSourceDestroy(*p);*p=nullptr;}return 0;}
static int l_audio_capture_mic(lua_State*L){return push_ud(L,Moss_AudioCaptureMicrophone(check_ud<Moss_Microphone>(L,1,"moss.Microphone")),"moss.AudioSource");}
static int l_audio_create_channel(lua_State*L){lua_pushinteger(L,Moss_AudioCreateChannel((ChannelID)luaL_checkinteger(L,1)));return 1;}
static int l_audio_remove_channel(lua_State*L){Audio_RemoveChannel((ChannelID)luaL_checkinteger(L,1));return 0;}
static int l_audio_master_channel(lua_State*L){lua_pushinteger(L,Moss_AudioGetMasterChannel());return 1;}
static int l_audio_channel_volume(lua_State*L){Moss_AudioSetChannelVolume((ChannelID)luaL_checkinteger(L,1),(float)luaL_checknumber(L,2));return 0;}
static int l_audio_channel_mute(lua_State*L){Moss_AudioSetChannelMute((ChannelID)luaL_checkinteger(L,1),lua_toboolean(L,2));return 0;}
static int l_audio_stream_create(lua_State*L){return push_ud(L,Moss_AudioStreamCreate(),"moss.AudioStream");}
static int l_audio_stream_gc(lua_State*L){auto**p=static_cast<AudioStream**>(luaL_checkudata(L,1,"moss.AudioStream"));if(*p){Moss_AudioStreamRemove(*p);*p=nullptr;}return 0;}
#define STREAM_BOOL(NAME,CNAME,TYPE,FIELD) static int NAME(lua_State*L){CNAME(check_ud<TYPE>(L,1,"moss." #FIELD), (float)luaL_checknumber(L,2));return 0;}
static int l_stream_play(lua_State*L){Moss_AudioStreamPlay(check_ud<AudioStream>(L,1,"moss.AudioStream"));return 0;} static int l_stream_stop(lua_State*L){Moss_AudioStreamStop(check_ud<AudioStream>(L,1,"moss.AudioStream"));return 0;} static int l_stream_volume(lua_State*L){Moss_AudioStreamSetVolume(check_ud<AudioStream>(L,1,"moss.AudioStream"),(float)luaL_checknumber(L,2));return 0;} static int l_stream_pitch(lua_State*L){Moss_AudioStreamSetPitch(check_ud<AudioStream>(L,1,"moss.AudioStream"),(float)luaL_checknumber(L,2));return 0;} static int l_stream_rate(lua_State*L){Moss_AudioStreamSetPlaybackRate(check_ud<AudioStream>(L,1,"moss.AudioStream"),(float)luaL_checknumber(L,2));return 0;} static int l_stream_pan(lua_State*L){Moss_AudioStreamSetPan(check_ud<AudioStream>(L,1,"moss.AudioStream"),(float)luaL_checknumber(L,2));return 0;} static int l_stream_loop(lua_State*L){Moss_AudioStreamSetLoop(check_ud<AudioStream>(L,1,"moss.AudioStream"),lua_toboolean(L,2));return 0;}
static int l_stream2d_create(lua_State*L){return push_ud(L,Moss_AudioStream2DCreate(),"moss.AudioStream2D");} static int l_stream2d_gc(lua_State*L){auto**p=static_cast<AudioStream2D**>(luaL_checkudata(L,1,"moss.AudioStream2D"));if(*p){Moss_AudioStream2DRemove(*p);*p=nullptr;}return 0;}
static int l_stream2d_play(lua_State*L){Moss_AudioStream2DPlay(check_ud<AudioStream2D>(L,1,"moss.AudioStream2D"));return 0;} static int l_stream2d_stop(lua_State*L){Moss_AudioStream2DStop(check_ud<AudioStream2D>(L,1,"moss.AudioStream2D"));return 0;} static int l_stream2d_volume(lua_State*L){Moss_AudioStream2DSetVolume(check_ud<AudioStream2D>(L,1,"moss.AudioStream2D"),(float)luaL_checknumber(L,2));return 0;} static int l_stream2d_pitch(lua_State*L){Moss_AudioStream2DSetPitch(check_ud<AudioStream2D>(L,1,"moss.AudioStream2D"),(float)luaL_checknumber(L,2));return 0;} static int l_stream2d_rate(lua_State*L){Moss_AudioStream2DSetPlaybackRate(check_ud<AudioStream2D>(L,1,"moss.AudioStream2D"),(float)luaL_checknumber(L,2));return 0;} static int l_stream2d_pan(lua_State*L){Moss_AudioStream2DSetPan(check_ud<AudioStream2D>(L,1,"moss.AudioStream2D"),(float)luaL_checknumber(L,2));return 0;} static int l_stream2d_loop(lua_State*L){Moss_AudioStream2DSetLoop(check_ud<AudioStream2D>(L,1,"moss.AudioStream2D"),lua_toboolean(L,2));return 0;}
static int l_stream3d_create(lua_State*L){return push_ud(L,Moss_AudioStream3DCreate(),"moss.AudioStream3D");} static int l_stream3d_gc(lua_State*L){auto**p=static_cast<AudioStream3D**>(luaL_checkudata(L,1,"moss.AudioStream3D"));if(*p){Moss_AudioStream3DRemove(*p);*p=nullptr;}return 0;}
static int l_stream3d_play(lua_State*L){Moss_AudioStream3DPlay(check_ud<AudioStream3D>(L,1,"moss.AudioStream3D"));return 0;} static int l_stream3d_stop(lua_State*L){Moss_AudioStream3DStop(check_ud<AudioStream3D>(L,1,"moss.AudioStream3D"));return 0;} static int l_stream3d_volume(lua_State*L){Moss_AudioStream3DSetVolume(check_ud<AudioStream3D>(L,1,"moss.AudioStream3D"),(float)luaL_checknumber(L,2));return 0;} static int l_stream3d_pitch(lua_State*L){Moss_AudioStream3DSetPitch(check_ud<AudioStream3D>(L,1,"moss.AudioStream3D"),(float)luaL_checknumber(L,2));return 0;} static int l_stream3d_rate(lua_State*L){Moss_AudioStream3DSetPlaybackRate(check_ud<AudioStream3D>(L,1,"moss.AudioStream3D"),(float)luaL_checknumber(L,2));return 0;} static int l_stream3d_pan(lua_State*L){Moss_AudioStream3DSetPan(check_ud<AudioStream3D>(L,1,"moss.AudioStream3D"),(float)luaL_checknumber(L,2));return 0;} static int l_stream3d_loop(lua_State*L){Moss_AudioStream3DSetLoop(check_ud<AudioStream3D>(L,1,"moss.AudioStream3D"),lua_toboolean(L,2));return 0;} static int l_stream3d_model(lua_State*L){Moss_AudioStream3DSetDistanceModel(check_ud<AudioStream3D>(L,1,"moss.AudioStream3D"),(DistanceModel)luaL_checkinteger(L,2));return 0;}
static int l_listener2d_new(lua_State*L){Float2 v{(float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2)};return push_ud(L,Moss_AudioCreateAudioListener2D(v),"moss.AudioListener2D");}
static int l_listener3d_new(lua_State*L){Float3 v{(float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3)};return push_ud(L,Moss_AudioCreateAudioListener3D(v),"moss.AudioListener3D");}
static int l_raylistener2d_new(lua_State*L){Float2 v{(float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2)};return push_ud(L,Moss_AudioCreateRayAudioListener2D(v),"moss.RayAudioListener2D");}
static int l_raylistener3d_new(lua_State*L){Float3 v{(float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3)};return push_ud(L,Moss_AudioCreateRayAudioListener3D(v),"moss.RayAudioListener3D");}
static int l_listener2d_gc(lua_State*L){auto**p=static_cast<AudioListener2D**>(luaL_checkudata(L,1,"moss.AudioListener2D"));if(*p){Moss_AudioRemoveAudioListener2D(*p);*p=nullptr;}return 0;} static int l_listener3d_gc(lua_State*L){auto**p=static_cast<AudioListener3D**>(luaL_checkudata(L,1,"moss.AudioListener3D"));if(*p){Moss_AudioRemoveAudioListener3D(*p);*p=nullptr;}return 0;} static int l_raylistener2d_gc(lua_State*L){auto**p=static_cast<RayAudioListener2D**>(luaL_checkudata(L,1,"moss.RayAudioListener2D"));if(*p){Moss_AudioRemoveRayAudioListener2D(*p);*p=nullptr;}return 0;} static int l_raylistener3d_gc(lua_State*L){auto**p=static_cast<RayAudioListener3D**>(luaL_checkudata(L,1,"moss.RayAudioListener3D"));if(*p){Moss_AudioRemoveRayAudioListener3D(*p);*p=nullptr;}return 0;}
static int l_listener_activate(lua_State*L, int which){bool a=lua_toboolean(L,2);if(which==0)Moss_AudioActivateAudioListener2D(check_ud<AudioListener2D>(L,1,"moss.AudioListener2D"),a);else if(which==1)Moss_AudioActivateAudioListener3D(check_ud<AudioListener3D>(L,1,"moss.AudioListener3D"),a);else if(which==2)Moss_AudioActivateRayAudioListener2D(check_ud<RayAudioListener2D>(L,1,"moss.RayAudioListener2D"),a);else Moss_AudioActivateRayAudioListener3D(check_ud<RayAudioListener3D>(L,1,"moss.RayAudioListener3D"),a);return 0;}
static int l_listener2d_activate(lua_State*L){return l_listener_activate(L,0);} static int l_listener3d_activate(lua_State*L){return l_listener_activate(L,1);} static int l_raylistener2d_activate(lua_State*L){return l_listener_activate(L,2);} static int l_raylistener3d_activate(lua_State*L){return l_listener_activate(L,3);}
static int l_audio_listener_orientation(lua_State*L){Vec3 f((float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4));Vec3 u((float)luaL_checknumber(L,5),(float)luaL_checknumber(L,6),(float)luaL_checknumber(L,7));Moss_AudioListenerSetOrientation(check_ud<AudioListener3D>(L,1,"moss.AudioListener3D"),f,u);return 0;}
static int l_speaker_ready(lua_State*L){return push_bool(L,Moss_IsSpeakerDeviceReady());} static int l_speaker_open(lua_State*L){(void)L;Moss_AudioSpeakerOpen();return 0;} static int l_speaker_pause(lua_State*L){(void)L;Moss_AudioSpeakerPause();return 0;} static int l_speaker_resume(lua_State*L){(void)L;Moss_AudioSpeakerResume();return 0;} static int l_speaker_paused(lua_State*L){return push_bool(L,Moss_AudioSpeakerIsPaused());} static int l_speaker_select(lua_State*L){return push_bool(L,Moss_AudioSelectSpeakerDevice((int)luaL_checkinteger(L,1)));} static int l_speaker_current(lua_State*L){lua_pushinteger(L,Moss_GetCurrentSpeakerDeviceID());return 1;} static int l_speaker_name(lua_State*L){return push_str(L,Moss_GetSpeakerDeviceName((int)luaL_checkinteger(L,1)));} static int l_speaker_count(lua_State*L){lua_pushinteger(L,Moss_ListSpeakerDevices());return 1;}
static int l_mic_count(lua_State*L){lua_pushinteger(L,Moss_MicrophoneGetDeviceCount());return 1;} static int l_mic_name(lua_State*L){return push_str(L,Moss_MicrophoneGetDeviceName((uint32)luaL_checkinteger(L,1)));}
static int l_mic_new(lua_State*L){Moss_MicrophoneDesc d{};d.device_index=(uint32)luaL_optinteger(L,1,0);d.sample_rate=(uint32)luaL_optinteger(L,2,48000);d.channels=(uint32)luaL_optinteger(L,3,1);d.buffer_frames=(uint32)luaL_optinteger(L,4,480);d.ring_buffer_frames=(uint32)luaL_optinteger(L,5,48000);d.start_immediately=lua_isnoneornil(L,6)?true:lua_toboolean(L,6);d.enable_voice_metrics=lua_isnoneornil(L,7)?true:lua_toboolean(L,7);return push_ud(L,Moss_MicrophoneOpen(&d),"moss.Microphone");}
static int l_mic_gc(lua_State*L){auto**p=static_cast<Moss_Microphone**>(luaL_checkudata(L,1,"moss.Microphone"));if(*p){Moss_MicrophoneClose(*p);*p=nullptr;}return 0;} static int l_mic_start(lua_State*L){return push_bool(L,Moss_MicrophoneStart(check_ud<Moss_Microphone>(L,1,"moss.Microphone")));} static int l_mic_stop(lua_State*L){Moss_MicrophoneStop(check_ud<Moss_Microphone>(L,1,"moss.Microphone"));return 0;} static int l_mic_read(lua_State*L){auto*p=check_ud<Moss_Microphone>(L,1,"moss.Microphone");uint32 n=(uint32)luaL_checkinteger(L,2);std::vector<float>b(n);n=Moss_MicrophoneRead(p,b.data(),n);lua_createtable(L,n,0);for(uint32 i=0;i<n;i++){lua_pushnumber(L,b[i]);lua_rawseti(L,-2,(lua_Integer)i+1);}return 1;} static int l_mic_gain(lua_State*L){Moss_MicrophoneSetGain(check_ud<Moss_Microphone>(L,1,"moss.Microphone"),(float)luaL_checknumber(L,2));return 0;} static int l_mic_sr(lua_State*L){lua_pushinteger(L,Moss_MicrophoneGetSampleRate(check_ud<Moss_Microphone>(L,1,"moss.Microphone")));return 1;} static int l_mic_channels(lua_State*L){lua_pushinteger(L,Moss_MicrophoneGetChannels(check_ud<Moss_Microphone>(L,1,"moss.Microphone")));return 1;} static int l_mic_levels(lua_State*L){push_levels(L,Moss_MicrophoneGetLevels(check_ud<Moss_Microphone>(L,1,"moss.Microphone")));return 1;} static int l_mic_rms(lua_State*L){lua_pushnumber(L,Moss_MicrophoneGetLevelRMS(check_ud<Moss_Microphone>(L,1,"moss.Microphone")));return 1;} static int l_mic_peak(lua_State*L){lua_pushnumber(L,Moss_MicrophoneGetLevelPeak(check_ud<Moss_Microphone>(L,1,"moss.Microphone")));return 1;} static int l_mic_smooth(lua_State*L){lua_pushnumber(L,Moss_MicrophoneGetSmoothedVolume(check_ud<Moss_Microphone>(L,1,"moss.Microphone")));return 1;} static int l_mic_voice(lua_State*L){lua_pushnumber(L,Moss_MicrophoneGetVoiceActivity(check_ud<Moss_Microphone>(L,1,"moss.Microphone")));return 1;} static int l_mic_ready(lua_State*L){return push_bool(L,Moss_IsMicrophoneDeviceReady());}
static int l_audio_mic_open(lua_State*L){lua_pushinteger(L,Moss_AudioMicrophoneOpen());return 1;} static int l_audio_mic_close(lua_State*L){(void)L;Moss_AudioMicrophoneClose();return 0;} static int l_audio_mic_play(lua_State*L){(void)L;Moss_AudioMicrophonePlay();return 0;} static int l_audio_mic_stop(lua_State*L){(void)L;Moss_AudioMicrophoneStop();return 0;} static int l_audio_mic_id(lua_State*L){lua_pushinteger(L,Moss_AudioMicrophoneID());return 1;} static int l_audio_mic_select(lua_State*L){return push_bool(L,Moss_AudioSelectMicrophoneDevice((int)luaL_checkinteger(L,1)));} static int l_audio_mic_name(lua_State*L){return push_str(L,Moss_GetMicrophoneDeviceName((int)luaL_checkinteger(L,1)));} static int l_audio_mic_count(lua_State*L){lua_pushinteger(L,Moss_ListMicrophoneDevices());return 1;} static int l_audio_mic_gain(lua_State*L){Moss_AudioMicrophoneSetGain(check_ud<Moss_Microphone>(L,1,"moss.Microphone"),(float)luaL_checknumber(L,2));return 0;} static int l_audio_mic_sr(lua_State*L){lua_pushinteger(L,Moss_AudioMicrophoneGetSampleRate(check_ud<Moss_Microphone>(L,1,"moss.Microphone")));return 1;} static int l_audio_mic_channels(lua_State*L){lua_pushinteger(L,Moss_AudioMicrophoneGetChannels(check_ud<Moss_Microphone>(L,1,"moss.Microphone")));return 1;}

static void add_meta(lua_State*L,const char*name,const luaL_Reg*methods,lua_CFunction gc=nullptr){luaL_newmetatable(L,name);if(methods)luaL_setfuncs(L,methods,0);if(gc){lua_pushcfunction(L,gc);lua_setfield(L,-2,"__gc");}lua_pop(L,1);}

} // namespace

extern "C" int luaopen_moss(lua_State* L) {
    G=L;
    static const luaL_Reg window_methods[]={{"should_close",l_window_should_close},{"close",l_window_close},{"set_title",l_window_title},{nullptr,nullptr}};
    static const luaL_Reg gamepad_methods[]={{"connected",l_gamepad_connected},{"button_pressed",l_gamepad_pressed},{"button_just_pressed",l_gamepad_just_pressed},{"button_just_released",l_gamepad_just_released},{"axis",l_gamepad_axis},{"rumble",l_rumble},{"rumble_triggers",l_rumble_triggers},{"set_led",l_gamepad_led},{"name",l_gamepad_name},{"id",l_gamepad_id},{"player_index",l_gamepad_player},{"power_info",l_gamepad_power},{"mapping",l_gamepad_mapping},{"set_mapping",l_gamepad_set_mapping},{nullptr,nullptr}};
    static const luaL_Reg haptic_methods[]={{"create_effect",l_haptic_create_effect},{"effect_status",l_haptic_status},{"features",l_haptic_features},{"name",l_haptic_name},{"name_for_id",l_haptic_name_id},{"limits",l_haptic_limits},{"supported",l_haptic_supported},{"rumble_supported",l_haptic_rumble_supported},{"pause",l_pause_haptic},{"resume",l_resume_haptic},{"gain",l_haptic_gain},{"autocenter",l_haptic_autocenter},{"play_rumble",l_haptic_rumble},{"run_effect",l_run_haptic},{"stop_effect",l_stop_haptic_effect},{"stop_effects",l_stop_haptic_effects},{"stop_rumble",l_stop_haptic_rumble},{"destroy_effect",l_destroy_haptic_effect},{nullptr,nullptr}};
    static const luaL_Reg storage_methods[]={{"ready",l_storage_ready},{"create_directory",l_storage_create_dir},{"copy_file",l_storage_copy},{"remove",l_storage_remove},{"rename",l_storage_rename},{"file_size",l_storage_size},{"space_remaining",l_storage_space},{"path_info",l_storage_path_info},{"read",l_storage_read},{"write",l_storage_write},{nullptr,nullptr}};
    static const luaL_Reg audio_source_methods[]={{nullptr,nullptr}};
    static const luaL_Reg stream_methods[]={{"play",l_stream_play},{"stop",l_stream_stop},{"set_volume",l_stream_volume},{"set_pitch",l_stream_pitch},{"set_playback_rate",l_stream_rate},{"set_pan",l_stream_pan},{"set_loop",l_stream_loop},{nullptr,nullptr}};
    static const luaL_Reg stream2d_methods[]={{"play",l_stream2d_play},{"stop",l_stream2d_stop},{"set_volume",l_stream2d_volume},{"set_pitch",l_stream2d_pitch},{"set_playback_rate",l_stream2d_rate},{"set_pan",l_stream2d_pan},{"set_loop",l_stream2d_loop},{nullptr,nullptr}};
    static const luaL_Reg stream3d_methods[]={{"play",l_stream3d_play},{"stop",l_stream3d_stop},{"set_volume",l_stream3d_volume},{"set_pitch",l_stream3d_pitch},{"set_playback_rate",l_stream3d_rate},{"set_pan",l_stream3d_pan},{"set_loop",l_stream3d_loop},{"set_distance_model",l_stream3d_model},{nullptr,nullptr}};

    add_meta(L,"moss.Window",window_methods,l_window_gc);
    add_meta(L,"moss.Monitor",nullptr,nullptr);
    add_meta(L,"moss.Gamepad",gamepad_methods,l_gamepad_gc);
    add_meta(L,"moss.Haptic",haptic_methods,l_haptic_gc);
    add_meta(L,"moss.Storage",storage_methods,l_storage_gc);
    add_meta(L,"moss.AudioSource",audio_source_methods,l_audio_source_gc);
    add_meta(L,"moss.AudioStream",stream_methods,l_audio_stream_gc);
    add_meta(L,"moss.AudioStream2D",stream2d_methods,l_stream2d_gc);
    add_meta(L,"moss.AudioStream3D",stream3d_methods,l_stream3d_gc);
    add_meta(L,"moss.AudioListener2D",nullptr,l_listener2d_gc);
    add_meta(L,"moss.AudioListener3D",nullptr,l_listener3d_gc);
    add_meta(L,"moss.RayAudioListener2D",nullptr,l_raylistener2d_gc);
    add_meta(L,"moss.RayAudioListener3D",nullptr,l_raylistener3d_gc);
    add_meta(L,"moss.Microphone",nullptr,l_mic_gc);
    add_meta(L,"moss.RawPointer",nullptr,nullptr);

    lua_newtable(L);
#define FN(name,fn) lua_pushcfunction(L,fn);lua_setfield(L,-2,name)
    FN("degrees_to_radians",l_degrees_to_radians);FN("radians_to_degrees",l_radians_to_degrees);FN("center_angle_around_zero",l_center_angle);FN("count_trailing_zeros",l_ctz);FN("count_leading_zeros",l_clz);FN("count_bits",l_count_bits);FN("get_next_power_of_2",l_next_pow2);FN("get_ticks",l_get_ticks);FN("get_seconds",l_get_seconds);FN("get_milliseconds",l_get_ms);FN("get_milliseconds_and_reset",l_get_ms_reset);FN("yield",l_yield);FN("delay",l_delay);FN("local_time",l_local);FN("timestamp",l_stamp);FN("time_now",l_now);FN("ctime_now",l_ctime);FN("timestamp_et",l_et);FN("timestamp_ct",l_ct);FN("timestamp_mt",l_mt);FN("timestamp_pt",l_pt);FN("format_time",l_format_time);
    FN("Window",l_window_new);FN("create_message_box",l_create_message_box);FN("poll_events",l_poll);FN("window_size",l_window_width);FN("primary_monitor",l_monitor_primary);FN("secondary_monitor",l_monitor_secondary);FN("monitor_position",l_monitor_position);FN("monitor_physical_size",l_monitor_physical);FN("monitor_content_scale",l_monitor_scale);FN("monitor_name",l_monitor_name);FN("monitor_set_gamma",l_monitor_gamma);
    FN("is_key_pressed",l_key_pressed);FN("is_key_released",l_key_released);FN("is_key_just_pressed",l_key_just_pressed);FN("is_key_just_released",l_key_just_released);FN("input_get_key",l_input_key);FN("is_mouse_pressed",l_mouse_pressed);FN("is_mouse_released",l_mouse_released);FN("is_mouse_just_pressed",l_mouse_just_pressed);FN("is_mouse_just_released",l_mouse_just_released);FN("input_get_mouse_button",l_input_mouse);FN("mouse_position",l_mouse_position);FN("set_mouse_position",l_set_mouse_position);FN("set_mouse_visible",l_set_mouse_visible);
    FN("num_gamepads",l_num_gamepads);FN("Gamepad",l_gamepad_open);FN("update_gamepads",l_update_gamepads);FN("set_gamepad_axis_deadzone",l_set_axis_deadzone);FN("set_gamepad_axis_inverted",l_set_axis_inverted);FN("reload_gamepad_mappings",l_reload_mappings);
    FN("Haptic",l_open_haptic);FN("cpu_count",l_cpu_count);FN("cpu_cache_line_size",l_cpu_cache);FN("system_ram",l_ram);FN("open_url",l_open_url);FN("power_info",l_power_info);FN("is_process_running",l_process_running);FN("load_dynamic_library",l_load_library);FN("get_library_symbol",l_get_symbol);FN("unload_dynamic_library",l_unload_library);FN("path_info",l_path_info);FN("copy_file",l_copy_file);FN("create_directory",l_create_dir);FN("remove_path",l_remove_path);FN("rename_path",l_rename_path);FN("current_directory",l_current_dir);FN("base_path",l_base_path);FN("user_folder",l_user_folder);FN("pref_path",l_pref_path);FN("Storage",l_open_storage);
    FN("audio_init",l_audio_init);FN("audio_terminate",l_audio_term);FN("audio_update",l_audio_update);FN("audio_load_wav",l_audio_load_wav);FN("audio_load_wav_legacy",l_audio_load_wav_legacy);FN("audio_load_ogg",l_audio_load_ogg);FN("audio_load_mp3",l_audio_load_mp3);FN("audio_capture_microphone",l_audio_capture_mic);FN("audio_create_channel",l_audio_create_channel);FN("audio_remove_channel",l_audio_remove_channel);FN("audio_master_channel",l_audio_master_channel);FN("audio_set_channel_volume",l_audio_channel_volume);FN("audio_set_channel_mute",l_audio_channel_mute);
    FN("AudioStream",l_audio_stream_create);FN("AudioStream2D",l_stream2d_create);FN("AudioStream3D",l_stream3d_create);FN("AudioListener2D",l_listener2d_new);FN("AudioListener3D",l_listener3d_new);FN("RayAudioListener2D",l_raylistener2d_new);FN("RayAudioListener3D",l_raylistener3d_new);FN("listener2d_activate",l_listener2d_activate);FN("listener3d_activate",l_listener3d_activate);FN("ray_listener2d_activate",l_raylistener2d_activate);FN("ray_listener3d_activate",l_raylistener3d_activate);FN("listener3d_set_orientation",l_audio_listener_orientation);
    FN("speaker_ready",l_speaker_ready);FN("speaker_open",l_speaker_open);FN("speaker_pause",l_speaker_pause);FN("speaker_resume",l_speaker_resume);FN("speaker_is_paused",l_speaker_paused);FN("select_speaker_device",l_speaker_select);FN("current_speaker_device",l_speaker_current);FN("speaker_device_name",l_speaker_name);FN("list_speaker_devices",l_speaker_count);FN("microphone_device_count",l_mic_count);FN("microphone_device_name",l_mic_name);FN("Microphone",l_mic_new);FN("microphone_ready",l_mic_ready);FN("audio_microphone_open",l_audio_mic_open);FN("audio_microphone_close",l_audio_mic_close);FN("audio_microphone_play",l_audio_mic_play);FN("audio_microphone_stop",l_audio_mic_stop);FN("audio_microphone_id",l_audio_mic_id);FN("select_microphone_device",l_audio_mic_select);FN("microphone_name",l_audio_mic_name);FN("list_microphone_devices",l_audio_mic_count);FN("audio_microphone_set_gain",l_audio_mic_gain);
#undef FN
    return 1;


// Vec2 is separate because its indexed setter is named SetX/SetY in Moss.
int vec2_new(lua_State* L) { push(L,Vec2Name,Vec2(static_cast<float>(luaL_optnumber(L,1,0)),static_cast<float>(luaL_optnumber(L,2,0)))); return 1; }
int vec2_index(lua_State* L) { auto*v=check<Vec2>(L,1,Vec2Name); const char*k=luaL_checkstring(L,2); if(!std::strcmp(k,"x")){lua_pushnumber(L,v->GetX());return 1;} if(!std::strcmp(k,"y")){lua_pushnumber(L,v->GetY());return 1;} luaL_getmetatable(L,Vec2Name);lua_getfield(L,-1,k);return 1; }
int vec2_newindex(lua_State* L) { auto*v=check<Vec2>(L,1,Vec2Name); const char*k=luaL_checkstring(L,2);float n=static_cast<float>(luaL_checknumber(L,3));if(!std::strcmp(k,"x")){v->SetX(n);return 0;}if(!std::strcmp(k,"y")){v->SetY(n);return 0;}return luaL_error(L,"unknown field '%s'",k); }

VALUE_TYPE(Vec3, vec3, Vec3Name, 3)
VALUE_TYPE(Vec4, vec4, Vec4Name, 4)

int color_new(lua_State* L) { push(L,ColorName,Color(static_cast<float>(luaL_optnumber(L,1,0)),static_cast<float>(luaL_optnumber(L,2,0)),static_cast<float>(luaL_optnumber(L,3,0)),static_cast<float>(luaL_optnumber(L,4,1))));return 1; }
int color_index(lua_State* L) { auto*c=check<Color>(L,1,ColorName);const char*k=luaL_checkstring(L,2);if(!std::strcmp(k,"r")){lua_pushnumber(L,c->r);return 1;}if(!std::strcmp(k,"g")){lua_pushnumber(L,c->g);return 1;}if(!std::strcmp(k,"b")){lua_pushnumber(L,c->b);return 1;}if(!std::strcmp(k,"a")){lua_pushnumber(L,c->a);return 1;}luaL_getmetatable(L,ColorName);lua_getfield(L,-1,k);return 1; }
int color_newindex(lua_State* L) { auto*c=check<Color>(L,1,ColorName);const char*k=luaL_checkstring(L,2);float n=static_cast<float>(luaL_checknumber(L,3));if(!std::strcmp(k,"r"))c->r=n;else if(!std::strcmp(k,"g"))c->g=n;else if(!std::strcmp(k,"b"))c->b=n;else if(!std::strcmp(k,"a"))c->a=n;else return luaL_error(L,"unknown field '%s'",k);return 0; }
int rect_new(lua_State* L) { push(L,RectName,Rect(static_cast<float>(luaL_optnumber(L,1,0)),static_cast<float>(luaL_optnumber(L,2,0)),static_cast<float>(luaL_optnumber(L,3,0)),static_cast<float>(luaL_optnumber(L,4,0))));return 1; }
int rect_index(lua_State* L) { auto*r=check<Rect>(L,1,RectName);const char*k=luaL_checkstring(L,2);if(!std::strcmp(k,"x")){lua_pushnumber(L,r->x);return 1;}if(!std::strcmp(k,"y")){lua_pushnumber(L,r->y);return 1;}if(!std::strcmp(k,"width")){lua_pushnumber(L,r->width);return 1;}if(!std::strcmp(k,"height")){lua_pushnumber(L,r->height);return 1;}return luaL_error(L,"unknown field '%s'",k); }
int rect_newindex(lua_State* L) { auto*r=check<Rect>(L,1,RectName);const char*k=luaL_checkstring(L,2);float n=static_cast<float>(luaL_checknumber(L,3));if(!std::strcmp(k,"x"))r->x=n;else if(!std::strcmp(k,"y"))r->y=n;else if(!std::strcmp(k,"width"))r->width=n;else if(!std::strcmp(k,"height"))r->height=n;else return luaL_error(L,"unknown field '%s'",k);return 0; }

int vec2_add(lua_State*L){auto*a=check<Vec2>(L,1,Vec2Name);auto*b=check<Vec2>(L,2,Vec2Name);push(L,Vec2Name,*a+*b);return 1;}
int vec2_sub(lua_State*L){auto*a=check<Vec2>(L,1,Vec2Name);auto*b=check<Vec2>(L,2,Vec2Name);push(L,Vec2Name,*a-*b);return 1;}
int vec2_mul(lua_State*L){auto*a=check<Vec2>(L,1,Vec2Name);push(L,Vec2Name,*a*static_cast<float>(luaL_checknumber(L,2)));return 1;}
int vec3_add(lua_State*L){auto*a=check<Vec3>(L,1,Vec3Name);auto*b=check<Vec3>(L,2,Vec3Name);push(L,Vec3Name,*a+*b);return 1;}
int vec3_sub(lua_State*L){auto*a=check<Vec3>(L,1,Vec3Name);auto*b=check<Vec3>(L,2,Vec3Name);push(L,Vec3Name,*a-*b);return 1;}
int vec3_mul(lua_State*L){auto*a=check<Vec3>(L,1,Vec3Name);push(L,Vec3Name,*a*static_cast<float>(luaL_checknumber(L,2)));return 1;}
int vec3_dot(lua_State*L){auto*a=check<Vec3>(L,1,Vec3Name);auto*b=check<Vec3>(L,2,Vec3Name);lua_pushnumber(L,a->Dot(*b));return 1;}
int vec3_cross(lua_State*L){auto*a=check<Vec3>(L,1,Vec3Name);auto*b=check<Vec3>(L,2,Vec3Name);push(L,Vec3Name,a->Cross(*b));return 1;}
int vec3_length(lua_State*L){lua_pushnumber(L,check<Vec3>(L,1,Vec3Name)->Length());return 1;}
int vec3_normalized(lua_State*L){auto*v=check<Vec3>(L,1,Vec3Name);push(L,Vec3Name,v->Normalized());return 1;}
int quat_new(lua_State*L){push(L,QuatName,Quat(static_cast<float>(luaL_optnumber(L,1,0)),static_cast<float>(luaL_optnumber(L,2,0)),static_cast<float>(luaL_optnumber(L,3,0)),static_cast<float>(luaL_optnumber(L,4,1))));return 1;}
int quat_rotation(lua_State*L){auto*axis=check<Vec3>(L,1,Vec3Name);push(L,QuatName,Quat::sRotation(*axis,static_cast<float>(luaL_checknumber(L,2))));return 1;}
int quat_rotate(lua_State*L){auto*q=check<Quat>(L,1,QuatName);auto*v=check<Vec3>(L,2,Vec3Name);push(L,Vec3Name,*q**v);return 1;}
int quat_index(lua_State*L){auto*q=check<Quat>(L,1,QuatName);const char*k=luaL_checkstring(L,2);if(!std::strcmp(k,"x")){lua_pushnumber(L,q->GetX());return 1;}if(!std::strcmp(k,"y")){lua_pushnumber(L,q->GetY());return 1;}if(!std::strcmp(k,"z")){lua_pushnumber(L,q->GetZ());return 1;}if(!std::strcmp(k,"w")){lua_pushnumber(L,q->GetW());return 1;}luaL_getmetatable(L,QuatName);lua_getfield(L,-1,k);return 1;}
static int poll_events(lua_State*){Moss_PollEvents();return 0;}


void add_meta(lua_State*L,const char*n,const luaL_Reg*m,lua_CFunction index,lua_CFunction newindex,lua_CFunction destructor){luaL_newmetatable(L,n);if(m)luaL_setfuncs(L,m,0);if(index){lua_pushcfunction(L,index);lua_setfield(L,-2,"__index");}if(newindex){lua_pushcfunction(L,newindex);lua_setfield(L,-2,"__newindex");}lua_pushcfunction(L,destructor);lua_setfield(L,-2,"__gc");lua_pop(L,1);}
}

extern "C" int luaopen_moss(lua_State*L){
  static const luaL_Reg v2[]={{"__add",vec2_add},{"__sub",vec2_sub},{"__mul",vec2_mul},{nullptr,nullptr}};
  static const luaL_Reg v3[]={{"dot",vec3_dot},{"cross",vec3_cross},{"length",vec3_length},{"normalized",vec3_normalized},{"__add",vec3_add},{"__sub",vec3_sub},{"__mul",vec3_mul},{nullptr,nullptr}};
  static const luaL_Reg quat[]={{"rotate",quat_rotate},{nullptr,nullptr}};
  static const luaL_Reg win[]={{"close",window_close},{"should_close",window_should_close},{"set_title",window_title},{"create_renderer",renderer_new},{nullptr,nullptr}};
  static const luaL_Reg renderer[]={{"begin_frame",renderer_begin},{"end_frame",renderer_end},{"line2d",renderer_line2d},{"circle2d",renderer_circle2d},{nullptr,nullptr}};
  add_meta(L,Vec2Name,v2,vec2_index,vec2_newindex,[](lua_State*l){return destroy<Vec2>(l,Vec2Name);});add_meta(L,Vec3Name,v3,vec3_index,vec3_newindex,[](lua_State*l){return destroy<Vec3>(l,Vec3Name);});add_meta(L,Vec4Name,nullptr,vec4_index,vec4_newindex,[](lua_State*l){return destroy<Vec4>(l,Vec4Name);});add_meta(L,ColorName,nullptr,color_index,color_newindex,[](lua_State*l){return destroy<Color>(l,ColorName);});add_meta(L,QuatName,quat,quat_index,nullptr,[](lua_State*l){return destroy<Quat>(l,QuatName);});add_meta(L,RectName,nullptr,rect_index,rect_newindex,[](lua_State*l){return destroy<Rect>(l,RectName);});add_meta(L,WindowName,win,nullptr,nullptr,window_gc);add_meta(L,RendererName,renderer,nullptr,nullptr,renderer_gc);
  lua_newtable(L);lua_pushcfunction(L,vec2_new);lua_setfield(L,-2,"Vec2");lua_pushcfunction(L,vec3_new);lua_setfield(L,-2,"Vec3");lua_pushcfunction(L,vec4_new);lua_setfield(L,-2,"Vec4");lua_pushcfunction(L,color_new);lua_setfield(L,-2,"Color");lua_pushcfunction(L,quat_new);lua_setfield(L,-2,"Quat");lua_pushcfunction(L,quat_rotation);lua_setfield(L,-2,"rotation");lua_pushcfunction(L,rect_new);lua_setfield(L,-2,"Rect");lua_pushcfunction(L,window_new);lua_setfield(L,-2,"Window");lua_pushcfunction(L,poll_events);lua_setfield(L,-2,"poll_events");lua_pushcfunction(L,window_size);lua_setfield(L,-2,"window_size");lua_pushcfunction(L,mouse_position);lua_setfield(L,-2,"mouse_position");lua_pushcfunction(L,key_pressed);lua_setfield(L,-2,"key_pressed");return 1;
}
