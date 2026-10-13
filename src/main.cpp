// LuaMoss - combined native Lua 5.4 bindings for the Moss Framework.
//
// This file consolidates the original LuaMoss core API and the extended
// moss.extra bindings into a single translation unit. It uses C++17 and Lua 5.4.
// Native Moss resources must be explicitly closed/destroyed by the caller.

#include <lua.hpp>

#include <Moss/Moss_stdinc.h>
#include <Moss/Moss_Platform.h>
#include <Moss/Moss_GPU.h>
#include <Moss/Moss_Renderer.h>
#include <Moss/Moss_Audio.h>
#include <Moss/Moss_Physics.h>
#include <Moss/Moss_XR.h>
#include <Moss/Variants/Color.h>
#include <Moss/Variants/Quat.h>
#include <Moss/Variants/Rect.h>
#include <Moss/Variants/Vector/Vec2.h>
#include <Moss/Variants/Vector/Vec3.h>
#include <Moss/Variants/Vector/Vec4.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <vector>

namespace luamoss_extra {

constexpr const char* kHandleMeta = "moss.extra.Handle";

struct HandleBox {
    void* value;
    const void* type_tag;
};

template <class T>
inline unsigned char type_tag_storage = 0;

template <class T>
inline const void* type_tag() noexcept { return &type_tag_storage<T>; }

template <class T>
inline void push_handle(lua_State* L, T* value) {
    if (!value) {
        lua_pushnil(L);
        return;
    }
    auto* box = new (lua_newuserdatauv(L, sizeof(HandleBox), 0)) HandleBox{};
    box->value = static_cast<void*>(value);
    box->type_tag = type_tag<T>();
    luaL_getmetatable(L, kHandleMeta);
    lua_setmetatable(L, -2);
}

inline HandleBox* test_handle_box(lua_State* L, int index) {
    return static_cast<HandleBox*>(luaL_testudata(L, index, kHandleMeta));
}

template <class T>
inline T* try_get_ptr(lua_State* L, int index, const char* legacy_metatable = nullptr) {
    if (HandleBox* box = test_handle_box(L, index)) {
        if (box->type_tag != type_tag<T>() || !box->value) return nullptr;
        return static_cast<T*>(box->value);
    }
    if (legacy_metatable) {
        if (void* userdata = luaL_testudata(L, index, legacy_metatable)) {
            // Existing LuaMoss wrappers store the native pointer as their first
            // member (Window::value / Renderer::value). This adapter is used only
            // for these two known wrappers during integration.
            return *static_cast<T**>(userdata);
        }
    }
    if (lua_islightuserdata(L, index)) return static_cast<T*>(lua_touserdata(L, index));
    return nullptr;
}

template <class T>
inline T* check_ptr(lua_State* L, int index, const char* label,
                    const char* legacy_metatable = nullptr) {
    T* value = try_get_ptr<T>(L, index, legacy_metatable);
    if (!value) luaL_error(L, "%s: expected a valid Moss handle", label);
    return value;
}

inline void invalidate_handle(lua_State* L, int index) {
    if (HandleBox* box = test_handle_box(L, index)) box->value = nullptr;
}
inline void invalidate_window_handle(lua_State* L, int index) {
    if (HandleBox* box = test_handle_box(L, index)) {
        if (box->type_tag == type_tag<Moss_Window>()) box->value = nullptr;
        return;
    }
    if (void* userdata = luaL_testudata(L, index, "moss.Window")) {
        *static_cast<Moss_Window**>(userdata) = nullptr;
    }
}

inline int handle_gc(lua_State* L) {
    if (auto* box = test_handle_box(L, 1)) box->~HandleBox();
    return 0;
}

inline void set_function(lua_State* L, const char* name, lua_CFunction fn) {
    lua_pushcfunction(L, fn);
    lua_setfield(L, -2, name);
}

inline float table_number(lua_State* L, int index, const char* key, int array_index,
                          float fallback) {
    index = lua_absindex(L, index);
    if (!lua_istable(L, index)) return fallback;
    lua_getfield(L, index, key);
    if (lua_isnumber(L, -1)) {
        const float value = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
        return value;
    }
    lua_pop(L, 1);
    lua_rawgeti(L, index, array_index);
    const float value = lua_isnumber(L, -1)
        ? static_cast<float>(lua_tonumber(L, -1)) : fallback;
    lua_pop(L, 1);
    return value;
}

inline Vec2 read_vec2(lua_State* L, int index, float fallback_x = 0.0f,
                      float fallback_y = 0.0f, bool optional = false) {
    if (auto* value = static_cast<Vec2*>(luaL_testudata(L, index, "moss.Vec2")))
        return *value;
    if (lua_istable(L, index)) {
        return Vec2(table_number(L, index, "x", 1, fallback_x),
                    table_number(L, index, "y", 2, fallback_y));
    }
    if (optional && (lua_isnoneornil(L, index))) return Vec2(fallback_x, fallback_y);
    luaL_error(L, "argument %d must be a moss.Vec2 or a {x,y} / {x,y} table", index);
    return Vec2(fallback_x, fallback_y);
}

inline Vec3 read_vec3(lua_State* L, int index, float x = 0.0f, float y = 0.0f,
                      float z = 0.0f, bool optional = false) {
    if (auto* value = static_cast<Vec3*>(luaL_testudata(L, index, "moss.Vec3")))
        return *value;
    if (lua_istable(L, index)) {
        return Vec3(table_number(L, index, "x", 1, x),
                    table_number(L, index, "y", 2, y),
                    table_number(L, index, "z", 3, z));
    }
    if (optional && lua_isnoneornil(L, index)) return Vec3(x, y, z);
    luaL_error(L, "argument %d must be a moss.Vec3 or a {x,y,z} table", index);
    return Vec3(x, y, z);
}

inline Color read_color(lua_State* L, int index, float r = 1.0f, float g = 1.0f,
                        float b = 1.0f, float a = 1.0f, bool optional = false) {
    if (auto* value = static_cast<Color*>(luaL_testudata(L, index, "moss.Color")))
        return *value;
    if (lua_istable(L, index)) {
        return Color(table_number(L, index, "r", 1, r),
                     table_number(L, index, "g", 2, g),
                     table_number(L, index, "b", 3, b),
                     table_number(L, index, "a", 4, a));
    }
    if (optional && lua_isnoneornil(L, index)) return Color(r, g, b, a);
    luaL_error(L, "argument %d must be a moss.Color or an {r,g,b,a} table", index);
    return Color(r, g, b, a);
}

inline Float2 read_float2(lua_State* L, int index, float x = 0.0f, float y = 0.0f,
                          bool optional = false) {
    const Vec2 value = read_vec2(L, index, x, y, optional);
    return Float2(value.GetX(), value.GetY());
}

inline Float3 read_float3(lua_State* L, int index, float x = 0.0f, float y = 0.0f,
                          float z = 0.0f, bool optional = false) {
    const Vec3 value = read_vec3(L, index, x, y, z, optional);
    return Float3(value.GetX(), value.GetY(), value.GetZ());
}

inline bool read_matrix16(lua_State* L, int index, float out[16]) {
    if (!lua_istable(L, index) || lua_rawlen(L, index) != 16) return false;
    for (int i = 0; i < 16; ++i) {
        lua_rawgeti(L, index, i + 1);
        if (!lua_isnumber(L, -1)) { lua_pop(L, 1); return false; }
        out[i] = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
    }
    return true;
}

inline void push_vec2(lua_State* L, const Vec2& v) {
    lua_createtable(L, 2, 2);
    lua_pushnumber(L, v.GetX()); lua_rawseti(L, -2, 1);
    lua_pushnumber(L, v.GetY()); lua_rawseti(L, -2, 2);
    lua_pushnumber(L, v.GetX()); lua_setfield(L, -2, "x");
    lua_pushnumber(L, v.GetY()); lua_setfield(L, -2, "y");
}
inline void push_vec3(lua_State* L, const Vec3& v) {
    lua_createtable(L, 3, 3);
    lua_pushnumber(L, v.GetX()); lua_rawseti(L, -2, 1);
    lua_pushnumber(L, v.GetY()); lua_rawseti(L, -2, 2);
    lua_pushnumber(L, v.GetZ()); lua_rawseti(L, -2, 3);
    lua_pushnumber(L, v.GetX()); lua_setfield(L, -2, "x");
    lua_pushnumber(L, v.GetY()); lua_setfield(L, -2, "y");
    lua_pushnumber(L, v.GetZ()); lua_setfield(L, -2, "z");
}
inline void push_pose(lua_State* L, const MossXR_Pose& pose) {
    lua_createtable(L, 0, 2);
    push_vec3(L, pose.position); lua_setfield(L, -2, "position");
    lua_createtable(L, 4, 4);
    lua_pushnumber(L, pose.orientation.GetX()); lua_rawseti(L, -2, 1);
    lua_pushnumber(L, pose.orientation.GetY()); lua_rawseti(L, -2, 2);
    lua_pushnumber(L, pose.orientation.GetZ()); lua_rawseti(L, -2, 3);
    lua_pushnumber(L, pose.orientation.GetW()); lua_rawseti(L, -2, 4);
    lua_pushnumber(L, pose.orientation.GetX()); lua_setfield(L, -2, "x");
    lua_pushnumber(L, pose.orientation.GetY()); lua_setfield(L, -2, "y");
    lua_pushnumber(L, pose.orientation.GetZ()); lua_setfield(L, -2, "z");
    lua_pushnumber(L, pose.orientation.GetW()); lua_setfield(L, -2, "w");
    lua_setfield(L, -2, "orientation_xyzw");
}

// -----------------------------------------------------------------------------
// std / time
// -----------------------------------------------------------------------------
inline int std_degrees_to_radians(lua_State* L) { lua_pushnumber(L, DegreesToRadians(static_cast<float>(luaL_checknumber(L, 1)))); return 1; }
inline int std_radians_to_degrees(lua_State* L) { lua_pushnumber(L, RadiansToDegrees(static_cast<float>(luaL_checknumber(L, 1)))); return 1; }
inline int std_center_angle(lua_State* L) { lua_pushnumber(L, CenterAngleAroundZero(static_cast<float>(luaL_checknumber(L, 1)))); return 1; }
inline int std_clamp(lua_State* L) { lua_pushnumber(L, Clamp(static_cast<float>(luaL_checknumber(L, 1)), static_cast<float>(luaL_checknumber(L, 2)), static_cast<float>(luaL_checknumber(L, 3)))); return 1; }
inline int std_lerp(lua_State* L) { lua_pushnumber(L, Lerp(static_cast<float>(luaL_checknumber(L, 1)), static_cast<float>(luaL_checknumber(L, 2)), static_cast<float>(luaL_checknumber(L, 3)))); return 1; }
inline int std_get_ticks(lua_State* L) { lua_pushinteger(L, static_cast<lua_Integer>(Moss_GetTicks())); return 1; }
inline int std_get_seconds(lua_State* L) { lua_pushnumber(L, Moss_GetSeconds(static_cast<Moss_Time>(luaL_checkinteger(L, 1)))); return 1; }
inline int std_get_milliseconds(lua_State* L) { lua_pushinteger(L, static_cast<lua_Integer>(Moss_GetMilliseconds(static_cast<Moss_Time>(luaL_checkinteger(L, 1))))); return 1; }
inline int std_delay(lua_State* L) { Moss_Delay(static_cast<uint32_t>(luaL_checkinteger(L, 1))); return 0; }
inline int std_yield(lua_State*) { Moss_Yield(); return 0; }
inline int std_local_time(lua_State* L) { const char* s = Moss_LocalTime(); lua_pushstring(L, s ? s : ""); return 1; }
inline int std_timestamp(lua_State* L) { const char* s = Moss_TimeStamp(); lua_pushstring(L, s ? s : ""); return 1; }
inline int std_time_now(lua_State* L) { const char* s = Moss_TimeNow(); lua_pushstring(L, s ? s : ""); return 1; }
inline int std_format_time(lua_State* L) { const char* fmt = luaL_checkstring(L, 1); const char* s = Moss_FormatTime(fmt); lua_pushstring(L, s ? s : ""); return 1; }

// -----------------------------------------------------------------------------
// Platform, windowing, input, gamepads, system and filesystem/storage.
// The wrappers use the Moss public API and typed userdata, rather than exposing
// raw C++ pointer values as Lua integers.
// -----------------------------------------------------------------------------
inline void push_path_info(lua_State* L, const Moss_PathInfo& info) {
    lua_createtable(L, 0, 8);
    lua_pushinteger(L, static_cast<lua_Integer>(info.type)); lua_setfield(L, -2, "type");
    lua_pushinteger(L, static_cast<lua_Integer>(info.size)); lua_setfield(L, -2, "size");
    lua_pushinteger(L, static_cast<lua_Integer>(info.create_time)); lua_setfield(L, -2, "create_time");
    lua_pushinteger(L, static_cast<lua_Integer>(info.modify_time)); lua_setfield(L, -2, "modify_time");
    lua_pushinteger(L, static_cast<lua_Integer>(info.access_time)); lua_setfield(L, -2, "access_time");
    lua_pushboolean(L, info.readable); lua_setfield(L, -2, "readable");
    lua_pushboolean(L, info.writable); lua_setfield(L, -2, "writable");
    lua_pushboolean(L, info.executable); lua_setfield(L, -2, "executable");
}
inline int platform_create_window(lua_State* L) {
    const char* title = luaL_checkstring(L, 1);
    const int width = static_cast<int>(luaL_checkinteger(L, 2));
    const int height = static_cast<int>(luaL_checkinteger(L, 3));
    Moss_Monitor* monitor = lua_isnoneornil(L, 4) ? nullptr : check_ptr<Moss_Monitor>(L, 4, "platform.create_window(monitor)", "moss.Monitor");
    Moss_Window* share = lua_isnoneornil(L, 5) ? nullptr : check_ptr<Moss_Window>(L, 5, "platform.create_window(share)", "moss.Window");
    push_handle(L, Moss_CreateWindow(title, width, height, monitor, share)); return 1;
}
inline int platform_terminate_window(lua_State* L) { Moss_TerminateWindow(check_ptr<Moss_Window>(L, 1, "platform.terminate_window(window)", "moss.Window")); invalidate_window_handle(L, 1); return 0; }
inline int platform_poll_events(lua_State*) { Moss_PollEvents(); return 0; }
inline int platform_should_window_close(lua_State* L) { lua_pushboolean(L, Moss_ShouldWindowClose(check_ptr<Moss_Window>(L, 1, "platform.should_window_close(window)", "moss.Window"))); return 1; }
inline int platform_close_window(lua_State* L) { Moss_CloseWindow(check_ptr<Moss_Window>(L, 1, "platform.close_window(window)", "moss.Window")); return 0; }
inline int platform_set_window_title(lua_State* L) { Moss_SetWindowTitle(check_ptr<Moss_Window>(L, 1, "platform.set_window_title(window)", "moss.Window"), luaL_checkstring(L, 2)); return 0; }
inline int platform_window_width(lua_State* L) { (void)L; lua_pushinteger(L, Moss_GetWindowWidth()); return 1; }
inline int platform_window_height(lua_State* L) { (void)L; lua_pushinteger(L, Moss_GetWindowHeight()); return 1; }
inline int platform_message_box(lua_State* L) {
    const char* title = luaL_checkstring(L, 1);
    const char* message = luaL_checkstring(L, 2);
    const auto flags = static_cast<Moss_MessageBoxFlags>(luaL_checkinteger(L, 3));
    Moss_Window* window = lua_isnoneornil(L, 4) ? nullptr : check_ptr<Moss_Window>(L, 4, "platform.message_box(window)", "moss.Window");
    lua_pushinteger(L, static_cast<lua_Integer>(Moss_CreateMessageBox(title, message, flags, window))); return 1;
}
inline int platform_primary_monitor(lua_State* L) { push_handle(L, Moss_MonitorGetPrimary()); return 1; }
inline int platform_secondary_monitor(lua_State* L) { push_handle(L, Moss_MonitorGetSecondary()); return 1; }
inline int platform_monitor_position(lua_State* L) { int x = 0, y = 0; Moss_MonitorGetPosition(check_ptr<Moss_Monitor>(L, 1, "platform.monitor_position(monitor)", "moss.Monitor"), &x, &y); lua_pushinteger(L, x); lua_pushinteger(L, y); return 2; }
inline int platform_monitor_physical_size(lua_State* L) { int x = 0, y = 0; Moss_MonitorGetPhysicalSize(check_ptr<Moss_Monitor>(L, 1, "platform.monitor_physical_size(monitor)", "moss.Monitor"), &x, &y); lua_pushinteger(L, x); lua_pushinteger(L, y); return 2; }
inline int platform_monitor_content_scale(lua_State* L) { float x = 1, y = 1; Moss_MonitorGetContentScale(check_ptr<Moss_Monitor>(L, 1, "platform.monitor_content_scale(monitor)", "moss.Monitor"), &x, &y); lua_pushnumber(L, x); lua_pushnumber(L, y); return 2; }
inline int platform_monitor_name(lua_State* L) { const char* s = Moss_MonitorGetName(check_ptr<Moss_Monitor>(L, 1, "platform.monitor_name(monitor)", "moss.Monitor")); lua_pushstring(L, s ? s : ""); return 1; }
inline int platform_monitor_set_gamma(lua_State* L) { Moss_MonitorSetGamma(check_ptr<Moss_Monitor>(L, 1, "platform.monitor_set_gamma(monitor)", "moss.Monitor"), static_cast<float>(luaL_checknumber(L, 2))); return 0; }
inline int input_key_pressed(lua_State* L) { lua_pushboolean(L, Moss_IsKeyPressed(static_cast<Keyboard>(luaL_checkinteger(L, 1)))); return 1; }
inline int input_key_released(lua_State* L) { lua_pushboolean(L, Moss_IsReleased(static_cast<Keyboard>(luaL_checkinteger(L, 1)))); return 1; }
inline int input_key_just_pressed(lua_State* L) { lua_pushboolean(L, Moss_IsKeyJustPressed(static_cast<Keyboard>(luaL_checkinteger(L, 1)))); return 1; }
inline int input_key_just_released(lua_State* L) { lua_pushboolean(L, Moss_IsKeyJustReleased(static_cast<Keyboard>(luaL_checkinteger(L, 1)))); return 1; }
inline int input_get_key(lua_State* L) { lua_pushinteger(L, static_cast<lua_Integer>(Moss_InputGetKey(static_cast<Keyboard>(luaL_checkinteger(L, 1))))); return 1; }
inline int input_mouse_pressed(lua_State* L) { lua_pushboolean(L, Moss_IsMousePressed(static_cast<Mouse>(luaL_checkinteger(L, 1)))); return 1; }
inline int input_mouse_released(lua_State* L) { lua_pushboolean(L, Moss_IsMouseReleased(static_cast<Mouse>(luaL_checkinteger(L, 1)))); return 1; }
inline int input_mouse_just_pressed(lua_State* L) { lua_pushboolean(L, Moss_IsMouseJustPressed(static_cast<Mouse>(luaL_checkinteger(L, 1)))); return 1; }
inline int input_mouse_just_released(lua_State* L) { lua_pushboolean(L, Moss_IsMouseJustReleased(static_cast<Mouse>(luaL_checkinteger(L, 1)))); return 1; }
inline int input_mouse_position(lua_State* L) { int x = 0, y = 0; Moss_GetMousePosition(&x, &y); lua_pushinteger(L, x); lua_pushinteger(L, y); return 2; }
inline int input_set_mouse_position(lua_State* L) { Moss_SetMousePosition(static_cast<int>(luaL_checkinteger(L, 1)), static_cast<int>(luaL_checkinteger(L, 2))); return 0; }
inline int input_set_mouse_visible(lua_State* L) { Moss_SetMouseVisible(lua_toboolean(L, 1) != 0); return 0; }
inline int gamepad_count(lua_State* L) { lua_pushinteger(L, Moss_GetNumGamepads()); return 1; }
inline int gamepad_open(lua_State* L) { push_handle(L, Moss_OpenGamepad(static_cast<Moss_GamepadID>(luaL_checkinteger(L, 1)))); return 1; }
inline int gamepad_close(lua_State* L) { Moss_CloseGamepad(check_ptr<Moss_Gamepad>(L, 1, "gamepad.close(gamepad)")); invalidate_handle(L, 1); return 0; }
inline int gamepad_connected(lua_State* L) { lua_pushboolean(L, Moss_GamepadConnected(check_ptr<Moss_Gamepad>(L, 1, "gamepad.connected(gamepad)"))); return 1; }
inline int gamepad_update_all(lua_State*) { Moss_UpdateGamepads(); return 0; }
inline int gamepad_button_pressed(lua_State* L) { lua_pushboolean(L, Moss_IsGamepadButtonPressed(check_ptr<Moss_Gamepad>(L, 1, "gamepad.button_pressed(gamepad)"), static_cast<Moss_GamepadButton>(luaL_checkinteger(L, 2)))); return 1; }
inline int gamepad_button_just_pressed(lua_State* L) { lua_pushboolean(L, Moss_IsGamepadButtonJustPressed(check_ptr<Moss_Gamepad>(L, 1, "gamepad.button_just_pressed(gamepad)"), static_cast<Moss_GamepadButton>(luaL_checkinteger(L, 2)))); return 1; }
inline int gamepad_button_just_released(lua_State* L) { lua_pushboolean(L, Moss_IsGamepadButtonJustReleased(check_ptr<Moss_Gamepad>(L, 1, "gamepad.button_just_released(gamepad)"), static_cast<Moss_GamepadButton>(luaL_checkinteger(L, 2)))); return 1; }
inline int gamepad_axis(lua_State* L) { lua_pushnumber(L, Moss_GetGamepadAxis(check_ptr<Moss_Gamepad>(L, 1, "gamepad.axis(gamepad)"), static_cast<GamepadAxis>(luaL_checkinteger(L, 2)))); return 1; }
inline int gamepad_name(lua_State* L) { const char* s = Moss_GetGamepadName(check_ptr<Moss_Gamepad>(L, 1, "gamepad.name(gamepad)")); lua_pushstring(L, s ? s : ""); return 1; }
inline int gamepad_id(lua_State* L) { lua_pushinteger(L, static_cast<lua_Integer>(Moss_GetGamepadID(check_ptr<Moss_Gamepad>(L, 1, "gamepad.id(gamepad)")))); return 1; }
inline int gamepad_player_index(lua_State* L) { lua_pushinteger(L, Moss_GetGamepadPlayerIndex(check_ptr<Moss_Gamepad>(L, 1, "gamepad.player_index(gamepad)"))); return 1; }
inline int gamepad_rumble(lua_State* L) { lua_pushboolean(L, Moss_RumbleGamepad(check_ptr<Moss_Gamepad>(L, 1, "gamepad.rumble(gamepad)"), static_cast<uint16_t>(luaL_checkinteger(L, 2)), static_cast<uint16_t>(luaL_checkinteger(L, 3)), static_cast<uint32_t>(luaL_checkinteger(L, 4)))); return 1; }
inline int gamepad_set_led(lua_State* L) { lua_pushboolean(L, Moss_SetGamepadLED(check_ptr<Moss_Gamepad>(L, 1, "gamepad.set_led(gamepad)"), static_cast<uint8_t>(luaL_checkinteger(L, 2)), static_cast<uint8_t>(luaL_checkinteger(L, 3)), static_cast<uint8_t>(luaL_checkinteger(L, 4)))); return 1; }
inline int system_cpu_count(lua_State* L) { lua_pushinteger(L, Moss_GetAvailableCPUCores()); return 1; }
inline int system_cpu_cache_line_size(lua_State* L) { lua_pushinteger(L, Moss_GetCPUCacheLineSize()); return 1; }
inline int system_ram(lua_State* L) { lua_pushinteger(L, Moss_GetSystemRAM()); return 1; }
inline int system_open_url(lua_State* L) { lua_pushboolean(L, Moss_OpenURL(luaL_checkstring(L, 1))); return 1; }
inline int system_is_process_running(lua_State* L) { lua_pushboolean(L, Moss_IsProcessRunningByName(luaL_checkstring(L, 1))); return 1; }
inline int filesystem_copy_file(lua_State* L) { lua_pushboolean(L, Moss_CopyFile(luaL_checkstring(L, 1), luaL_checkstring(L, 2), lua_toboolean(L, 3) != 0)); return 1; }
inline int filesystem_create_directory(lua_State* L) { lua_pushboolean(L, Moss_CreateDirectory(luaL_checkstring(L, 1), lua_toboolean(L, 2) != 0)); return 1; }
inline int filesystem_remove_path(lua_State* L) { lua_pushboolean(L, Moss_RemovePath(luaL_checkstring(L, 1), lua_toboolean(L, 2) != 0)); return 1; }
inline int filesystem_rename_path(lua_State* L) { lua_pushboolean(L, Moss_RenamePath(luaL_checkstring(L, 1), luaL_checkstring(L, 2), lua_toboolean(L, 3) != 0)); return 1; }
inline int filesystem_path_info(lua_State* L) { Moss_PathInfo info{}; if (!Moss_GetPathInfo(luaL_checkstring(L, 1), &info)) { lua_pushnil(L); return 1; } push_path_info(L, info); return 1; }
inline int filesystem_current_directory(lua_State* L) { char buffer[4096]{}; lua_pushstring(L, Moss_GetCurrentDirectory(buffer, sizeof(buffer)) ? buffer : ""); return 1; }
inline int filesystem_base_path(lua_State* L) { char buffer[4096]{}; lua_pushstring(L, Moss_GetBasePath(buffer, sizeof(buffer)) ? buffer : ""); return 1; }
inline int filesystem_user_folder(lua_State* L) { char buffer[4096]{}; const auto folder = static_cast<Moss_UserFolder>(luaL_checkinteger(L, 1)); lua_pushstring(L, Moss_GetUserFolder(folder, buffer, sizeof(buffer)) ? buffer : ""); return 1; }
inline int filesystem_pref_path(lua_State* L) { char buffer[4096]{}; lua_pushstring(L, Moss_GetPrefPath(luaL_checkstring(L, 1), luaL_checkstring(L, 2), buffer, sizeof(buffer)) ? buffer : ""); return 1; }
inline int storage_open_file(lua_State* L) { push_handle(L, Moss_OpenFileStorage(luaL_checkstring(L, 1))); return 1; }
inline int storage_open_title(lua_State* L) { push_handle(L, Moss_OpenTitleStorage(luaL_checkstring(L, 1), static_cast<uint32_t>(luaL_optinteger(L, 2, 0)))); return 1; }
inline int storage_open_user(lua_State* L) { push_handle(L, Moss_OpenUserStorage(luaL_checkstring(L, 1), luaL_checkstring(L, 2), static_cast<uint32_t>(luaL_optinteger(L, 3, 0)))); return 1; }
inline int storage_ready(lua_State* L) { lua_pushboolean(L, Moss_StorageReady(check_ptr<Moss_Storage>(L, 1, "storage.ready(storage)"))); return 1; }
inline int storage_close(lua_State* L) { const bool result = Moss_CloseStorage(check_ptr<Moss_Storage>(L, 1, "storage.close(storage)")); invalidate_handle(L, 1); lua_pushboolean(L, result); return 1; }
inline int storage_create_directory(lua_State* L) { lua_pushboolean(L, Moss_CreateStorageDirectory(check_ptr<Moss_Storage>(L, 1, "storage.create_directory(storage)"), luaL_checkstring(L, 2))); return 1; }
inline int storage_copy_file(lua_State* L) { lua_pushboolean(L, Moss_CopyStorageFile(check_ptr<Moss_Storage>(L, 1, "storage.copy_file(storage)"), luaL_checkstring(L, 2), luaL_checkstring(L, 3))); return 1; }
inline int storage_remove_path(lua_State* L) { lua_pushboolean(L, Moss_RemoveStoragePath(check_ptr<Moss_Storage>(L, 1, "storage.remove_path(storage)"), luaL_checkstring(L, 2))); return 1; }
inline int storage_rename_path(lua_State* L) { lua_pushboolean(L, Moss_RenameStoragePath(check_ptr<Moss_Storage>(L, 1, "storage.rename_path(storage)"), luaL_checkstring(L, 2), luaL_checkstring(L, 3))); return 1; }
inline int storage_space_remaining(lua_State* L) { lua_pushinteger(L, static_cast<lua_Integer>(Moss_GetStorageSpaceRemaining(check_ptr<Moss_Storage>(L, 1, "storage.space_remaining(storage)")))); return 1; }
inline int storage_path_info(lua_State* L) { Moss_PathInfo info{}; if (!Moss_GetStoragePathInfo(check_ptr<Moss_Storage>(L, 1, "storage.path_info(storage)"), luaL_checkstring(L, 2), &info)) { lua_pushnil(L); return 1; } push_path_info(L, info); return 1; }
inline int storage_read_file(lua_State* L) {
    auto* storage = check_ptr<Moss_Storage>(L, 1, "storage.read_file(storage)");
    const char* path = luaL_checkstring(L, 2);
    const lua_Integer requested = luaL_checkinteger(L, 3);
    if (requested < 0 || static_cast<unsigned long long>(requested) > 64ull * 1024ull * 1024ull) return luaL_error(L, "length must be between 0 and 64 MiB");
    std::vector<char> bytes(static_cast<size_t>(requested));
    if (!Moss_ReadStorageFile(storage, path, bytes.data(), static_cast<uint64_t>(bytes.size()))) { lua_pushnil(L); return 1; }
    lua_pushlstring(L, bytes.empty() ? "" : bytes.data(), bytes.size()); return 1;
}
inline int storage_write_file(lua_State* L) { size_t size = 0; const char* bytes = luaL_checklstring(L, 3, &size); lua_pushboolean(L, Moss_WriteStorageFile(check_ptr<Moss_Storage>(L, 1, "storage.write_file(storage)"), luaL_checkstring(L, 2), bytes, static_cast<uint64_t>(size))); return 1; }


// -----------------------------------------------------------------------------
// Renderer / assets / 2D debug drawing
// -----------------------------------------------------------------------------
inline int renderer_create(lua_State* L) {
    Moss_Window* window = check_ptr<Moss_Window>(L, 1, "renderer.create(window)", "moss.Window");
    push_handle(L, Moss_CreateRenderer(window));
    return 1;
}
inline int renderer_destroy(lua_State* L) {
    auto* renderer = check_ptr<Moss_Renderer>(L, 1, "renderer.destroy(renderer)");
    Moss_RendererDestroy(renderer); invalidate_handle(L, 1); return 0;
}
inline int renderer_terminate(lua_State* L) { Moss_TerminateRenderer(check_ptr<Moss_Renderer>(L, 1, "renderer.terminate(renderer)")); return 0; }
inline int renderer_begin_frame(lua_State* L) { Moss_RendererBeginFrame(check_ptr<Moss_Renderer>(L, 1, "renderer.begin_frame(renderer)")); return 0; }
inline int renderer_begin_frame_ex(lua_State* L) {
    auto* renderer = check_ptr<Moss_Renderer>(L, 1, "renderer.begin_frame_ex(renderer)");
    Moss_RenderFrameDesc desc{};
    desc.delta_time = static_cast<float>(luaL_optnumber(L, 2, 0.0));
    desc.clear_color = read_color(L, 3, 0.0f, 0.0f, 0.0f, 1.0f, true);
    desc.world_scale = static_cast<float>(luaL_optnumber(L, 4, 1.0));
    desc.camera = lua_isnoneornil(L, 5) ? nullptr : check_ptr<Moss_Camera>(L, 5, "camera");
    Moss_RendererBeginFrameEx(renderer, &desc); return 0;
}
inline int renderer_end_frame(lua_State* L) { Moss_RendererEndFrame(check_ptr<Moss_Renderer>(L, 1, "renderer.end_frame(renderer)")); return 0; }
inline int renderer_gpu_device(lua_State* L) { push_handle(L, Moss_RendererGetGPUDevice(check_ptr<Moss_Renderer>(L, 1, "renderer.gpu_device(renderer)"))); return 1; }
inline int renderer_backbuffer(lua_State* L) { push_handle(L, Moss_RendererGetBackbuffer(check_ptr<Moss_Renderer>(L, 1, "renderer.backbuffer(renderer)"))); return 1; }
inline int renderer_set_camera(lua_State* L) {
    Moss_RendererSetCamera(check_ptr<Moss_Renderer>(L, 1, "renderer.set_camera(renderer)"),
                           check_ptr<Moss_Camera>(L, 2, "renderer.set_camera(camera)")); return 0;
}
inline int mesh_load_obj(lua_State* L) { push_handle(L, Moss_MeshLoadOBJ(luaL_checkstring(L, 1))); return 1; }
inline int mesh_load_fbx(lua_State* L) { push_handle(L, Moss_MeshLoadFBX(luaL_checkstring(L, 1))); return 1; }
inline int mesh_load_glb(lua_State* L) { push_handle(L, Moss_MeshLoadGLB(luaL_checkstring(L, 1))); return 1; }
inline int mesh_remove(lua_State* L) { Moss_MeshRemove(check_ptr<Moss_Mesh>(L, 1, "mesh.remove(mesh)")); invalidate_handle(L, 1); return 0; }
inline int mesh_draw(lua_State* L) { Moss_MeshDraw(check_ptr<Moss_Mesh>(L, 1, "mesh.draw(mesh)")); return 0; }
inline int model_load(lua_State* L) { push_handle(L, Moss_ModelLoad(luaL_checkstring(L, 1))); return 1; }
inline int model_load_obj(lua_State* L) { push_handle(L, Moss_ModelLoadOBJ(luaL_checkstring(L, 1))); return 1; }
inline int model_load_fbx(lua_State* L) { push_handle(L, Moss_ModelLoadFBX(luaL_checkstring(L, 1))); return 1; }
inline int model_load_glb(lua_State* L) { push_handle(L, Moss_ModelLoadGLB(luaL_checkstring(L, 1))); return 1; }
inline int model_remove(lua_State* L) { Moss_ModelRemove(check_ptr<Moss_Model>(L, 1, "model.remove(model)")); invalidate_handle(L, 1); return 0; }
inline int model_draw(lua_State* L) { Moss_ModelDraw(check_ptr<Moss_Model>(L, 1, "model.draw(model)")); return 0; }
inline int model_last_load_error(lua_State* L) { lua_pushinteger(L, static_cast<int>(Moss_ModelGetLastLoadError())); return 1; }
inline int model_last_load_error_message(lua_State* L) { const char* s = Moss_ModelGetLastLoadErrorMessage(); lua_pushstring(L, s ? s : ""); return 1; }
inline int model_find_bone(lua_State* L) { lua_pushinteger(L, Moss_ModelFindBone(check_ptr<Moss_Model>(L, 1, "model.find_bone(model)"), luaL_checkstring(L, 2))); return 1; }
inline int model_find_morph_target(lua_State* L) { lua_pushinteger(L, Moss_ModelFindMorphTarget(check_ptr<Moss_Model>(L, 1, "model.find_morph_target(model)"), luaL_checkstring(L, 2))); return 1; }
inline int model_set_morph_weight(lua_State* L) { lua_pushboolean(L, Moss_ModelSetMorphWeight(check_ptr<Moss_Model>(L, 1, "model.set_morph_weight(model)"), static_cast<uint32_t>(luaL_checkinteger(L, 2)), static_cast<float>(luaL_checknumber(L, 3)))); return 1; }
inline int model_set_bone_transform(lua_State* L) {
    float matrix[16]{};
    if (!read_matrix16(L, 3, matrix)) return luaL_error(L, "transform must be a 16-number array");
    lua_pushboolean(L, Moss_ModelSetBoneTransform(check_ptr<Moss_Model>(L, 1, "model.set_bone_transform(model)"), static_cast<uint32_t>(luaL_checkinteger(L, 2)), matrix)); return 1;
}
inline int renderer_draw_model(lua_State* L) {
    float matrix[16]{};
    if (!read_matrix16(L, 3, matrix)) return luaL_error(L, "transform must be a 16-number array");
    Moss_RendererDrawModel(check_ptr<Moss_Renderer>(L, 1, "renderer.draw_model(renderer)"), check_ptr<Moss_Model>(L, 2, "renderer.draw_model(model)"), matrix); return 0;
}
inline int renderer_draw_mesh(lua_State* L) {
    float matrix[16]{};
    if (!read_matrix16(L, 3, matrix)) return luaL_error(L, "transform must be a 16-number array");
    Moss_DrawMeshDesc desc{};
    desc.mesh = check_ptr<Moss_Mesh>(L, 2, "renderer.draw_mesh(mesh)");
    std::memcpy(desc.transform, matrix, sizeof(desc.transform));
    desc.visibility = static_cast<VisibleLayer>(luaL_optinteger(L, 4, 0xFFFFFFFFu));
    Moss_RendererDrawMesh(check_ptr<Moss_Renderer>(L, 1, "renderer.draw_mesh(renderer)"), &desc); return 0;
}
inline int camera2d_create(lua_State* L) {
    Moss_Camera2DDesc desc{};
    desc.position = read_float2(L, 1, 0, 0, true);
    desc.offset = read_float2(L, 2, 0, 0, true);
    desc.zoom = static_cast<float>(luaL_optnumber(L, 3, 1.0));
    desc.rotation = static_cast<float>(luaL_optnumber(L, 4, 0.0));
    desc.viewport_width = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    desc.viewport_height = static_cast<float>(luaL_optnumber(L, 6, 1.0));
    push_handle(L, Moss_CameraCreate2D(&desc)); return 1;
}
inline int camera3d_create(lua_State* L) {
    Moss_Camera3DDesc desc{};
    desc.position = read_float3(L, 1, 0, 0, 0, true);
    desc.target = read_float3(L, 2, 0, 0, -1, true);
    desc.up = read_float3(L, 3, 0, 1, 0, true);
    desc.fov_degrees = static_cast<float>(luaL_optnumber(L, 4, 45.0));
    desc.aspect_ratio = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    desc.near_plane = static_cast<float>(luaL_optnumber(L, 6, 0.1));
    desc.far_plane = static_cast<float>(luaL_optnumber(L, 7, 1000.0));
    push_handle(L, Moss_CameraCreate3D(&desc)); return 1;
}
inline int camera_destroy(lua_State* L) { Moss_CameraDestroy(check_ptr<Moss_Camera>(L, 1, "camera.destroy(camera)")); invalidate_handle(L, 1); return 0; }
inline int shadow_map_create(lua_State* L) {
    Moss_ShadowMapDesc desc{};
    desc.width = static_cast<uint32_t>(luaL_optinteger(L, 2, 2048));
    desc.height = static_cast<uint32_t>(luaL_optinteger(L, 3, 2048));
    desc.enable_cascades = lua_toboolean(L, 4) != 0;
    desc.cascade_count = static_cast<uint32_t>(luaL_optinteger(L, 5, 1));
    push_handle(L, Moss_RendererCreateShadowMap(check_ptr<Moss_Renderer>(L, 1, "shadow_map.create(renderer)"), &desc)); return 1;
}
inline int shadow_map_destroy(lua_State* L) { Moss_RendererDestroyShadowMap(check_ptr<Moss_Renderer>(L, 1, "shadow_map.destroy(renderer)"), check_ptr<Moss_ShadowMap>(L, 2, "shadow_map.destroy(shadow)")); invalidate_handle(L, 2); return 0; }
inline int shadow_pass_begin(lua_State* L) { Moss_RendererBeginShadowPass(check_ptr<Moss_Renderer>(L, 1, "shadow_pass.begin(renderer)"), check_ptr<Moss_ShadowMap>(L, 2, "shadow_pass.begin(shadow)")); return 0; }
inline int shadow_pass_end(lua_State* L) { Moss_RendererEndShadowPass(check_ptr<Moss_Renderer>(L, 1, "shadow_pass.end(renderer)"), check_ptr<Moss_ShadowMap>(L, 2, "shadow_pass.end(shadow)")); return 0; }
inline int renderer_line2d(lua_State* L) {
    const Vec2 from = read_vec2(L, 2), to = read_vec2(L, 3);
    const Color color = read_color(L, 4, 1, 1, 1, 1, true);
    Moss_RendererDrawLine2D(check_ptr<Moss_Renderer>(L, 1, "renderer.line2d(renderer)"), &from, &to, color, static_cast<float>(luaL_optnumber(L, 5, 1.0))); return 0;
}
inline int renderer_circle2d(lua_State* L) {
    const Vec2 center = read_vec2(L, 2);
    const Color color = read_color(L, 4, 1, 1, 1, 1, true);
    Moss_RendererDrawCircle2D(check_ptr<Moss_Renderer>(L, 1, "renderer.circle2d(renderer)"), &center, static_cast<float>(luaL_checknumber(L, 3)), color); return 0;
}
inline int renderer_box2d(lua_State* L) {
    const Vec2 center = read_vec2(L, 2), size = read_vec2(L, 3);
    const Color color = read_color(L, 5, 1, 1, 1, 1, true);
    Moss_RendererDrawBox2D(check_ptr<Moss_Renderer>(L, 1, "renderer.box2d(renderer)"), &center, &size, static_cast<float>(luaL_optnumber(L, 4, 0.0)), color); return 0;
}
inline int renderer_arrow2d(lua_State* L) {
    const Vec2 from = read_vec2(L, 2), to = read_vec2(L, 3);
    const Color color = read_color(L, 4, 1, 1, 1, 1, true);
    Moss_RendererDrawArrow2D(check_ptr<Moss_Renderer>(L, 1, "renderer.arrow2d(renderer)"), &from, &to, color, static_cast<float>(luaL_optnumber(L, 5, 8.0))); return 0;
}
inline int font_load(lua_State* L) { push_handle(L, Moss_FontLoad(luaL_checkstring(L, 1), static_cast<float>(luaL_optnumber(L, 2, 16.0)))); return 1; }
inline int font_destroy(lua_State* L) { Moss_FontDestroy(check_ptr<Moss_Font>(L, 1, "font.destroy(font)")); invalidate_handle(L, 1); return 0; }
inline int font_measure_text(lua_State* L) {
    const Moss_TextMeasure result = Moss_FontMeasureText(check_ptr<Moss_Font>(L, 1, "font.measure_text(font)"), luaL_checkstring(L, 2), static_cast<float>(luaL_optnumber(L, 3, 1.0)));
    lua_pushnumber(L, result.width); lua_pushnumber(L, result.height); return 2;
}
inline int renderer_draw_text(lua_State* L) {
    Moss_DrawTextDesc desc{};
    desc.font = check_ptr<Moss_Font>(L, 2, "renderer.draw_text(font)");
    desc.text = luaL_checkstring(L, 3);
    desc.position = read_float2(L, 4, 0, 0, true);
    desc.color = read_color(L, 5, 1, 1, 1, 1, true);
    desc.scale = static_cast<float>(luaL_optnumber(L, 6, 1.0));
    desc.depth = static_cast<float>(luaL_optnumber(L, 7, 0.0));
    desc.visibility = static_cast<VisibleLayer>(luaL_optinteger(L, 8, 0xFFFFFFFFu));
    Moss_RendererDrawText(check_ptr<Moss_Renderer>(L, 1, "renderer.draw_text(renderer)"), &desc); return 0;
}
inline int sprite_batch_create(lua_State* L) { push_handle(L, Moss_RendererCreateSpriteBatch(check_ptr<Moss_Renderer>(L, 1, "sprite_batch.create(renderer)"), static_cast<uint32_t>(luaL_optinteger(L, 2, 1024)))); return 1; }
inline int sprite_batch_clear(lua_State* L) { Moss_SpriteBatchClear(check_ptr<Moss_SpriteBatch>(L, 1, "sprite_batch.clear(batch)")); return 0; }
inline int sprite_batch_add(lua_State* L) {
    Moss_SpriteDesc desc{};
    desc.texture = check_ptr<Texture>(L, 2, "sprite_batch.add(texture)");
    desc.position = read_float2(L, 3, 0, 0, false);
    desc.size = read_float2(L, 4, 1, 1, false);
    desc.color = read_color(L, 5, 1, 1, 1, 1, true);
    desc.rotation = static_cast<float>(luaL_optnumber(L, 6, 0));
    desc.depth = static_cast<float>(luaL_optnumber(L, 7, 0));
    lua_pushboolean(L, Moss_SpriteBatchAdd(check_ptr<Moss_SpriteBatch>(L, 1, "sprite_batch.add(batch)"), &desc)); return 1;
}
inline int sprite_batch_draw(lua_State* L) { Moss_RendererDrawSpriteBatch(check_ptr<Moss_Renderer>(L, 1, "sprite_batch.draw(renderer)"), check_ptr<Moss_SpriteBatch>(L, 2, "sprite_batch.draw(batch)")); return 0; }
inline int sprite_batch_destroy(lua_State* L) { Moss_RendererDestroySpriteBatch(check_ptr<Moss_Renderer>(L, 1, "sprite_batch.destroy(renderer)"), check_ptr<Moss_SpriteBatch>(L, 2, "sprite_batch.destroy(batch)")); invalidate_handle(L, 2); return 0; }

// -----------------------------------------------------------------------------
// GPU — Lua-facing allocation helpers accept a renderer, retrieve its GPU
// device, and then call the device-taking functions in Moss_GPU.h.
// -----------------------------------------------------------------------------
inline int gpu_device_driver(lua_State* L) {
    const char* value = Moss_GetGPUDeviceDriver(check_ptr<Moss_GPUDevice>(L, 1, "gpu.device_driver(device)"));
    lua_pushstring(L, value ? value : ""); return 1;
}
inline int gpu_buffer_create(lua_State* L) {
    Moss_GPUBufferDesc desc{};
    const lua_Integer bytes = luaL_checkinteger(L, 2);
    if (bytes <= 0) return luaL_error(L, "size must be greater than zero");
    desc.size = static_cast<uint64_t>(bytes);
    desc.usage = static_cast<EGPUBufferUsage>(luaL_checkinteger(L, 3));
    desc.cpu_visible = lua_toboolean(L, 4) ? 1 : 0;
    auto* renderer = check_ptr<Moss_Renderer>(L, 1, "gpu.buffer_create(renderer)");
    push_handle(L, Moss_GPUBufferCreate(Moss_RendererGetGPUDevice(renderer), &desc)); return 1;
}
inline int gpu_buffer_upload(lua_State* L) {
    size_t size = 0;
    const char* bytes = luaL_checklstring(L, 3, &size);
    const lua_Integer offset = luaL_optinteger(L, 4, 0);
    if (offset < 0) return luaL_error(L, "offset cannot be negative");
    auto* renderer = check_ptr<Moss_Renderer>(L, 1, "gpu.buffer_upload(renderer)");
    Moss_GPUBufferUpload(Moss_RendererGetGPUDevice(renderer),
                         check_ptr<Moss_GPUBuffer>(L, 2, "gpu.buffer_upload(buffer)"),
                         bytes, static_cast<uint64_t>(size), static_cast<uint64_t>(offset)); return 0;
}
inline int gpu_buffer_destroy(lua_State* L) {
    auto* renderer = check_ptr<Moss_Renderer>(L, 1, "gpu.buffer_destroy(renderer)");
    Moss_GPUBufferDestroy(Moss_RendererGetGPUDevice(renderer), check_ptr<Moss_GPUBuffer>(L, 2, "gpu.buffer_destroy(buffer)")); invalidate_handle(L, 2); return 0;
}
inline int gpu_command_begin(lua_State* L) {
    const int queue = static_cast<int>(luaL_optinteger(L, 2, static_cast<int>(ECommandQueue::GRAPHICS)));
    auto* renderer = check_ptr<Moss_Renderer>(L, 1, "gpu.command_begin(renderer)");
    push_handle(L, Moss_GPUCommandBufferBegin(Moss_RendererGetGPUDevice(renderer), static_cast<ECommandQueue>(queue))); return 1;
}
inline int gpu_command_end(lua_State* L) { Moss_GPUCommandBufferEnd(check_ptr<Moss_GPUCommandBuffer>(L, 1, "gpu.command_end(command)")); return 0; }
inline int gpu_command_submit(lua_State* L) { auto* renderer = check_ptr<Moss_Renderer>(L, 1, "gpu.command_submit(renderer)"); Moss_GPUCommandBufferSubmit(Moss_RendererGetGPUDevice(renderer), check_ptr<Moss_GPUCommandBuffer>(L, 2, "gpu.command_submit(command)")); invalidate_handle(L, 2); return 0; }
inline int gpu_bind_pipeline(lua_State* L) { Moss_CmdBindPipeline(check_ptr<Moss_GPUCommandBuffer>(L, 1, "gpu.bind_pipeline(command)"), check_ptr<Moss_PipelineState>(L, 2, "gpu.bind_pipeline(pipeline)")); return 0; }
inline int gpu_bind_vertex_buffer(lua_State* L) { Moss_CmdBindVertexBuffer(check_ptr<Moss_GPUCommandBuffer>(L, 1, "gpu.bind_vertex_buffer(command)"), check_ptr<Moss_GPUBuffer>(L, 2, "gpu.bind_vertex_buffer(buffer)"), static_cast<uint64_t>(luaL_optinteger(L, 3, 0))); return 0; }
inline int gpu_bind_index_buffer(lua_State* L) { Moss_CmdBindIndexBuffer(check_ptr<Moss_GPUCommandBuffer>(L, 1, "gpu.bind_index_buffer(command)"), check_ptr<Moss_GPUBuffer>(L, 2, "gpu.bind_index_buffer(buffer)"), static_cast<uint64_t>(luaL_optinteger(L, 3, 0))); return 0; }
inline int gpu_draw(lua_State* L) { Moss_CmdDraw(check_ptr<Moss_GPUCommandBuffer>(L, 1, "gpu.draw(command)"), static_cast<uint32_t>(luaL_checkinteger(L, 2)), static_cast<uint32_t>(luaL_optinteger(L, 3, 0))); return 0; }
inline int gpu_dispatch(lua_State* L) { Moss_CmdDispatch(check_ptr<Moss_GPUCommandBuffer>(L, 1, "gpu.dispatch(command)"), static_cast<uint32_t>(luaL_checkinteger(L, 2)), static_cast<uint32_t>(luaL_checkinteger(L, 3)), static_cast<uint32_t>(luaL_checkinteger(L, 4))); return 0; }
inline int renderer_supports_async_compute(lua_State* L) { auto* renderer = check_ptr<Moss_Renderer>(L, 1, "renderer.supports_async_compute(renderer)"); lua_pushboolean(L, Moss_GPUDeviceSupportsAsyncCompute(Moss_RendererGetGPUDevice(renderer))); return 1; }

// -----------------------------------------------------------------------------
// Audio — wrappers use the current public Moss_Audio.h declarations. The
// callback APIs are intentionally omitted until a safe Lua-thread callback
// dispatcher exists.
// -----------------------------------------------------------------------------
inline int audio_init(lua_State* L) { lua_pushinteger(L, Moss_Init_Audio()); return 1; }
inline int audio_terminate(lua_State*) { Moss_Terminate_Audio(); return 0; }
inline int audio_update(lua_State* L) { Moss_AudioUpdate(static_cast<float>(luaL_checknumber(L, 1))); return 0; }
inline int audio_load_wav(lua_State* L) { push_handle(L, Moss_AudioLoadWavFile(luaL_checkstring(L, 1))); return 1; }
inline int audio_load_ogg(lua_State* L) { push_handle(L, Moss_AudioLoadOgg(luaL_checkstring(L, 1), static_cast<AudioLoadType>(luaL_optinteger(L, 2, static_cast<int>(AudioLoadType::FULLY_LOADED))))); return 1; }
inline int audio_source_destroy(lua_State* L) { Moss_AudioSourceDestroy(check_ptr<Moss_AudioSource>(L, 1, "audio.source_destroy(source)")); invalidate_handle(L, 1); return 0; }
inline int audio_load_mp3(lua_State* L) { push_handle(L, Moss_AudioLoadMP3(luaL_checkstring(L, 1))); return 1; }
inline int audio_channel_create(lua_State* L) { lua_pushinteger(L, Moss_AudioCreateChannel(static_cast<ChannelID>(luaL_checkinteger(L, 1)))); return 1; }
inline int audio_channel_remove(lua_State* L) { Audio_RemoveChannel(static_cast<ChannelID>(luaL_checkinteger(L, 1))); return 0; }
inline int audio_channel_master(lua_State* L) { lua_pushinteger(L, Moss_AudioGetMasterChannel()); return 1; }
inline int audio_channel_set_volume(lua_State* L) { Moss_AudioSetChannelVolume(static_cast<ChannelID>(luaL_checkinteger(L, 1)), static_cast<float>(luaL_checknumber(L, 2))); return 0; }
inline int audio_channel_set_mute(lua_State* L) { Moss_AudioSetChannelMute(static_cast<ChannelID>(luaL_checkinteger(L, 1)), lua_toboolean(L, 2) != 0); return 0; }
inline int audio_stream_create(lua_State* L) { push_handle(L, Moss_AudioStreamCreate()); return 1; }
inline int audio_stream_play(lua_State* L) { Moss_AudioStreamPlay(check_ptr<AudioStream>(L, 1, "audio.stream_play(stream)")); return 0; }
inline int audio_stream_stop(lua_State* L) { Moss_AudioStreamStop(check_ptr<AudioStream>(L, 1, "audio.stream_stop(stream)")); return 0; }
inline int audio_stream_set_volume(lua_State* L) { Moss_AudioStreamSetVolume(check_ptr<AudioStream>(L, 1, "audio.stream_set_volume(stream)"), static_cast<float>(luaL_checknumber(L, 2))); return 0; }
inline int audio_stream_set_pitch(lua_State* L) { Moss_AudioStreamSetPitch(check_ptr<AudioStream>(L, 1, "audio.stream_set_pitch(stream)"), static_cast<float>(luaL_checknumber(L, 2))); return 0; }
inline int audio_stream_set_loop(lua_State* L) { Moss_AudioStreamSetLoop(check_ptr<AudioStream>(L, 1, "audio.stream_set_loop(stream)"), lua_toboolean(L, 2) != 0); return 0; }
inline int audio_stream_set_playback_rate(lua_State* L) { Moss_AudioStreamSetPlaybackRate(check_ptr<AudioStream>(L, 1, "audio.stream_set_playback_rate(stream)"), static_cast<float>(luaL_checknumber(L, 2))); return 0; }
inline int audio_stream_set_pan(lua_State* L) { Moss_AudioStreamSetPan(check_ptr<AudioStream>(L, 1, "audio.stream_set_pan(stream)"), static_cast<float>(luaL_checknumber(L, 2))); return 0; }
inline int audio_stream_remove(lua_State* L) { Moss_AudioStreamRemove(check_ptr<AudioStream>(L, 1, "audio.stream_remove(stream)")); invalidate_handle(L, 1); return 0; }
inline int audio_stream2d_create(lua_State* L) { push_handle(L, Moss_AudioStream2DCreate()); return 1; }
inline int audio_stream2d_play(lua_State* L) { Moss_AudioStream2DPlay(check_ptr<AudioStream2D>(L, 1, "audio.stream2d_play(stream)")); return 0; }
inline int audio_stream2d_stop(lua_State* L) { Moss_AudioStream2DStop(check_ptr<AudioStream2D>(L, 1, "audio.stream2d_stop(stream)")); return 0; }
inline int audio_stream2d_set_volume(lua_State* L) { Moss_AudioStream2DSetVolume(check_ptr<AudioStream2D>(L, 1, "audio.stream2d_set_volume(stream)"), static_cast<float>(luaL_checknumber(L, 2))); return 0; }
inline int audio_stream2d_set_pitch(lua_State* L) { Moss_AudioStream2DSetPitch(check_ptr<AudioStream2D>(L, 1, "audio.stream2d_set_pitch(stream)"), static_cast<float>(luaL_checknumber(L, 2))); return 0; }
inline int audio_stream2d_set_loop(lua_State* L) { Moss_AudioStream2DSetLoop(check_ptr<AudioStream2D>(L, 1, "audio.stream2d_set_loop(stream)"), lua_toboolean(L, 2) != 0); return 0; }
inline int audio_stream2d_set_playback_rate(lua_State* L) { Moss_AudioStream2DSetPlaybackRate(check_ptr<AudioStream2D>(L, 1, "audio.stream2d_set_playback_rate(stream)"), static_cast<float>(luaL_checknumber(L, 2))); return 0; }
inline int audio_stream2d_set_pan(lua_State* L) { Moss_AudioStream2DSetPan(check_ptr<AudioStream2D>(L, 1, "audio.stream2d_set_pan(stream)"), static_cast<float>(luaL_checknumber(L, 2))); return 0; }
inline int audio_stream2d_remove(lua_State* L) { Moss_AudioStream2DRemove(check_ptr<AudioStream2D>(L, 1, "audio.stream2d_remove(stream)")); invalidate_handle(L, 1); return 0; }
inline int audio_stream3d_create(lua_State* L) { push_handle(L, Moss_AudioStream3DCreate()); return 1; }
inline int audio_stream3d_play(lua_State* L) { Moss_AudioStream3DPlay(check_ptr<AudioStream3D>(L, 1, "audio.stream3d_play(stream)")); return 0; }
inline int audio_stream3d_stop(lua_State* L) { Moss_AudioStream3DStop(check_ptr<AudioStream3D>(L, 1, "audio.stream3d_stop(stream)")); return 0; }
inline int audio_stream3d_set_volume(lua_State* L) { Moss_AudioStream3DSetVolume(check_ptr<AudioStream3D>(L, 1, "audio.stream3d_set_volume(stream)"), static_cast<float>(luaL_checknumber(L, 2))); return 0; }
inline int audio_stream3d_set_pitch(lua_State* L) { Moss_AudioStream3DSetPitch(check_ptr<AudioStream3D>(L, 1, "audio.stream3d_set_pitch(stream)"), static_cast<float>(luaL_checknumber(L, 2))); return 0; }
inline int audio_stream3d_set_loop(lua_State* L) { Moss_AudioStream3DSetLoop(check_ptr<AudioStream3D>(L, 1, "audio.stream3d_set_loop(stream)"), lua_toboolean(L, 2) != 0); return 0; }
inline int audio_stream3d_set_playback_rate(lua_State* L) { Moss_AudioStream3DSetPlaybackRate(check_ptr<AudioStream3D>(L, 1, "audio.stream3d_set_playback_rate(stream)"), static_cast<float>(luaL_checknumber(L, 2))); return 0; }
inline int audio_stream3d_set_pan(lua_State* L) { Moss_AudioStream3DSetPan(check_ptr<AudioStream3D>(L, 1, "audio.stream3d_set_pan(stream)"), static_cast<float>(luaL_checknumber(L, 2))); return 0; }
inline int audio_stream3d_set_distance_model(lua_State* L) { Moss_AudioStream3DSetDistanceModel(check_ptr<AudioStream3D>(L, 1, "audio.stream3d_set_distance_model(stream)"), static_cast<DistanceModel>(luaL_checkinteger(L, 2))); return 0; }
inline int audio_stream3d_remove(lua_State* L) { Moss_AudioStream3DRemove(check_ptr<AudioStream3D>(L, 1, "audio.stream3d_remove(stream)")); invalidate_handle(L, 1); return 0; }
inline int audio_listener2d_create(lua_State* L) { push_handle(L, Moss_AudioCreateAudioListener2D(read_float2(L, 1))); return 1; }
inline int audio_listener3d_create(lua_State* L) { push_handle(L, Moss_AudioCreateAudioListener3D(read_float3(L, 1))); return 1; }
inline int ray_listener2d_create(lua_State* L) { push_handle(L, Moss_AudioCreateRayAudioListener2D(read_float2(L, 1))); return 1; }
inline int ray_listener3d_create(lua_State* L) { push_handle(L, Moss_AudioCreateRayAudioListener3D(read_float3(L, 1))); return 1; }
inline int audio_listener2d_activate(lua_State* L) { Moss_AudioActivateAudioListener2D(check_ptr<AudioListener2D>(L, 1, "audio.listener2d_activate(listener)"), lua_toboolean(L, 2) != 0); return 0; }
inline int audio_listener3d_activate(lua_State* L) { Moss_AudioActivateAudioListener3D(check_ptr<AudioListener3D>(L, 1, "audio.listener3d_activate(listener)"), lua_toboolean(L, 2) != 0); return 0; }
inline int ray_listener2d_activate(lua_State* L) { Moss_AudioActivateRayAudioListener2D(check_ptr<RayAudioListener2D>(L, 1, "audio.ray_listener2d_activate(listener)"), lua_toboolean(L, 2) != 0); return 0; }
inline int ray_listener3d_activate(lua_State* L) { Moss_AudioActivateRayAudioListener3D(check_ptr<RayAudioListener3D>(L, 1, "audio.ray_listener3d_activate(listener)"), lua_toboolean(L, 2) != 0); return 0; }
inline int audio_listener2d_remove(lua_State* L) { Moss_AudioRemoveAudioListener2D(check_ptr<AudioListener2D>(L, 1, "audio.listener2d_remove(listener)")); invalidate_handle(L, 1); return 0; }
inline int audio_listener3d_remove(lua_State* L) { Moss_AudioRemoveAudioListener3D(check_ptr<AudioListener3D>(L, 1, "audio.listener3d_remove(listener)")); invalidate_handle(L, 1); return 0; }
inline int ray_listener2d_remove(lua_State* L) { Moss_AudioRemoveRayAudioListener2D(check_ptr<RayAudioListener2D>(L, 1, "audio.ray_listener2d_remove(listener)")); invalidate_handle(L, 1); return 0; }
inline int ray_listener3d_remove(lua_State* L) { Moss_AudioRemoveRayAudioListener3D(check_ptr<RayAudioListener3D>(L, 1, "audio.ray_listener3d_remove(listener)")); invalidate_handle(L, 1); return 0; }
inline int audio_listener3d_orientation(lua_State* L) { const Vec3 forward = read_vec3(L, 2), up = read_vec3(L, 3); Moss_AudioListenerSetOrientation(check_ptr<AudioListener3D>(L, 1, "audio.listener3d_orientation(listener)"), forward, up); return 0; }
inline int audio_speaker_ready(lua_State* L) { lua_pushboolean(L, Moss_IsSpeakerDeviceReady()); return 1; }
inline int audio_speaker_open(lua_State*) { Moss_AudioSpeakerOpen(); return 0; }
inline int audio_speaker_pause(lua_State*) { Moss_AudioSpeakerPause(); return 0; }
inline int audio_speaker_resume(lua_State*) { Moss_AudioSpeakerResume(); return 0; }
inline int audio_speaker_is_paused(lua_State* L) { lua_pushboolean(L, Moss_AudioSpeakerIsPaused()); return 1; }
inline int audio_speaker_select(lua_State* L) { lua_pushboolean(L, Moss_AudioSelectSpeakerDevice(static_cast<int>(luaL_checkinteger(L, 1)))); return 1; }
inline int audio_speaker_count(lua_State* L) { lua_pushinteger(L, Moss_ListSpeakerDevices()); return 1; }
inline int audio_speaker_name(lua_State* L) { const char* s = Moss_GetSpeakerDeviceName(static_cast<int>(luaL_checkinteger(L, 1))); lua_pushstring(L, s ? s : ""); return 1; }
inline int audio_microphone_ready(lua_State* L) { lua_pushboolean(L, Moss_IsMicrophoneDeviceReady()); return 1; }
inline int audio_microphone_open(lua_State*) { Moss_AudioMicrophoneOpen(); return 0; }
inline int audio_microphone_close(lua_State*) { Moss_AudioMicrophoneClose(); return 0; }
inline int audio_microphone_start(lua_State*) { Moss_AudioMicrophonePlay(); return 0; }
inline int audio_microphone_stop(lua_State*) { Moss_AudioMicrophoneStop(); return 0; }
inline int audio_microphone_id(lua_State* L) { lua_pushinteger(L, Moss_AudioMicrophoneID()); return 1; }
inline int audio_microphone_select(lua_State* L) { lua_pushboolean(L, Moss_AudioSelectMicrophoneDevice(static_cast<int>(luaL_checkinteger(L, 1)))); return 1; }
inline int audio_microphone_count(lua_State* L) { lua_pushinteger(L, Moss_ListMicrophoneDevices()); return 1; }
inline int audio_microphone_name(lua_State* L) { const char* s = Moss_GetMicrophoneDeviceName(static_cast<int>(luaL_checkinteger(L, 1))); lua_pushstring(L, s ? s : ""); return 1; }

// Camera-device capture, separate from renderer camera objects in moss.extra.camera.
inline void push_camera_spec(lua_State* L, const Moss_CameraSpec& spec) {
    lua_createtable(L, 0, 6);
    lua_pushinteger(L, static_cast<lua_Integer>(spec.format)); lua_setfield(L, -2, "format");
    lua_pushinteger(L, static_cast<lua_Integer>(spec.colorspace)); lua_setfield(L, -2, "colorspace");
    lua_pushinteger(L, spec.width); lua_setfield(L, -2, "width");
    lua_pushinteger(L, spec.height); lua_setfield(L, -2, "height");
    lua_pushinteger(L, spec.framerate_numerator); lua_setfield(L, -2, "framerate_numerator");
    lua_pushinteger(L, spec.framerate_denominator); lua_setfield(L, -2, "framerate_denominator");
}
inline int capture_list_cameras(lua_State* L) {
    int count = 0;
    auto* cameras = Moss_GetCameras(&count);
    lua_createtable(L, count > 0 ? count : 0, 0);
    if (cameras && count > 0) for (int i = 0; i < count; ++i) { lua_pushinteger(L, static_cast<lua_Integer>(cameras[i])); lua_rawseti(L, -2, i + 1); }
    return 1;
}
inline int capture_camera_name(lua_State* L) { const char* s = Moss_GetCameraName(static_cast<Moss_CameraID>(luaL_checkinteger(L, 1))); lua_pushstring(L, s ? s : ""); return 1; }
inline int capture_camera_position(lua_State* L) { lua_pushinteger(L, static_cast<lua_Integer>(Moss_GetCameraPosition(static_cast<Moss_CameraID>(luaL_checkinteger(L, 1))))); return 1; }
inline int capture_current_driver(lua_State* L) { const char* s = Moss_GetCurrentCameraDriver(); lua_pushstring(L, s ? s : ""); return 1; }
inline int capture_driver_count(lua_State* L) { lua_pushinteger(L, Moss_GetNumCameraDrivers()); return 1; }
inline int capture_supported_formats(lua_State* L) {
    int count = 0; auto* specs = Moss_GetCameraSupportedFormats(static_cast<Moss_CameraID>(luaL_checkinteger(L, 1)), &count);
    lua_createtable(L, count > 0 ? count : 0, 0);
    if (specs && count > 0) for (int i = 0; i < count; ++i) { push_camera_spec(L, specs[i]); lua_rawseti(L, -2, i + 1); }
    return 1;
}
inline int capture_open(lua_State* L) {
    const auto id = static_cast<Moss_CameraID>(luaL_checkinteger(L, 1));
    luaL_checktype(L, 2, LUA_TTABLE);
    Moss_CameraSpec spec{};
    lua_getfield(L, 2, "format"); spec.format = static_cast<PixelFormat>(luaL_checkinteger(L, -1)); lua_pop(L, 1);
    lua_getfield(L, 2, "colorspace"); spec.colorspace = static_cast<Colorspace>(luaL_checkinteger(L, -1)); lua_pop(L, 1);
    lua_getfield(L, 2, "width"); spec.width = static_cast<int>(luaL_checkinteger(L, -1)); lua_pop(L, 1);
    lua_getfield(L, 2, "height"); spec.height = static_cast<int>(luaL_checkinteger(L, -1)); lua_pop(L, 1);
    lua_getfield(L, 2, "framerate_numerator"); spec.framerate_numerator = static_cast<int>(luaL_optinteger(L, -1, 30)); lua_pop(L, 1);
    lua_getfield(L, 2, "framerate_denominator"); spec.framerate_denominator = static_cast<int>(luaL_optinteger(L, -1, 1)); lua_pop(L, 1);
    push_handle(L, Moss_OpenCapture(id, &spec)); return 1;
}
inline int capture_acquire_frame(lua_State* L) {
    uint64_t timestamp = 0;
    auto* surface = Moss_AcquireCameraFrame(check_ptr<Moss_Capture>(L, 1, "capture.acquire_frame(capture)"), &timestamp);
    push_handle(L, surface); lua_pushinteger(L, static_cast<lua_Integer>(timestamp)); return 2;
}
inline int capture_release_frame(lua_State* L) { Moss_ReleaseCameraFrame(check_ptr<Moss_Capture>(L, 1, "capture.release_frame(capture)"), check_ptr<Moss_Surface>(L, 2, "capture.release_frame(surface)")); invalidate_handle(L, 2); return 0; }
inline int capture_format(lua_State* L) { Moss_CameraSpec spec{}; if (!Moss_GetCameraFormat(check_ptr<Moss_Capture>(L, 1, "capture.format(capture)"), &spec)) { lua_pushnil(L); return 1; } push_camera_spec(L, spec); return 1; }
inline int capture_permission_state(lua_State* L) { lua_pushinteger(L, static_cast<lua_Integer>(Moss_GetCameraPermissionState(check_ptr<Moss_Capture>(L, 1, "capture.permission_state(capture)")))); return 1; }
inline int capture_close(lua_State* L) { Moss_CloseCamera(check_ptr<Moss_Capture>(L, 1, "capture.close(capture)")); invalidate_handle(L, 1); return 0; }
inline int capture_id(lua_State* L) { lua_pushinteger(L, static_cast<lua_Integer>(Moss_GetCameraID(check_ptr<Moss_Capture>(L, 1, "capture.id(capture)")))); return 1; }


// Typed microphone capture API. Lua callbacks intentionally aren't provided;
// the caller pulls PCM samples on the Lua/game thread instead of invoking Lua
// directly from an audio thread.
inline void push_microphone_levels(lua_State* L, const Moss_MicrophoneLevels& levels) {
    lua_createtable(L, 0, 4);
    lua_pushnumber(L, levels.rms); lua_setfield(L, -2, "rms");
    lua_pushnumber(L, levels.peak); lua_setfield(L, -2, "peak");
    lua_pushnumber(L, levels.smoothed_volume); lua_setfield(L, -2, "smoothed_volume");
    lua_pushboolean(L, levels.voice_activity); lua_setfield(L, -2, "voice_activity");
}
inline int microphone_device_count(lua_State* L) { lua_pushinteger(L, Moss_MicrophoneGetDeviceCount()); return 1; }
inline int microphone_device_name(lua_State* L) { const char* s = Moss_MicrophoneGetDeviceName(static_cast<uint32_t>(luaL_checkinteger(L, 1))); lua_pushstring(L, s ? s : ""); return 1; }
inline int microphone_open(lua_State* L) {
    Moss_MicrophoneDesc desc{};
    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "device_index"); if (!lua_isnil(L, -1)) desc.device_index = static_cast<uint32_t>(lua_tointeger(L, -1)); lua_pop(L, 1);
        lua_getfield(L, 1, "sample_rate"); if (!lua_isnil(L, -1)) desc.sample_rate = static_cast<uint32_t>(lua_tointeger(L, -1)); lua_pop(L, 1);
        lua_getfield(L, 1, "channels"); if (!lua_isnil(L, -1)) desc.channels = static_cast<uint32_t>(lua_tointeger(L, -1)); lua_pop(L, 1);
        lua_getfield(L, 1, "buffer_frames"); if (!lua_isnil(L, -1)) desc.buffer_frames = static_cast<uint32_t>(lua_tointeger(L, -1)); lua_pop(L, 1);
        lua_getfield(L, 1, "ring_buffer_frames"); if (!lua_isnil(L, -1)) desc.ring_buffer_frames = static_cast<uint32_t>(lua_tointeger(L, -1)); lua_pop(L, 1);
        lua_getfield(L, 1, "start_immediately"); if (!lua_isnil(L, -1)) desc.start_immediately = lua_toboolean(L, -1) != 0; lua_pop(L, 1);
        lua_getfield(L, 1, "enable_voice_metrics"); if (!lua_isnil(L, -1)) desc.enable_voice_metrics = lua_toboolean(L, -1) != 0; lua_pop(L, 1);
    }
    push_handle(L, Moss_MicrophoneOpen(&desc)); return 1;
}
inline int microphone_close(lua_State* L) { Moss_MicrophoneClose(check_ptr<Moss_Microphone>(L, 1, "microphone.close(microphone)")); invalidate_handle(L, 1); return 0; }
inline int microphone_start(lua_State* L) { lua_pushboolean(L, Moss_MicrophoneStart(check_ptr<Moss_Microphone>(L, 1, "microphone.start(microphone)"))); return 1; }
inline int microphone_stop(lua_State* L) { Moss_MicrophoneStop(check_ptr<Moss_Microphone>(L, 1, "microphone.stop(microphone)")); return 0; }
inline int microphone_read(lua_State* L) {
    auto* microphone = check_ptr<Moss_Microphone>(L, 1, "microphone.read(microphone)");
    const lua_Integer requested = luaL_checkinteger(L, 2);
    if (requested < 0 || requested > 1'048'576) return luaL_error(L, "max_frames must be between 0 and 1048576");
    std::vector<float> samples(static_cast<size_t>(requested));
    const auto actual = Moss_MicrophoneRead(microphone, samples.data(), static_cast<uint32_t>(requested));
    const size_t count = std::min<size_t>(static_cast<size_t>(actual), samples.size());
    lua_createtable(L, static_cast<int>(count), 0);
    for (size_t i = 0; i < count; ++i) { lua_pushnumber(L, samples[i]); lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1)); }
    return 1;
}
inline int microphone_set_gain(lua_State* L) { Moss_MicrophoneSetGain(check_ptr<Moss_Microphone>(L, 1, "microphone.set_gain(microphone)"), static_cast<float>(luaL_checknumber(L, 2))); return 0; }
inline int microphone_sample_rate(lua_State* L) { lua_pushinteger(L, Moss_MicrophoneGetSampleRate(check_ptr<Moss_Microphone>(L, 1, "microphone.sample_rate(microphone)"))); return 1; }
inline int microphone_channels(lua_State* L) { lua_pushinteger(L, Moss_MicrophoneGetChannels(check_ptr<Moss_Microphone>(L, 1, "microphone.channels(microphone)"))); return 1; }
inline int microphone_levels(lua_State* L) { push_microphone_levels(L, Moss_MicrophoneGetLevels(check_ptr<Moss_Microphone>(L, 1, "microphone.levels(microphone)"))); return 1; }
inline int microphone_rms(lua_State* L) { lua_pushnumber(L, Moss_MicrophoneGetLevelRMS(check_ptr<Moss_Microphone>(L, 1, "microphone.rms(microphone)"))); return 1; }
inline int microphone_peak(lua_State* L) { lua_pushnumber(L, Moss_MicrophoneGetLevelPeak(check_ptr<Moss_Microphone>(L, 1, "microphone.peak(microphone)"))); return 1; }
inline int microphone_smoothed_volume(lua_State* L) { lua_pushnumber(L, Moss_MicrophoneGetSmoothedVolume(check_ptr<Moss_Microphone>(L, 1, "microphone.smoothed_volume(microphone)"))); return 1; }
inline int microphone_voice_activity(lua_State* L) { lua_pushboolean(L, Moss_MicrophoneGetVoiceActivity(check_ptr<Moss_Microphone>(L, 1, "microphone.voice_activity(microphone)"))); return 1; }


// -----------------------------------------------------------------------------
// XR — explicit handles and table-based poses/views.
// -----------------------------------------------------------------------------
inline int xr_initialize(lua_State* L) { lua_pushboolean(L, Moss_XR_Initialize(check_ptr<Moss_Renderer>(L, 1, "xr.initialize(renderer)"))); return 1; }
inline int xr_shutdown(lua_State*) { Moss_XR_Shutdown(); return 0; }
inline int xr_validation(lua_State* L) { lua_pushboolean(L, Moss_XR_EnableValidationLayers()); return 1; }
inline int xr_backend_name(lua_State* L) { const char* s = Moss_XR_GetBackendName(); lua_pushstring(L, s ? s : ""); return 1; }
inline int xr_backend_version(lua_State* L) { lua_pushinteger(L, Moss_XR_GetBackendVersion()); return 1; }
inline int xr_delta_seconds(lua_State* L) { lua_pushnumber(L, Moss_XR_GetDeltaSeconds()); return 1; }
inline int xr_capabilities(lua_State* L) {
    const MossXR_Capabilities* c = Moss_XR_GetCapabilities();
    if (!c) { lua_pushnil(L); return 1; }
    lua_createtable(L, 0, 9);
    lua_pushinteger(L, c->viewCount); lua_setfield(L, -2, "view_count");
    lua_pushinteger(L, static_cast<int>(c->viewType)); lua_setfield(L, -2, "view_type");
    lua_pushboolean(L, c->handTracking); lua_setfield(L, -2, "hand_tracking");
    lua_pushboolean(L, c->eyeTracking); lua_setfield(L, -2, "eye_tracking");
    lua_pushboolean(L, c->bodyTracking); lua_setfield(L, -2, "body_tracking");
    lua_pushboolean(L, c->faceTracking); lua_setfield(L, -2, "face_tracking");
    lua_pushboolean(L, c->passthrough); lua_setfield(L, -2, "passthrough");
    lua_pushboolean(L, c->anchors); lua_setfield(L, -2, "anchors");
    lua_pushboolean(L, c->depthLayers); lua_setfield(L, -2, "depth_layers");
    return 1;
}
inline int xr_session_create(lua_State* L) { push_handle(L, Moss_XR_CreateSession()); return 1; }
inline int xr_session_destroy(lua_State* L) { Moss_XR_DestroySession(check_ptr<MossXR_Session>(L, 1, "xr.session_destroy(session)")); invalidate_handle(L, 1); return 0; }
inline int xr_session_state(lua_State* L) { lua_pushinteger(L, static_cast<int>(Moss_XR_GetSessionState(check_ptr<MossXR_Session>(L, 1, "xr.session_state(session)")))); return 1; }
inline int xr_begin_frame(lua_State* L) { lua_pushboolean(L, Moss_XR_BeginFrame(check_ptr<MossXR_Session>(L, 1, "xr.begin_frame(session)"))); return 1; }
inline int xr_end_frame(lua_State* L) { Moss_XR_EndFrame(check_ptr<MossXR_Session>(L, 1, "xr.end_frame(session)")); return 0; }
inline int xr_predicted_time(lua_State* L) { lua_pushinteger(L, static_cast<lua_Integer>(Moss_XR_GetPredictedDisplayTime(check_ptr<MossXR_Session>(L, 1, "xr.predicted_time(session)")))); return 1; }
inline int xr_handle_events(lua_State* L) { Moss_XR_HandleEvents(check_ptr<MossXR_Session>(L, 1, "xr.handle_events(session)")); return 0; }
inline int xr_view_count(lua_State* L) { lua_pushinteger(L, Moss_XR_GetViewCount(check_ptr<MossXR_Session>(L, 1, "xr.view_count(session)"))); return 1; }
inline int xr_get_view(lua_State* L) {
    MossXR_View view{};
    if (!Moss_XR_GetView(check_ptr<MossXR_Session>(L, 1, "xr.get_view(session)"), static_cast<uint32_t>(luaL_checkinteger(L, 2)), &view)) { lua_pushnil(L); return 1; }
    lua_createtable(L, 0, 4);
    push_pose(L, view.pose); lua_setfield(L, -2, "pose");
    lua_createtable(L, 4, 0);
    lua_pushnumber(L, view.fov.left); lua_rawseti(L, -2, 1);
    lua_pushnumber(L, view.fov.right); lua_rawseti(L, -2, 2);
    lua_pushnumber(L, view.fov.up); lua_rawseti(L, -2, 3);
    lua_pushnumber(L, view.fov.down); lua_rawseti(L, -2, 4);
    lua_setfield(L, -2, "fov");
    lua_pushlstring(L, reinterpret_cast<const char*>(&view.view), sizeof(view.view)); lua_setfield(L, -2, "view_matrix_bytes");
    lua_pushlstring(L, reinterpret_cast<const char*>(&view.projection), sizeof(view.projection)); lua_setfield(L, -2, "projection_matrix_bytes");
    return 1;
}
inline int xr_swapchain_create(lua_State* L) { push_handle(L, Moss_XR_CreateSwapchain(check_ptr<MossXR_Session>(L, 1, "xr.swapchain_create(session)"), static_cast<uint32_t>(luaL_checkinteger(L, 2)), static_cast<uint32_t>(luaL_checkinteger(L, 3)), static_cast<ETextureFormat>(luaL_checkinteger(L, 4)))); return 1; }
inline int xr_swapchain_destroy(lua_State* L) { Moss_XR_DestroySwapchain(check_ptr<MossXR_Swapchain>(L, 1, "xr.swapchain_destroy(swapchain)")); invalidate_handle(L, 1); return 0; }
inline int xr_swapchain_acquire(lua_State* L) { uint32_t index = 0; void* image = Moss_XR_AcquireSwapchainImage(check_ptr<MossXR_Swapchain>(L, 1, "xr.swapchain_acquire(swapchain)"), &index); if (image) lua_pushlightuserdata(L, image); else lua_pushnil(L); lua_pushinteger(L, index); return 2; }
inline int xr_swapchain_release(lua_State* L) { Moss_XR_ReleaseSwapchainImage(check_ptr<MossXR_Swapchain>(L, 1, "xr.swapchain_release(swapchain)"), static_cast<uint32_t>(luaL_checkinteger(L, 2))); return 0; }
inline int xr_anchor_create(lua_State* L) {
    MossXR_Pose pose{};
    pose.position = read_vec3(L, 1);
    const Vec4 orientation = [&]() {
        if (!lua_istable(L, 2)) { luaL_error(L, "orientation must be an {x,y,z,w} array"); return Vec4(0, 0, 0, 1); }
        return Vec4(table_number(L, 2, "x", 1, 0), table_number(L, 2, "y", 2, 0), table_number(L, 2, "z", 3, 0), table_number(L, 2, "w", 4, 1));
    }();
    pose.orientation = Quat(orientation.GetX(), orientation.GetY(), orientation.GetZ(), orientation.GetW());
    push_handle(L, Moss_XR_CreateAnchor(&pose)); return 1;
}
inline int xr_anchor_destroy(lua_State* L) { Moss_XR_DestroyAnchor(check_ptr<MossXR_Anchor>(L, 1, "xr.anchor_destroy(anchor)")); invalidate_handle(L, 1); return 0; }
inline int xr_anchor_locate(lua_State* L) { MossXR_Pose pose{}; if (!Moss_XR_LocateAnchor(check_ptr<MossXR_Anchor>(L, 1, "xr.anchor_locate(anchor)"), static_cast<MossXR_Time>(luaL_checkinteger(L, 2)), &pose)) { lua_pushnil(L); return 1; } push_pose(L, pose); return 1; }
inline int xr_projection_layer_create(lua_State* L) { push_handle(L, Moss_XR_CreateProjectionLayer(check_ptr<MossXR_Swapchain>(L, 1, "xr.projection_layer_create(swapchain)"))); return 1; }
inline int xr_quad_layer_create(lua_State* L) {
    MossXR_Pose pose{}; pose.position = read_vec3(L, 2);
    const Vec4 orientation = [&]() {
        if (!lua_istable(L, 3)) { luaL_error(L, "orientation must be an {x,y,z,w} array"); return Vec4(0, 0, 0, 1); }
        return Vec4(table_number(L, 3, "x", 1, 0), table_number(L, 3, "y", 2, 0), table_number(L, 3, "z", 3, 0), table_number(L, 3, "w", 4, 1));
    }();
    pose.orientation = Quat(orientation.GetX(), orientation.GetY(), orientation.GetZ(), orientation.GetW());
    const Vec2 size = read_vec2(L, 4);
    push_handle(L, Moss_XR_CreateQuadLayer(check_ptr<MossXR_Swapchain>(L, 1, "xr.quad_layer_create(swapchain)"), &pose, size)); return 1;
}
inline int xr_layer_destroy(lua_State* L) { Moss_XR_DestroyLayer(check_ptr<MossXR_Layer>(L, 1, "xr.layer_destroy(layer)")); invalidate_handle(L, 1); return 0; }
inline int xr_submit_layers(lua_State* L) {
    auto* session = check_ptr<MossXR_Session>(L, 1, "xr.submit_layers(session)");
    luaL_checktype(L, 2, LUA_TTABLE);
    const size_t count = lua_rawlen(L, 2);
    if (count > 64) return luaL_error(L, "at most 64 XR layers can be submitted at once");
    std::array<MossXR_Layer*, 64> layers{};
    for (size_t i = 0; i < count; ++i) {
        lua_rawgeti(L, 2, static_cast<lua_Integer>(i + 1));
        layers[i] = check_ptr<MossXR_Layer>(L, -1, "xr.submit_layers(layers[i])");
        lua_pop(L, 1);
    }
    Moss_XR_SubmitLayers(session, static_cast<uint32_t>(count), layers.data());
    return 0;
}
inline int xr_action_set_create(lua_State* L) { push_handle(L, Moss_XR_CreateActionSet(luaL_checkstring(L, 1))); return 1; }
inline int xr_action_set_destroy(lua_State* L) { Moss_XR_DestroyActionSet(check_ptr<MossXR_ActionSet>(L, 1, "xr.action_set_destroy(set)")); invalidate_handle(L, 1); return 0; }
inline int xr_action_create(lua_State* L) { push_handle(L, Moss_XR_CreateAction(check_ptr<MossXR_ActionSet>(L, 1, "xr.action_create(set)"), luaL_checkstring(L, 2), static_cast<MossXR_ActionType>(luaL_checkinteger(L, 3)))); return 1; }
inline int xr_action_destroy(lua_State* L) { Moss_XR_DestroyAction(check_ptr<MossXR_Action>(L, 1, "xr.action_destroy(action)")); invalidate_handle(L, 1); return 0; }
inline int xr_attach_action_set(lua_State* L) { Moss_XR_AttachActionSet(check_ptr<MossXR_Session>(L, 1, "xr.attach_action_set(session)"), check_ptr<MossXR_ActionSet>(L, 2, "xr.attach_action_set(set)")); return 0; }
inline int xr_sync_actions(lua_State* L) { Moss_XR_SyncActions(check_ptr<MossXR_Session>(L, 1, "xr.sync_actions(session)")); return 0; }
inline int xr_action_boolean(lua_State* L) { lua_pushboolean(L, Moss_XR_GetActionBoolean(check_ptr<MossXR_Action>(L, 1, "xr.action_boolean(action)"))); return 1; }
inline int xr_action_float(lua_State* L) { lua_pushnumber(L, Moss_XR_GetActionFloat(check_ptr<MossXR_Action>(L, 1, "xr.action_float(action)"))); return 1; }
inline int xr_action_pose(lua_State* L) { MossXR_Pose pose{}; if (!Moss_XR_GetActionPose(check_ptr<MossXR_Action>(L, 1, "xr.action_pose(action)"), &pose)) { lua_pushnil(L); return 1; } push_pose(L, pose); return 1; }
inline int xr_action_stop_haptic(lua_State* L) { Moss_XR_StopHaptic(check_ptr<MossXR_Action>(L, 1, "xr.action_stop_haptic(action)")); return 0; }
inline int xr_hand_create(lua_State* L) { push_handle(L, Moss_XR_CreateHand()); return 1; }
inline int xr_hand_destroy(lua_State* L) { Moss_XR_DestroyHand(check_ptr<MossXR_HandModifier>(L, 1, "xr.hand_destroy(hand)")); invalidate_handle(L, 1); return 0; }
inline int xr_hand_pose(lua_State* L) { MossXR_Pose pose{}; if (!Moss_XR_GetHandPose(check_ptr<MossXR_HandModifier>(L, 1, "xr.hand_pose(hand)"), &pose)) { lua_pushnil(L); return 1; } push_pose(L, pose); return 1; }
inline int xr_hand_joint_pose(lua_State* L) { MossXR_Pose pose{}; if (!Moss_XR_GetHandJointPose(check_ptr<MossXR_HandModifier>(L, 1, "xr.hand_joint_pose(hand)"), static_cast<XRHandJoint>(luaL_checkinteger(L, 2)), &pose)) { lua_pushnil(L); return 1; } push_pose(L, pose); return 1; }
inline int xr_body_create(lua_State* L) { push_handle(L, Moss_XR_CreateBody()); return 1; }
inline int xr_body_destroy(lua_State* L) { Moss_XR_DestroyBody(check_ptr<MossXR_BodyModifier>(L, 1, "xr.body_destroy(body)")); invalidate_handle(L, 1); return 0; }
inline int xr_body_joint_pose(lua_State* L) { MossXR_Pose pose{}; if (!Moss_XR_GetBodyJointPose(check_ptr<MossXR_BodyModifier>(L, 1, "xr.body_joint_pose(body)"), static_cast<XRBodyJoint>(luaL_checkinteger(L, 2)), &pose)) { lua_pushnil(L); return 1; } push_pose(L, pose); return 1; }
inline int xr_update_hand_tracking(lua_State* L) { Moss_XR_UpdateHandTracking(check_ptr<MossXR_Session>(L, 1, "xr.update_hand_tracking(session)")); return 0; }
inline int xr_update_body_tracking(lua_State* L) { Moss_XR_UpdateBodyTracking(check_ptr<MossXR_Session>(L, 1, "xr.update_body_tracking(session)")); return 0; }
inline int xr_enable_passthrough(lua_State* L) { lua_pushboolean(L, Moss_XR_EnablePassthrough(lua_toboolean(L, 1) != 0)); return 1; }
inline int xr_enable_foveation(lua_State* L) { lua_pushboolean(L, Moss_XR_EnableFoveatedRendering(lua_toboolean(L, 1) != 0)); return 1; }
inline int xr_debug_begin(lua_State* L) { Moss_XR_BeginDebugLabel(luaL_checkstring(L, 1)); return 0; }
inline int xr_debug_end(lua_State*) { Moss_XR_EndDebugLabel(); return 0; }

// -----------------------------------------------------------------------------
// Physics: lightweight public-value and collision-layer helpers. Moss_Physics.h
// exposes a larger Jolt-based C++ model; this add-on does not invent a world API.
// -----------------------------------------------------------------------------
inline int physics_object_layer_from_mask(lua_State* L) { lua_pushinteger(L, ObjectLayerPairFilterMask_GetObjectLayer(static_cast<uint32_t>(luaL_checkinteger(L, 1)), static_cast<uint32_t>(luaL_checkinteger(L, 2)))); return 1; }
inline int physics_object_layer_group(lua_State* L) { lua_pushinteger(L, ObjectLayerPairFilterMask_GetGroup(static_cast<ObjectLayer>(luaL_checkinteger(L, 1)))); return 1; }
inline int physics_object_layer_mask(lua_State* L) { lua_pushinteger(L, ObjectLayerPairFilterMask_GetMask(static_cast<ObjectLayer>(luaL_checkinteger(L, 1)))); return 1; }
inline int physics_material(lua_State* L) {
    lua_createtable(L, 0, 4);
    lua_pushnumber(L, luaL_optnumber(L, 1, 0.5)); lua_setfield(L, -2, "static_friction");
    lua_pushnumber(L, luaL_optnumber(L, 2, 0.5)); lua_setfield(L, -2, "dynamic_friction");
    lua_pushnumber(L, luaL_optnumber(L, 3, 0.0)); lua_setfield(L, -2, "restitution");
    lua_pushnumber(L, luaL_optnumber(L, 4, 1.0)); lua_setfield(L, -2, "density");
    return 1;
}

inline void register_category(lua_State* L, int parent, const char* name,
                              const luaL_Reg* functions) {
    parent = lua_absindex(L, parent);
    lua_newtable(L);
    luaL_setfuncs(L, functions, 0);
    lua_setfield(L, parent, name);
}

inline void register_into(lua_State* L, int module_index) {
    module_index = lua_absindex(L, module_index);
    luaL_newmetatable(L, kHandleMeta);
    lua_pushcfunction(L, handle_gc); lua_setfield(L, -2, "__gc");
    lua_pushliteral(L, "Moss opaque native handle"); lua_setfield(L, -2, "__name");
    lua_pop(L, 1);

    lua_newtable(L);
    const int extra = lua_absindex(L, -1);

    static const luaL_Reg std_functions[] = {
        {"degrees_to_radians", std_degrees_to_radians}, {"radians_to_degrees", std_radians_to_degrees},
        {"center_angle_around_zero", std_center_angle}, {"clamp", std_clamp}, {"lerp", std_lerp},
        {"get_ticks", std_get_ticks}, {"get_seconds", std_get_seconds}, {"get_milliseconds", std_get_milliseconds},
        {"delay", std_delay}, {"yield_cpu", std_yield}, {"local_time", std_local_time},
        {"timestamp", std_timestamp}, {"time_now", std_time_now}, {"format_time", std_format_time}, {nullptr, nullptr}
    };
    static const luaL_Reg platform_functions[] = {
        {"create_window", platform_create_window}, {"terminate_window", platform_terminate_window},
        {"poll_events", platform_poll_events}, {"should_window_close", platform_should_window_close},
        {"close_window", platform_close_window}, {"set_window_title", platform_set_window_title},
        {"window_width", platform_window_width}, {"window_height", platform_window_height}, {"message_box", platform_message_box},
        {"primary_monitor", platform_primary_monitor}, {"secondary_monitor", platform_secondary_monitor},
        {"monitor_position", platform_monitor_position}, {"monitor_physical_size", platform_monitor_physical_size},
        {"monitor_content_scale", platform_monitor_content_scale}, {"monitor_name", platform_monitor_name}, {"monitor_set_gamma", platform_monitor_set_gamma}, {nullptr, nullptr}
    };
    static const luaL_Reg input_functions[] = {
        {"key_pressed", input_key_pressed}, {"key_released", input_key_released}, {"key_just_pressed", input_key_just_pressed},
        {"key_just_released", input_key_just_released}, {"get_key", input_get_key}, {"mouse_pressed", input_mouse_pressed},
        {"mouse_released", input_mouse_released}, {"mouse_just_pressed", input_mouse_just_pressed}, {"mouse_just_released", input_mouse_just_released},
        {"mouse_position", input_mouse_position}, {"set_mouse_position", input_set_mouse_position}, {"set_mouse_visible", input_set_mouse_visible}, {nullptr, nullptr}
    };
    static const luaL_Reg gamepad_functions[] = {
        {"count", gamepad_count}, {"open", gamepad_open}, {"close", gamepad_close}, {"connected", gamepad_connected}, {"update_all", gamepad_update_all},
        {"button_pressed", gamepad_button_pressed}, {"button_just_pressed", gamepad_button_just_pressed}, {"button_just_released", gamepad_button_just_released},
        {"axis", gamepad_axis}, {"name", gamepad_name}, {"id", gamepad_id}, {"player_index", gamepad_player_index}, {"rumble", gamepad_rumble}, {"set_led", gamepad_set_led}, {nullptr, nullptr}
    };
    static const luaL_Reg system_functions[] = {
        {"cpu_count", system_cpu_count}, {"cpu_cache_line_size", system_cpu_cache_line_size}, {"ram", system_ram},
        {"open_url", system_open_url}, {"is_process_running", system_is_process_running}, {nullptr, nullptr}
    };
    static const luaL_Reg filesystem_functions[] = {
        {"copy_file", filesystem_copy_file}, {"create_directory", filesystem_create_directory}, {"remove_path", filesystem_remove_path},
        {"rename_path", filesystem_rename_path}, {"path_info", filesystem_path_info}, {"current_directory", filesystem_current_directory},
        {"base_path", filesystem_base_path}, {"user_folder", filesystem_user_folder}, {"pref_path", filesystem_pref_path}, {nullptr, nullptr}
    };
    static const luaL_Reg storage_functions[] = {
        {"open_file", storage_open_file}, {"open_title", storage_open_title}, {"open_user", storage_open_user}, {"ready", storage_ready},
        {"close", storage_close}, {"create_directory", storage_create_directory}, {"copy_file", storage_copy_file}, {"remove_path", storage_remove_path},
        {"rename_path", storage_rename_path}, {"space_remaining", storage_space_remaining}, {"path_info", storage_path_info},
        {"read_file", storage_read_file}, {"write_file", storage_write_file}, {nullptr, nullptr}
    };
    static const luaL_Reg renderer_functions[] = {
        {"create", renderer_create}, {"destroy", renderer_destroy}, {"terminate", renderer_terminate},
        {"begin_frame", renderer_begin_frame}, {"begin_frame_ex", renderer_begin_frame_ex}, {"end_frame", renderer_end_frame},
        {"gpu_device", renderer_gpu_device}, {"backbuffer", renderer_backbuffer}, {"set_camera", renderer_set_camera},
        {"draw_model", renderer_draw_model}, {"draw_mesh", renderer_draw_mesh}, {"supports_async_compute", renderer_supports_async_compute},
        {"line2d", renderer_line2d}, {"circle2d", renderer_circle2d}, {"box2d", renderer_box2d}, {"arrow2d", renderer_arrow2d},
        {"draw_text", renderer_draw_text}, {nullptr, nullptr}
    };
    static const luaL_Reg mesh_functions[] = {{"load_obj", mesh_load_obj}, {"load_fbx", mesh_load_fbx}, {"load_glb", mesh_load_glb}, {"remove", mesh_remove}, {"draw", mesh_draw}, {nullptr, nullptr}};
    static const luaL_Reg model_functions[] = {
        {"load", model_load}, {"load_obj", model_load_obj}, {"load_fbx", model_load_fbx}, {"load_glb", model_load_glb},
        {"remove", model_remove}, {"draw", model_draw}, {"last_load_error", model_last_load_error},
        {"last_load_error_message", model_last_load_error_message}, {"find_bone", model_find_bone},
        {"find_morph_target", model_find_morph_target}, {"set_morph_weight", model_set_morph_weight},
        {"set_bone_transform", model_set_bone_transform}, {nullptr, nullptr}
    };
    static const luaL_Reg camera_functions[] = {{"create2d", camera2d_create}, {"create3d", camera3d_create}, {"destroy", camera_destroy}, {nullptr, nullptr}};
    static const luaL_Reg shadow_functions[] = {{"create", shadow_map_create}, {"destroy", shadow_map_destroy}, {"begin_pass", shadow_pass_begin}, {"end_pass", shadow_pass_end}, {nullptr, nullptr}};
    static const luaL_Reg font_functions[] = {{"load", font_load}, {"destroy", font_destroy}, {"measure_text", font_measure_text}, {nullptr, nullptr}};
    static const luaL_Reg sprite_batch_functions[] = {{"create", sprite_batch_create}, {"clear", sprite_batch_clear}, {"draw", sprite_batch_draw}, {"destroy", sprite_batch_destroy}, {nullptr, nullptr}};
    static const luaL_Reg gpu_functions[] = {
        {"device_driver", gpu_device_driver}, {"buffer_create", gpu_buffer_create}, {"buffer_upload", gpu_buffer_upload},
        {"buffer_destroy", gpu_buffer_destroy}, {"command_begin", gpu_command_begin}, {"command_end", gpu_command_end},
        {"command_submit", gpu_command_submit}, {"bind_pipeline", gpu_bind_pipeline}, {"bind_vertex_buffer", gpu_bind_vertex_buffer},
        {"bind_index_buffer", gpu_bind_index_buffer}, {"draw", gpu_draw}, {"dispatch", gpu_dispatch}, {nullptr, nullptr}
    };
    static const luaL_Reg audio_functions[] = {
        {"init", audio_init}, {"terminate", audio_terminate}, {"update", audio_update},
        {"load_wav", audio_load_wav}, {"load_ogg", audio_load_ogg}, {"load_mp3", audio_load_mp3}, {"source_destroy", audio_source_destroy},
        {"channel_create", audio_channel_create}, {"channel_remove", audio_channel_remove}, {"channel_master", audio_channel_master},
        {"channel_set_volume", audio_channel_set_volume}, {"channel_set_mute", audio_channel_set_mute},
        {"stream_create", audio_stream_create}, {"stream_play", audio_stream_play}, {"stream_stop", audio_stream_stop},
        {"stream_set_volume", audio_stream_set_volume}, {"stream_set_pitch", audio_stream_set_pitch}, {"stream_set_playback_rate", audio_stream_set_playback_rate}, {"stream_set_pan", audio_stream_set_pan}, {"stream_set_loop", audio_stream_set_loop}, {"stream_remove", audio_stream_remove},
        {"stream2d_create", audio_stream2d_create}, {"stream2d_play", audio_stream2d_play}, {"stream2d_stop", audio_stream2d_stop},
        {"stream2d_set_volume", audio_stream2d_set_volume}, {"stream2d_set_pitch", audio_stream2d_set_pitch}, {"stream2d_set_playback_rate", audio_stream2d_set_playback_rate}, {"stream2d_set_pan", audio_stream2d_set_pan}, {"stream2d_set_loop", audio_stream2d_set_loop}, {"stream2d_remove", audio_stream2d_remove},
        {"stream3d_create", audio_stream3d_create}, {"stream3d_play", audio_stream3d_play}, {"stream3d_stop", audio_stream3d_stop},
        {"stream3d_set_volume", audio_stream3d_set_volume}, {"stream3d_set_pitch", audio_stream3d_set_pitch}, {"stream3d_set_playback_rate", audio_stream3d_set_playback_rate}, {"stream3d_set_pan", audio_stream3d_set_pan}, {"stream3d_set_loop", audio_stream3d_set_loop}, {"stream3d_set_distance_model", audio_stream3d_set_distance_model}, {"stream3d_remove", audio_stream3d_remove},
        {"listener2d_create", audio_listener2d_create}, {"listener3d_create", audio_listener3d_create}, {"ray_listener2d_create", ray_listener2d_create}, {"ray_listener3d_create", ray_listener3d_create},
        {"listener2d_activate", audio_listener2d_activate}, {"listener3d_activate", audio_listener3d_activate}, {"ray_listener2d_activate", ray_listener2d_activate}, {"ray_listener3d_activate", ray_listener3d_activate},
        {"listener2d_remove", audio_listener2d_remove}, {"listener3d_remove", audio_listener3d_remove}, {"ray_listener2d_remove", ray_listener2d_remove}, {"ray_listener3d_remove", ray_listener3d_remove}, {"listener3d_set_orientation", audio_listener3d_orientation},
        {"speaker_ready", audio_speaker_ready}, {"speaker_open", audio_speaker_open}, {"speaker_pause", audio_speaker_pause}, {"speaker_resume", audio_speaker_resume}, {"speaker_is_paused", audio_speaker_is_paused}, {"speaker_select", audio_speaker_select}, {"speaker_count", audio_speaker_count}, {"speaker_name", audio_speaker_name},
        {"microphone_ready", audio_microphone_ready}, {"microphone_open", audio_microphone_open}, {"microphone_close", audio_microphone_close}, {"microphone_start", audio_microphone_start}, {"microphone_stop", audio_microphone_stop}, {"microphone_id", audio_microphone_id}, {"microphone_select", audio_microphone_select}, {"microphone_count", audio_microphone_count}, {"microphone_name", audio_microphone_name}, {nullptr, nullptr}
    };
    static const luaL_Reg capture_functions[] = {
        {"list_cameras", capture_list_cameras}, {"camera_name", capture_camera_name}, {"camera_position", capture_camera_position},
        {"current_driver", capture_current_driver}, {"driver_count", capture_driver_count}, {"supported_formats", capture_supported_formats},
        {"open", capture_open}, {"acquire_frame", capture_acquire_frame}, {"release_frame", capture_release_frame},
        {"format", capture_format}, {"permission_state", capture_permission_state}, {"close", capture_close}, {"id", capture_id}, {nullptr, nullptr}
    };
    static const luaL_Reg microphone_functions[] = {
        {"device_count", microphone_device_count}, {"device_name", microphone_device_name}, {"open", microphone_open},
        {"close", microphone_close}, {"start", microphone_start}, {"stop", microphone_stop}, {"read", microphone_read},
        {"set_gain", microphone_set_gain}, {"sample_rate", microphone_sample_rate}, {"channels", microphone_channels},
        {"levels", microphone_levels}, {"rms", microphone_rms}, {"peak", microphone_peak},
        {"smoothed_volume", microphone_smoothed_volume}, {"voice_activity", microphone_voice_activity}, {nullptr, nullptr}
    };
    static const luaL_Reg xr_functions[] = {
        {"initialize", xr_initialize}, {"shutdown", xr_shutdown}, {"enable_validation_layers", xr_validation}, {"backend_name", xr_backend_name}, {"backend_version", xr_backend_version}, {"delta_seconds", xr_delta_seconds}, {"capabilities", xr_capabilities},
        {"session_create", xr_session_create}, {"session_destroy", xr_session_destroy}, {"session_state", xr_session_state}, {"begin_frame", xr_begin_frame}, {"end_frame", xr_end_frame}, {"predicted_display_time", xr_predicted_time}, {"handle_events", xr_handle_events}, {"view_count", xr_view_count}, {"get_view", xr_get_view},
        {"swapchain_create", xr_swapchain_create}, {"swapchain_destroy", xr_swapchain_destroy}, {"swapchain_acquire_image", xr_swapchain_acquire}, {"swapchain_release_image", xr_swapchain_release},
        {"anchor_create", xr_anchor_create}, {"anchor_destroy", xr_anchor_destroy}, {"anchor_locate", xr_anchor_locate}, {"projection_layer_create", xr_projection_layer_create}, {"quad_layer_create", xr_quad_layer_create}, {"layer_destroy", xr_layer_destroy}, {"submit_layers", xr_submit_layers},
        {"action_set_create", xr_action_set_create}, {"action_set_destroy", xr_action_set_destroy}, {"action_create", xr_action_create}, {"action_destroy", xr_action_destroy}, {"attach_action_set", xr_attach_action_set}, {"sync_actions", xr_sync_actions}, {"action_boolean", xr_action_boolean}, {"action_float", xr_action_float}, {"action_pose", xr_action_pose}, {"action_stop_haptic", xr_action_stop_haptic},
        {"hand_create", xr_hand_create}, {"hand_destroy", xr_hand_destroy}, {"hand_pose", xr_hand_pose}, {"hand_joint_pose", xr_hand_joint_pose}, {"body_create", xr_body_create}, {"body_destroy", xr_body_destroy}, {"body_joint_pose", xr_body_joint_pose}, {"update_hand_tracking", xr_update_hand_tracking}, {"update_body_tracking", xr_update_body_tracking}, {"enable_passthrough", xr_enable_passthrough}, {"enable_foveated_rendering", xr_enable_foveation}, {"begin_debug_label", xr_debug_begin}, {"end_debug_label", xr_debug_end}, {nullptr, nullptr}
    };
    static const luaL_Reg physics_functions[] = {{"object_layer_from_mask", physics_object_layer_from_mask}, {"object_layer_group", physics_object_layer_group}, {"object_layer_mask", physics_object_layer_mask}, {"material", physics_material}, {nullptr, nullptr}};

    register_category(L, extra, "platform", platform_functions);
    register_category(L, extra, "input", input_functions);
    register_category(L, extra, "gamepad", gamepad_functions);
    register_category(L, extra, "system", system_functions);
    register_category(L, extra, "filesystem", filesystem_functions);
    register_category(L, extra, "storage", storage_functions);
    register_category(L, extra, "std", std_functions);
    register_category(L, extra, "renderer", renderer_functions);
    register_category(L, extra, "mesh", mesh_functions);
    register_category(L, extra, "model", model_functions);
    register_category(L, extra, "camera", camera_functions);
    register_category(L, extra, "shadow_map", shadow_functions);
    register_category(L, extra, "font", font_functions);
    register_category(L, extra, "sprite_batch", sprite_batch_functions);
    register_category(L, extra, "gpu", gpu_functions);
    register_category(L, extra, "audio", audio_functions);
    register_category(L, extra, "microphone", microphone_functions);
    register_category(L, extra, "capture", capture_functions);
    register_category(L, extra, "xr", xr_functions);
    register_category(L, extra, "physics", physics_functions);

    lua_setfield(L, module_index, "extra");
}

} // namespace luamoss_extra


// -----------------------------------------------------------------------------
// Original LuaMoss convenience API.
// The extended binding groups are registered under moss.extra below.
// -----------------------------------------------------------------------------
namespace {

struct Window { Moss_Window* value{}; };
struct Renderer { Moss_Renderer* value{}; };

constexpr const char* WindowName = "moss.Window";
constexpr const char* RendererName = "moss.Renderer";
constexpr const char* Vec2Name = "moss.Vec2";
constexpr const char* Vec3Name = "moss.Vec3";
constexpr const char* Vec4Name = "moss.Vec4";
constexpr const char* ColorName = "moss.Color";
constexpr const char* QuatName = "moss.Quat";
constexpr const char* RectName = "moss.Rect";

template<class T>
T* check(lua_State* L, int index, const char* name) {
    return static_cast<T*>(luaL_checkudata(L, index, name));
}

template<class T>
T* push(lua_State* L, const char* name, const T& value) {
    auto* result = static_cast<T*>(lua_newuserdatauv(L, sizeof(T), 0));
    new (result) T(value);
    luaL_getmetatable(L, name);
    lua_setmetatable(L, -2);
    return result;
}

template<class T>
int destroy_value(lua_State* L, const char* name) {
    check<T>(L, 1, name)->~T();
    return 0;
}

void push_method_or_nil(lua_State* L, const char* meta, const char* key) {
    luaL_getmetatable(L, meta);
    lua_getfield(L, -1, key);
    lua_remove(L, -2);
}

int vec2_new(lua_State* L) {
    push(L, Vec2Name, Vec2(static_cast<float>(luaL_optnumber(L, 1, 0.0)),
                           static_cast<float>(luaL_optnumber(L, 2, 0.0))));
    return 1;
}
int vec2_index(lua_State* L) {
    auto* v = check<Vec2>(L, 1, Vec2Name);
    const char* key = luaL_checkstring(L, 2);
    if (!std::strcmp(key, "x")) { lua_pushnumber(L, v->GetX()); return 1; }
    if (!std::strcmp(key, "y")) { lua_pushnumber(L, v->GetY()); return 1; }
    push_method_or_nil(L, Vec2Name, key);
    return 1;
}
int vec2_newindex(lua_State* L) {
    auto* v = check<Vec2>(L, 1, Vec2Name);
    const char* key = luaL_checkstring(L, 2);
    const float n = static_cast<float>(luaL_checknumber(L, 3));
    if (!std::strcmp(key, "x")) { v->SetX(n); return 0; }
    if (!std::strcmp(key, "y")) { v->SetY(n); return 0; }
    return luaL_error(L, "unknown Vec2 field '%s'", key);
}
int vec2_add(lua_State* L) {
    auto* a = check<Vec2>(L, 1, Vec2Name); auto* b = check<Vec2>(L, 2, Vec2Name);
    push(L, Vec2Name, *a + *b); return 1;
}
int vec2_sub(lua_State* L) {
    auto* a = check<Vec2>(L, 1, Vec2Name); auto* b = check<Vec2>(L, 2, Vec2Name);
    push(L, Vec2Name, *a - *b); return 1;
}
int vec2_mul(lua_State* L) {
    auto* a = check<Vec2>(L, 1, Vec2Name);
    push(L, Vec2Name, *a * static_cast<float>(luaL_checknumber(L, 2))); return 1;
}

template<class T>
int vector_index(lua_State* L, const char* meta, int components) {
    auto* v = check<T>(L, 1, meta);
    const char* key = luaL_checkstring(L, 2);
    const char* keys[] = {"x", "y", "z", "w"};
    for (int i = 0; i < components; ++i) {
        if (!std::strcmp(key, keys[i])) { lua_pushnumber(L, (*v)[i]); return 1; }
    }
    push_method_or_nil(L, meta, key);
    return 1;
}
template<class T>
int vector_newindex(lua_State* L, const char* meta, int components) {
    auto* v = check<T>(L, 1, meta);
    const char* key = luaL_checkstring(L, 2);
    const float n = static_cast<float>(luaL_checknumber(L, 3));
    const char* keys[] = {"x", "y", "z", "w"};
    for (int i = 0; i < components; ++i) {
        if (!std::strcmp(key, keys[i])) { v->SetComponent(i, n); return 0; }
    }
    return luaL_error(L, "unknown vector field '%s'", key);
}
int vec3_new(lua_State* L) {
    push(L, Vec3Name, Vec3(static_cast<float>(luaL_optnumber(L, 1, 0.0)),
                           static_cast<float>(luaL_optnumber(L, 2, 0.0)),
                           static_cast<float>(luaL_optnumber(L, 3, 0.0)))); return 1;
}
int vec3_index(lua_State* L) { return vector_index<Vec3>(L, Vec3Name, 3); }
int vec3_newindex(lua_State* L) { return vector_newindex<Vec3>(L, Vec3Name, 3); }
int vec4_new(lua_State* L) {
    push(L, Vec4Name, Vec4(static_cast<float>(luaL_optnumber(L, 1, 0.0)),
                           static_cast<float>(luaL_optnumber(L, 2, 0.0)),
                           static_cast<float>(luaL_optnumber(L, 3, 0.0)),
                           static_cast<float>(luaL_optnumber(L, 4, 0.0)))); return 1;
}
int vec4_index(lua_State* L) { return vector_index<Vec4>(L, Vec4Name, 4); }
int vec4_newindex(lua_State* L) { return vector_newindex<Vec4>(L, Vec4Name, 4); }
int vec3_add(lua_State* L) { auto* a=check<Vec3>(L,1,Vec3Name); auto* b=check<Vec3>(L,2,Vec3Name); push(L,Vec3Name,*a+*b); return 1; }
int vec3_sub(lua_State* L) { auto* a=check<Vec3>(L,1,Vec3Name); auto* b=check<Vec3>(L,2,Vec3Name); push(L,Vec3Name,*a-*b); return 1; }
int vec3_mul(lua_State* L) { auto* a=check<Vec3>(L,1,Vec3Name); push(L,Vec3Name,*a*static_cast<float>(luaL_checknumber(L,2))); return 1; }
int vec3_dot(lua_State* L) { lua_pushnumber(L,check<Vec3>(L,1,Vec3Name)->Dot(*check<Vec3>(L,2,Vec3Name))); return 1; }
int vec3_cross(lua_State* L) { auto* a=check<Vec3>(L,1,Vec3Name); push(L,Vec3Name,a->Cross(*check<Vec3>(L,2,Vec3Name))); return 1; }
int vec3_length(lua_State* L) { lua_pushnumber(L,check<Vec3>(L,1,Vec3Name)->Length()); return 1; }
int vec3_normalized(lua_State* L) { push(L,Vec3Name,check<Vec3>(L,1,Vec3Name)->Normalized()); return 1; }

int color_new(lua_State* L) {
    push(L, ColorName, Color(static_cast<float>(luaL_optnumber(L,1,0.0)),
                             static_cast<float>(luaL_optnumber(L,2,0.0)),
                             static_cast<float>(luaL_optnumber(L,3,0.0)),
                             static_cast<float>(luaL_optnumber(L,4,1.0)))); return 1;
}
int color_index(lua_State* L) {
    auto* c=check<Color>(L,1,ColorName); const char* k=luaL_checkstring(L,2);
    if (!std::strcmp(k,"r")){lua_pushnumber(L,c->r);return 1;}
    if (!std::strcmp(k,"g")){lua_pushnumber(L,c->g);return 1;}
    if (!std::strcmp(k,"b")){lua_pushnumber(L,c->b);return 1;}
    if (!std::strcmp(k,"a")){lua_pushnumber(L,c->a);return 1;}
    push_method_or_nil(L,ColorName,k); return 1;
}
int color_newindex(lua_State* L) {
    auto* c=check<Color>(L,1,ColorName); const char* k=luaL_checkstring(L,2);
    const float n=static_cast<float>(luaL_checknumber(L,3));
    if (!std::strcmp(k,"r"))c->r=n; else if(!std::strcmp(k,"g"))c->g=n;
    else if(!std::strcmp(k,"b"))c->b=n; else if(!std::strcmp(k,"a"))c->a=n;
    else return luaL_error(L,"unknown Color field '%s'",k); return 0;
}
int rect_new(lua_State* L) {
    push(L,RectName,Rect(static_cast<float>(luaL_optnumber(L,1,0.0)),
                         static_cast<float>(luaL_optnumber(L,2,0.0)),
                         static_cast<float>(luaL_optnumber(L,3,0.0)),
                         static_cast<float>(luaL_optnumber(L,4,0.0)))); return 1;
}
int rect_index(lua_State* L) {
    auto* r=check<Rect>(L,1,RectName); const char* k=luaL_checkstring(L,2);
    if(!std::strcmp(k,"x")){lua_pushnumber(L,r->x);return 1;}
    if(!std::strcmp(k,"y")){lua_pushnumber(L,r->y);return 1;}
    if(!std::strcmp(k,"width")){lua_pushnumber(L,r->width);return 1;}
    if(!std::strcmp(k,"height")){lua_pushnumber(L,r->height);return 1;}
    return luaL_error(L,"unknown Rect field '%s'",k);
}
int rect_newindex(lua_State* L) {
    auto* r=check<Rect>(L,1,RectName); const char* k=luaL_checkstring(L,2);
    const float n=static_cast<float>(luaL_checknumber(L,3));
    if(!std::strcmp(k,"x"))r->x=n; else if(!std::strcmp(k,"y"))r->y=n;
    else if(!std::strcmp(k,"width"))r->width=n; else if(!std::strcmp(k,"height"))r->height=n;
    else return luaL_error(L,"unknown Rect field '%s'",k); return 0;
}
int quat_new(lua_State* L) {
    push(L,QuatName,Quat(static_cast<float>(luaL_optnumber(L,1,0.0)),
                         static_cast<float>(luaL_optnumber(L,2,0.0)),
                         static_cast<float>(luaL_optnumber(L,3,0.0)),
                         static_cast<float>(luaL_optnumber(L,4,1.0)))); return 1;
}
int quat_rotation(lua_State* L) {
    auto* axis=check<Vec3>(L,1,Vec3Name);
    push(L,QuatName,Quat::sRotation(*axis,static_cast<float>(luaL_checknumber(L,2)))); return 1;
}
int quat_rotate(lua_State* L) {
    auto* q=check<Quat>(L,1,QuatName); auto* v=check<Vec3>(L,2,Vec3Name);
    push(L,Vec3Name,*q * *v); return 1;
}
int quat_index(lua_State* L) {
    auto* q=check<Quat>(L,1,QuatName); const char* k=luaL_checkstring(L,2);
    if(!std::strcmp(k,"x")){lua_pushnumber(L,q->GetX());return 1;}
    if(!std::strcmp(k,"y")){lua_pushnumber(L,q->GetY());return 1;}
    if(!std::strcmp(k,"z")){lua_pushnumber(L,q->GetZ());return 1;}
    if(!std::strcmp(k,"w")){lua_pushnumber(L,q->GetW());return 1;}
    push_method_or_nil(L,QuatName,k); return 1;
}

int poll_events(lua_State*) { Moss_PollEvents(); return 0; }
int window_new(lua_State* L) {
    const char* title=luaL_checkstring(L,1);
    const int width=static_cast<int>(luaL_checkinteger(L,2));
    const int height=static_cast<int>(luaL_checkinteger(L,3));
    Moss_Window* native=Moss_CreateWindow(title,width,height,nullptr,nullptr);
    if(!native) return luaL_error(L,"Moss_CreateWindow failed");
    auto* w=static_cast<Window*>(lua_newuserdatauv(L,sizeof(Window),0));
    new(w) Window{}; w->value=native; luaL_setmetatable(L,WindowName); return 1;
}
int window_gc(lua_State* L) {
    auto* w=check<Window>(L,1,WindowName);
    if(w->value){Moss_TerminateWindow(w->value);w->value=nullptr;}
    return 0;
}
int window_close(lua_State* L) { Moss_CloseWindow(check<Window>(L,1,WindowName)->value); return 0; }
int window_should_close(lua_State* L) { lua_pushboolean(L,Moss_ShouldWindowClose(check<Window>(L,1,WindowName)->value)); return 1; }
int window_title(lua_State* L) { Moss_SetWindowTitle(check<Window>(L,1,WindowName)->value,luaL_checkstring(L,2)); return 0; }
int window_size(lua_State* L) { lua_pushinteger(L,Moss_GetWindowWidth()); lua_pushinteger(L,Moss_GetWindowHeight()); return 2; }
int mouse_position(lua_State* L) { int x=0,y=0; Moss_GetMousePosition(&x,&y); lua_pushinteger(L,x); lua_pushinteger(L,y); return 2; }
int key_pressed(lua_State* L) { lua_pushboolean(L,Moss_IsKeyPressed(static_cast<Keyboard>(luaL_checkinteger(L,1)))); return 1; }
int renderer_new(lua_State* L) {
    auto* w=check<Window>(L,1,WindowName);
    Moss_Renderer* native=Moss_CreateRenderer(w->value);
    if(!native) return luaL_error(L,"Moss_CreateRenderer failed");
    auto* r=static_cast<Renderer*>(lua_newuserdatauv(L,sizeof(Renderer),1));
    new(r) Renderer{}; r->value=native;
    lua_pushvalue(L,1); lua_setiuservalue(L,-2,1);
    luaL_setmetatable(L,RendererName); return 1;
}
int renderer_gc(lua_State* L) {
    auto* r=check<Renderer>(L,1,RendererName);
    if(r->value){Moss_RendererDestroy(r->value);r->value=nullptr;}
    return 0;
}
int renderer_begin(lua_State* L) { Moss_RendererBeginFrame(check<Renderer>(L,1,RendererName)->value); return 0; }
int renderer_end(lua_State* L) { Moss_RendererEndFrame(check<Renderer>(L,1,RendererName)->value); return 0; }
int renderer_line2d(lua_State* L) {
    auto* r=check<Renderer>(L,1,RendererName); auto* a=check<Vec2>(L,2,Vec2Name);
    auto* b=check<Vec2>(L,3,Vec2Name); auto* c=check<Color>(L,4,ColorName);
    Moss_RendererDrawLine2D(r->value,a,b,*c,static_cast<float>(luaL_optnumber(L,5,1.0))); return 0;
}
int renderer_circle2d(lua_State* L) {
    auto* r=check<Renderer>(L,1,RendererName); auto* p=check<Vec2>(L,2,Vec2Name); auto* c=check<Color>(L,4,ColorName);
    Moss_RendererDrawCircle2D(r->value,p,static_cast<float>(luaL_checknumber(L,3)),*c); return 0;
}

void add_meta(lua_State* L, const char* name, const luaL_Reg* methods,
              lua_CFunction index, lua_CFunction newindex, lua_CFunction gc,
              lua_CFunction gc_fallback = nullptr) {
    luaL_newmetatable(L,name);
    if(methods) luaL_setfuncs(L,methods,0);
    if(index){lua_pushcfunction(L,index);lua_setfield(L,-2,"__index");}
    if(newindex){lua_pushcfunction(L,newindex);lua_setfield(L,-2,"__newindex");}
    if(gc){lua_pushcfunction(L,gc);lua_setfield(L,-2,"__gc");}
    if(gc_fallback){lua_pushcfunction(L,gc_fallback);lua_setfield(L,-2,"__close");}
    lua_pop(L,1);
}

} // namespace

extern "C" int luaopen_moss(lua_State* L) {
    static const luaL_Reg vec2_methods[] = {{"__add",vec2_add},{"__sub",vec2_sub},{"__mul",vec2_mul},{nullptr,nullptr}};
    static const luaL_Reg vec3_methods[] = {{"dot",vec3_dot},{"cross",vec3_cross},{"length",vec3_length},{"normalized",vec3_normalized},{"__add",vec3_add},{"__sub",vec3_sub},{"__mul",vec3_mul},{nullptr,nullptr}};
    static const luaL_Reg quat_methods[] = {{"rotate",quat_rotate},{nullptr,nullptr}};
    static const luaL_Reg window_methods[] = {{"close",window_close},{"should_close",window_should_close},{"set_title",window_title},{"create_renderer",renderer_new},{nullptr,nullptr}};
    static const luaL_Reg renderer_methods[] = {{"begin_frame",renderer_begin},{"end_frame",renderer_end},{"line2d",renderer_line2d},{"circle2d",renderer_circle2d},{nullptr,nullptr}};

    add_meta(L,Vec2Name,vec2_methods,vec2_index,vec2_newindex,[](lua_State* l){return destroy_value<Vec2>(l,Vec2Name);});
    add_meta(L,Vec3Name,vec3_methods,vec3_index,vec3_newindex,[](lua_State* l){return destroy_value<Vec3>(l,Vec3Name);});
    add_meta(L,Vec4Name,nullptr,vec4_index,vec4_newindex,[](lua_State* l){return destroy_value<Vec4>(l,Vec4Name);});
    add_meta(L,ColorName,nullptr,color_index,color_newindex,[](lua_State* l){return destroy_value<Color>(l,ColorName);});
    add_meta(L,QuatName,quat_methods,quat_index,nullptr,[](lua_State* l){return destroy_value<Quat>(l,QuatName);});
    add_meta(L,RectName,nullptr,rect_index,rect_newindex,[](lua_State* l){return destroy_value<Rect>(l,RectName);});
    add_meta(L,WindowName,window_methods,nullptr,nullptr,window_gc);
    add_meta(L,RendererName,renderer_methods,nullptr,nullptr,renderer_gc);

    lua_newtable(L);
    lua_pushcfunction(L,vec2_new); lua_setfield(L,-2,"Vec2");
    lua_pushcfunction(L,vec3_new); lua_setfield(L,-2,"Vec3");
    lua_pushcfunction(L,vec4_new); lua_setfield(L,-2,"Vec4");
    lua_pushcfunction(L,color_new); lua_setfield(L,-2,"Color");
    lua_pushcfunction(L,quat_new); lua_setfield(L,-2,"Quat");
    lua_pushcfunction(L,quat_rotation); lua_setfield(L,-2,"rotation");
    lua_pushcfunction(L,rect_new); lua_setfield(L,-2,"Rect");
    lua_pushcfunction(L,window_new); lua_setfield(L,-2,"Window");
    lua_pushcfunction(L,poll_events); lua_setfield(L,-2,"poll_events");
    lua_pushcfunction(L,window_size); lua_setfield(L,-2,"window_size");
    lua_pushcfunction(L,mouse_position); lua_setfield(L,-2,"mouse_position");
    lua_pushcfunction(L,key_pressed); lua_setfield(L,-2,"key_pressed");

    // Keep the existing LuaMoss convenience functions and add the extended APIs.
    luamoss_extra::register_into(L,-1);
    return 1;
}
