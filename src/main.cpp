#include <lua.hpp>

#include <Moss/Moss_stdinc.h>
#include <Moss/Moss_Platform.h>
#include <Moss/Moss_Renderer.h>
#include <Moss/Variants/Color.h>
#include <Moss/Variants/Quat.h>
#include <Moss/Variants/Rect.h>
#include <Moss/Variants/Vector/Vec2.h>
#include <Moss/Variants/Vector/Vec3.h>
#include <Moss/Variants/Vector/Vec4.h>

#include <cstring>
#include <new>

namespace {


// Platform
struct Window { Moss_Window* value{}; };
struct Monitor { Moss_Monitor* value{}; };
struct Gamepad { Moss_Gamepad* value{}; };
struct Haptic { Moss_Haptic* value{}; };
struct Camera { Moss_Camera* value{}; };
struct Storage { Moss_Storage* value{}; };

struct { Moss_Curser* value{}; };
struct { Moss_GamepadBinding* value{}; };
struct { Moss_Capture* value{}; }; // Camera Device Dont use as the rendering camera
struct { Moss_Storage* value{}; };
struct { Moss_Surface* value{}; };
struct { Moss_HapticDirection* value{}; };
struct { Moss_HapticConstant* value{}; };
struct { Moss_HapticPeriodic* value{}; };
struct { Moss_HapticCondition* value{}; };
struct { Moss_HapticRamp* value{}; };
struct { Moss_HapticLeftRight* value{}; };
struct { Moss_HapticCustom* value{}; }; 
union { Moss_HapticEffect* value{}; };
struct { Moss_Haptic* value{}; };
struct { Moss_Gamepad* value{}; };
struct { Moss_GamepadAxis* value{}; };
struct { Moss_CameraSpec* value{}; };
struct { Moss_GammaRamp* value{}; };
struct { Moss_VideoMode* value{}; };
struct { Moss_Image* value{}; };
struct { Moss_Locale* value{}; };
struct { Moss_PenAxisEvent* value{}; };
struct { Moss_PenButtonEvent* value{}; };
struct { Moss_PenMotionEvent* value{}; };
struct { Moss_PenProximityEvent* value{}; };
struct { Moss_PenTouchEvent* value{}; };
struct { Moss_Finger* value{}; };
struct { Moss_HapticDirection* value{}; };
struct { Moss_HapticLeftRight* value{}; };
struct { Moss_HapticCustom* value{}; };
struct { Moss_HapticRamp* value{}; };
struct { Moss_HapticConstant* value{}; };
struct { Moss_HapticCondition* value{}; };
struct { Moss_HapticPeriodic* value{}; };
union { Moss_HapticEffect* value{}; };
struct { Moss_Haptic* value{}; };
struct { Moss_PathInfo* value{}; };
struct { Moss_DialogFileFilter* value{}; };

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

struct Frustum2D { };
struct Frustum3D { };

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
constexpr const char* WindowName = "moss.Window";
constexpr const char* RendererName = "moss.Renderer";

template<class T> T* check(lua_State* L, int i, const char* name) {
  return static_cast<T*>(luaL_checkudata(L, i, name));
}
template<class T> T* push(lua_State* L, const char* name, const T& value) {
  auto* result = static_cast<T*>(lua_newuserdatauv(L, sizeof(T), 0));
  new(result) T(value); luaL_setmetatable(L, name); return result;
}
template<class T> int destroy(lua_State* L, const char* name) {
  check<T>(L, 1, name)->~T(); return 0;
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

int window_new(lua_State*L){auto*w=static_cast<Window*>(lua_newuserdatauv(L,sizeof(Window),0));w->value=Moss_CreateWindow(luaL_checkstring(L,1),static_cast<int>(luaL_checkinteger(L,2)),static_cast<int>(luaL_checkinteger(L,3)),nullptr,nullptr);if(!w->value)return luaL_error(L,"Moss_CreateWindow failed");luaL_setmetatable(L,WindowName);return 1;}
int window_gc(lua_State*L){auto*w=check<Window>(L,1,WindowName);if(w->value){Moss_TerminateWindow(w->value);w->value=nullptr;}return 0;}
int window_close(lua_State*L){Moss_CloseWindow(check<Window>(L,1,WindowName)->value);return 0;}
int window_should_close(lua_State*L){lua_pushboolean(L,Moss_ShouldWindowClose(check<Window>(L,1,WindowName)->value));return 1;}
int window_title(lua_State*L){Moss_SetWindowTitle(check<Window>(L,1,WindowName)->value,luaL_checkstring(L,2));return 0;}
int renderer_new(lua_State*L){auto*w=check<Window>(L,1,WindowName);auto*r=static_cast<Renderer*>(lua_newuserdatauv(L,sizeof(Renderer),1));r->value=Moss_CreateRenderer(w->value);if(!r->value)return luaL_error(L,"Moss_CreateRenderer failed");lua_pushvalue(L,1);lua_setiuservalue(L,-2,1);luaL_setmetatable(L,RendererName);return 1;}
int renderer_gc(lua_State*L){auto*r=check<Renderer>(L,1,RendererName);if(r->value){Moss_RendererDestroy(r->value);r->value=nullptr;}return 0;}
int renderer_begin(lua_State*L){Moss_RendererBeginFrame(check<Renderer>(L,1,RendererName)->value);return 0;}
int renderer_end(lua_State*L){Moss_RendererEndFrame(check<Renderer>(L,1,RendererName)->value);return 0;}
int renderer_line2d(lua_State*L){auto*r=check<Renderer>(L,1,RendererName);auto*a=check<Vec2>(L,2,Vec2Name);auto*b=check<Vec2>(L,3,Vec2Name);auto*c=check<Color>(L,4,ColorName);Moss_RendererDrawLine2D(r->value,a,b,*c,static_cast<float>(luaL_optnumber(L,5,1)));return 0;}
int renderer_circle2d(lua_State*L){auto*r=check<Renderer>(L,1,RendererName);auto*p=check<Vec2>(L,2,Vec2Name);auto*c=check<Color>(L,4,ColorName);Moss_RendererDrawCircle2D(r->value,p,static_cast<float>(luaL_checknumber(L,3)),*c);return 0;}
int window_size(lua_State*L){lua_pushinteger(L,Moss_GetWindowWidth());lua_pushinteger(L,Moss_GetWindowHeight());return 2;}
int mouse_position(lua_State*L){int x,y;Moss_GetMousePosition(&x,&y);lua_pushinteger(L,x);lua_pushinteger(L,y);return 2;}
int key_pressed(lua_State*L){lua_pushboolean(L,Moss_IsKeyPressed(static_cast<Moss_Keyboard>(luaL_checkinteger(L,1))));return 1;}

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
