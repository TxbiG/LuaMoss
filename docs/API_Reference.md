# LuaMoss API Reference

> **Complete-parity public API design for Lua.** This document specifies the intended idiomatic API when the binding exposes the full Moss feature set. It is a language-facing design contract, not a statement that every projected member is implemented in the current wrapper.

## 1. API design principles

Moss C++/CMoss is the canonical feature and behaviour baseline. The language binding is a **native Lua 5.4 library**, not a mechanical translation of C++ identifiers. The FFI may use C-compatible handles and exported `Moss_*` symbols internally, but ordinary users should not see those details.

The public surface is a Lua 5.4 module. Use lowercase snake_case module functions, tables for option records, userdata for owned native resources, colon syntax for object methods, and idiomatic `nil, error` failure returns.

The binding must preserve feature coverage and semantics, not the number or spelling of native functions. Several native calls may become one managed method, a property, an iterator, a scoped object or a language protocol. Conversely, an overloaded native function may become distinct named methods when the language does not support overloads cleanly.

### Non-negotiable rules

- Keep the complete native feature set: Core, Platform, GPU, Renderer, Audio, GUI, Physics, Networking, XR, Navigation, Geometry, Resources, Skeleton, TriangleSplitter and Variants.
- Expose resource objects rather than asking users to pass opaque pointers repeatedly.
- Hide allocator plumbing, user-data pointers, C calling conventions, struct packing and raw WASM/JNI/P/Invoke handles behind the binding layer.
- Preserve status-query meaning. `false`, `0`, an empty collection and an absent optional value are not interchangeable with an error.
- Use asynchronous operations only when the target runtime requires them. WebAssembly startup and browser fetches are asynchronous; ordinary native API calls are not made asynchronous merely for stylistic consistency.
- Document platform/backend feature gates. A declared API is not proof that a backend or optional transport is available in a particular build.

## 2. Package and module layout

```lua
local moss = require("moss")
local platform = moss.platform
local audio = moss.audio
```

### Subsystem map

| Moss subsystem | Public module / facade | Responsibility |
|---|---|---|
| Core | `moss.core` | Foundational value types, logging, memory, profiler, jobs, streams and utilities. |
| Platform | `moss.platform` | Windows/surfaces, monitors, events, input, gamepads, sensors, cameras, storage and dialogs. |
| GPU | `moss.gpu` | Devices, buffers, textures, samplers, shaders, pipelines, command recording, swapchains and synchronization. |
| Renderer | `moss.renderer` | Frame lifecycle, materials, resource sets, framebuffers, meshes, models, fonts, sprites, cameras and debug draw. |
| Audio | `moss.audio` | Audio engine, sources, streams, channels, effects, listeners, acoustics, speaker and microphone capture. |
| GUI | `moss.gui` | Contexts, windows, containers, widgets, text/scalar input and drawing. |
| Physics | `moss.physics` | Bodies, shapes, filters, collisions, constraints, characters, vehicles, soft bodies and navigation integration. |
| Network | `moss.network` | Hosts, peers, packets, transports, HTTP/TLS, WebSocket and WebRTC where enabled. |
| XR | `moss.xr` | Runtime/session lifecycle, views, actions, tracking, haptics, hand/body/face tracking and composition. |

The public package/module root should follow the repository's established package metadata. The names in this document are the recommended public design; implementation namespaces can be adjusted to the actual package identifier without changing the member design.

## 3. Core language type system

| Native Moss concept | Lua 5.4 public representation | Design requirement |
|---|---|---|
| `bool` | `boolean` | Keep boolean status and query values boolean. |
| Fixed-width signed/unsigned integers | `integer` | Preserve width and unsigned semantics at the ABI boundary. |
| `size_t` | `integer` | Must be large enough for the host architecture. |
| `float` / `double` | `number` | Do not narrow double precision silently. |
| UTF-8 text | `string` | Copy or borrow according to documented native lifetime; validate encoding. |
| Opaque native resource | `full userdata with :close()` | Never expose raw pointers as normal user-facing values. |
| Pointer + element count | `table / string / userdata buffer` | Validate length, element type and retention duration. |
| Function pointer + user data | `Lua function held by registry reference` | Unregister before callback state is released. |

### Value types and descriptors

Vectors, matrices, quaternions, colours, bounds, planes, rectangles and descriptor types should be real Lua 5.4 values. Use value structs/records where copy semantics are cheap and safe; use resource classes only for objects with identity or native lifetime. Descriptor objects should have named fields/options instead of long sequences of positional primitives. Validate invalid enum combinations before crossing FFI.

## 4. Public resource model

| Moss native resource family | Recommended public type | Ownership model |
|---|---|---|
| Window / monitor / gamepad / camera / capture / storage | `Window`, `Monitor`, `Gamepad`, `Camera`, `Capture`, `Storage` | The application owns platform state; each opened resource has explicit close/dispose semantics. |
| GPU device / buffer / texture / sampler / shader / pipeline / command buffer / fence / semaphore | `GpuDevice`, `GpuBuffer`, `GpuTexture`, `GpuSampler`, `GpuShader`, `GpuPipeline`, `GpuCommandBuffer`, `GpuFence`, `GpuSemaphore` | Child objects retain or borrow their creating device; destroy children before device shutdown. |
| Renderer / material / framebuffer / mesh / model / font / sprite / batch | `Renderer`, `Material`, `Framebuffer`, `Mesh`, `Model`, `Font`, `Sprite`, `SpriteBatch` | Renderer-owned resources cannot outlive the renderer unless explicitly reference-counted. |
| Audio source / stream / listener / effect / microphone | `AudioSource`, `AudioStream`, `AudioListener`, `AudioEffect`, `Microphone` | Release sources, streams and listeners before audio shutdown. |
| GUI context | `GuiContext` | Context scopes widget calls; begin/end pairs are balanced by scoped helpers or callbacks. |
| Physics system / body / shape / constraint / character / vehicle / soft body | `PhysicsSystem`, `Body`, `Shape`, `Constraint`, `Character`, `Vehicle`, `SoftBody` | System and job/allocator dependencies outlive bodies and simulation work. |
| Network host / peer / packet / HTTP request / WebSocket / WebRTC | `NetworkHost`, `Peer`, `Packet`, `HttpRequest`, `WebSocket`, `WebRtcConnection` | Explicit ownership of hosts, packets, requests and connections. |
| XR instance / session / action set / action / hand / body / face | `XrRuntime`, `XrSession`, `XrActionSet`, `XrAction`, `HandTracker`, `BodyTracker`, `FaceTracker` | Session-bound objects cannot outlive the session/runtime they depend on. |

The exact implementation may consolidate thin native objects where that is safer (for example, a renderer may own framebuffers or a device may own command pools). The public API must still make ownership unambiguous.

## 5. Lifecycle, errors, callbacks and concurrency

### Resource lifetime

Owned resources are full userdata with `__gc` as a safety net and an explicit `:close()` method for deterministic release. `__close` should support Lua 5.4 to enable to-be-closed variables. Borrowed views must not have independent destructors. Use tables for descriptor arguments and retain registry references for callbacks until unregister.

### Errors and results

Functions that can fail return `value` on success or `nil, message` on failure. Boolean queries return one boolean value. Do not raise errors for routine missing assets or unavailable devices; reserve `error()` for programmer errors and offer `assert` as a caller choice. Callback errors must be caught at the C boundary and converted to logged Lua errors.

### Threading and callbacks

- State which thread owns window/event pumping, renderer submission, audio callbacks, physics updates and XR frame operations.
- Do not promise thread safety unless the native subsystem guarantees it.
- Keep callback state alive until native unregistration completes; no callback may run after disposal/close returns.
- Never allow a managed exception, panic or language-runtime longjmp to unwind through a C ABI frame.
- For asynchronous APIs, define cancellation, completion and disposal behaviour. Do not return a view over memory that is freed when the Promise/callback completes.

## 6. Idiomatic quick-start patterns

### Platform and event loop

```lua
local moss = require("moss")
local window, err = moss.platform.create_window {
    title = "Moss application",
    width = 1280,
    height = 720,
}
assert(window, err)

while not window:should_close() do
    moss.platform.poll_events()
    -- Update application state.
end
window:close()
```

### GPU resource creation

```lua
local device = assert(moss.gpu.create_device { adapter = "default" })
local buffer = assert(device:create_buffer {
    size = #vertices,
    usage = "vertex",
})
buffer:upload(vertices)
-- Close resources in reverse creation order.
buffer:close()
device:close()
```

### Renderer frame

```lua
local frame = assert(renderer:begin_frame())
frame:set_camera(camera)
frame:draw_sprite(sprite)
frame:draw_text("Hello Moss", position, text_style)
frame:finish()
```

### Audio playback

```lua
assert(moss.audio.initialize())
local source = assert(moss.audio.load_wav("assets/theme.wav"))
source:play()
moss.audio.update(delta_time)
source:close()
moss.audio.shutdown()
```

### GUI context and widgets

```lua
local ui = moss.gui.create_context()
ui:window("Inspector", function(panel)
    panel:text("Player")
    local enabled = panel:checkbox("Enabled", current_value)
end)
ui:close()
```

### Physics world

```lua
local physics = assert(moss.physics.create_system(settings))
local body = assert(physics.bodies:create {
    shape = moss.physics.shape.box(size),
    motion_type = "dynamic",
})
physics:step(delta_time)
local contacts = physics.query:contacts(body)
```

### Networking

```lua
local host = assert(moss.network.create_host(host_options))
while true do
    local event = host:service(1)
    if not event then break end
    if event.type == "received" then
        -- event.peer and event.packet
    end
end
host:close()
```

### XR session

```lua
local session = assert(moss.xr.create_session(renderer))
while session:poll_events() do
    if session:begin_frame() then
        local views = session:locate_views()
        -- Render active views.
        session:end_frame()
    end
end
session:close()
```

The examples show the intended public shape and lifecycle rules. Names and overloads in the index below are the parity mapping target; implementation-specific initialization parameters can be added to the relevant options objects without exposing the FFI.

## 7. Complete public API projection index

The following table maps every exported callable/allocator callback in the supplied CMoss declaration catalogue to a proposed Lua 5.4 public entry point. The native name is included **only for traceability**. It is not the public name that developers should call. Target signatures use idiomatic types and hide receiver/context handles where those belong to an object.

- **1063 indexed entries:** 1,058 callable/constructor declarations and five allocator callback values.
- Duplicate native overloads are retained as separate rows where their parameters differ; the public API should resolve each to a distinct overload or a disambiguated method name.
- Native pointer/out-parameter patterns should become owned resource values, spans/slices, tuples/records or callback objects as appropriate to the language.

### `Moss_Audio.h` → `moss.audio`

| Native symbol (traceability) | Public API projection | Target-language signature | Kind |
|---|---|---|---|
| `Moss_Init_Audio` | `moss.audio.initialize(...)` | `function moss.audio.initialize() end` | function |
| `Moss_Terminate_Audio` | `moss.audio.shutdown(...)` | `function moss.audio.shutdown() end` | function |
| `Moss_AudioUpdate` | `moss.audio.update(...)` | `function moss.audio.update(delta_time) end` | function |
| `Moss_AudioLoadWavFile` | `moss.audio.load_wav_file(...)` | `moss.audio.load_wav_file(path) -> AudioSource \| nil, error?` | function |
| `Moss_AudioLoadWav` | `moss.audio.load_wav(...)` | `function moss.audio.load_wav() end` | function |
| `Moss_AudioLoadOgg` | `moss.audio.load_ogg(...)` | `function moss.audio.load_ogg(filename, type) end` | function |
| `Moss_AudioLoadMP3` | `moss.audio.load_mp3(...)` | `function moss.audio.load_mp3(filename) end` | function |
| `Moss_AudioSourceDestroy` | `source:close(...)` | `function source:close() end` | function |
| `Moss_AudioCaptureMicrophone` | `moss.audio.capture_microphone(...)` | `moss.audio.capture_microphone(microphone) -> AudioSource \| nil, error?` | function |
| `Moss_AudioCreateEffect` (overload 1) | `moss.audio.create_effect(...)` | `function moss.audio.create_effect(type) end` | function |
| `Moss_AudioCreateEffect` (overload 2) | `effect:set_parameter(...)` | `function effect:set_parameter(param_name, value) end` | function |
| `Moss_AudioRemoveEffect` | `effect:close(...)` | `function effect:close() end` | function |
| `Moss_AudioCreateChannel` | `moss.audio.create_channel(...)` | `function moss.audio.create_channel(channel) end` | function |
| `Audio_RemoveChannel` | `moss.audio.remove_channel(...)` | `function moss.audio.remove_channel(channel) end` | function |
| `Moss_AudioGetMasterChannel` | `moss.audio.get_master_channel(...)` | `function moss.audio.get_master_channel() end` | function |
| `Moss_AudioSetChannelVolume` | `moss.audio.set_channel_volume(...)` | `function moss.audio.set_channel_volume(channel, volume) end` | function |
| `Moss_AudioSetChannelMute` | `moss.audio.set_channel_mute(...)` | `function moss.audio.set_channel_mute(channel, mute) end` | function |
| `Moss_AudioAddChannelEffect` | `moss.audio.add_effect_to_channel(...)` | `function moss.audio.add_effect_to_channel(channel, effect) end` | function |
| `Moss_AudioRemoveChannelEffect` | `moss.audio.remove_effect_from_channel(...)` | `function moss.audio.remove_effect_from_channel(channel, effect) end` | function |
| `Moss_AudioRemoveAllChannelEffects` | `moss.audio.clear_channel_effects(...)` | `function moss.audio.clear_channel_effects(channel) end` | function |
| `Moss_AudioStreamCreate` | `moss.audio.create_stream(...)` | `function moss.audio.create_stream() end` | function |
| `Moss_AudioStreamPlay` | `stream:play(...)` | `function stream:play() end` | function |
| `Moss_AudioStreamStop` | `stream:stop(...)` | `function stream:stop() end` | function |
| `Moss_AudioStreamSetVolume` | `stream:set_volume(...)` | `function stream:set_volume(volume) end` | function |
| `Moss_AudioStreamSetPitch` | `stream:set_pitch(...)` | `function stream:set_pitch(pitch) end` | function |
| `Moss_AudioStreamSetPlaybackRate` | `stream:set_playback_rate(...)` | `function stream:set_playback_rate(rate) end` | function |
| `Moss_AudioStreamSetPan` | `stream:set_pan(...)` | `function stream:set_pan(pan) end` | function |
| `Moss_AudioStreamSetLoop` | `stream:set_loop(...)` | `function stream:set_loop(loop) end` | function |
| `Moss_AudioStreamRemove` | `stream:close(...)` | `function stream:close() end` | function |
| `Moss_AudioStream2DCreate` | `moss.audio.create_stream_dimension2_d(...)` | `function moss.audio.create_stream_dimension2_d() end` | function |
| `Moss_AudioStream2DPlay` | `stream:play(...)` | `function stream:play() end` | function |
| `Moss_AudioStream2DStop` | `stream:stop(...)` | `function stream:stop() end` | function |
| `Moss_AudioStream2DSetVolume` | `stream:set_volume(...)` | `function stream:set_volume(volume) end` | function |
| `Moss_AudioStream2DSetPitch` | `stream:set_pitch(...)` | `function stream:set_pitch(pitch) end` | function |
| `Moss_AudioStream2DSetPan` | `stream:set_pan(...)` | `function stream:set_pan(pan) end` | function |
| `Moss_AudioStream2DSetLoop` | `stream:set_loop(...)` | `function stream:set_loop(loop) end` | function |
| `Moss_AudioStream3DSetPosition` (overload 1) | `stream:stream_dimension3_set_position(...)` | `function stream:stream_dimension3_set_position() end` | function |
| `Moss_AudioStream3DSetVelocity` (overload 1) | `stream:stream_dimension3_set_velocity(...)` | `function stream:stream_dimension3_set_velocity() end` | function |
| `Moss_AudioStream3DSetMaxDistance` (overload 1) | `stream:stream_dimension3_set_max_distance(...)` | `function stream:stream_dimension3_set_max_distance() end` | function |
| `Moss_AudioStream2DRemove` | `stream:close(...)` | `function stream:close() end` | function |
| `Moss_AudioStream3DCreate` | `moss.audio.create_stream_dimension3_d(...)` | `function moss.audio.create_stream_dimension3_d() end` | function |
| `Moss_AudioStream3DPlay` | `stream:play(...)` | `function stream:play() end` | function |
| `Moss_AudioStream3DStop` | `stream:stop(...)` | `function stream:stop() end` | function |
| `Moss_AudioStream3DSetVolume` | `stream:set_volume(...)` | `function stream:set_volume(volume) end` | function |
| `Moss_AudioStream3DSetPitch` | `stream:set_pitch(...)` | `function stream:set_pitch(pitch) end` | function |
| `Moss_AudioStream3DSetPlaybackRate` | `stream:set_playback_rate(...)` | `function stream:set_playback_rate(rate) end` | function |
| `Moss_AudioStream3DSetPan` | `stream:set_pan(...)` | `function stream:set_pan(pan) end` | function |
| `Moss_AudioStream3DSetLoop` | `stream:set_loop(...)` | `function stream:set_loop(loop) end` | function |
| `Moss_AudioStream3DSetPosition` (overload 2) | `stream:set_position(...)` | `function stream:set_position() end` | function |
| `Moss_AudioStream3DSetVelocity` (overload 2) | `stream:set_velocity(...)` | `function stream:set_velocity() end` | function |
| `Moss_AudioStream3DSetMaxDistance` (overload 2) | `stream:set_max_distance(...)` | `function stream:set_max_distance() end` | function |
| `Moss_AudioStream3DSetDistanceModel` | `stream:set_distance_model(...)` | `function stream:set_distance_model(model) end` | function |
| `Moss_AudioStream3DRemove` | `stream:close(...)` | `function stream:close() end` | function |
| `Moss_AudioCreateAudioListener2D` | `moss.audio.create_audio_listener_dimension2_d(...)` | `function moss.audio.create_audio_listener_dimension2_d(position) end` | function |
| `Moss_AudioCreateAudioListener3D` | `moss.audio.create_audio_listener_dimension3_d(...)` | `function moss.audio.create_audio_listener_dimension3_d(position) end` | function |
| `Moss_AudioCreateRayAudioListener2D` | `moss.audio.create_ray_audio_listener_dimension2_d(...)` | `function moss.audio.create_ray_audio_listener_dimension2_d(position) end` | function |
| `Moss_AudioCreateRayAudioListener3D` | `moss.audio.create_ray_audio_listener_dimension3_d(...)` | `function moss.audio.create_ray_audio_listener_dimension3_d(position) end` | function |
| `Moss_AudioRemoveAudioListener2D` | `listener:close(...)` | `function listener:close() end` | function |
| `Moss_AudioRemoveAudioListener3D` | `listener:close(...)` | `function listener:close() end` | function |
| `Moss_AudioRemoveRayAudioListener2D` | `listener:close(...)` | `function listener:close() end` | function |
| `Moss_AudioRemoveRayAudioListener3D` | `listener:close(...)` | `function listener:close() end` | function |
| `Moss_AudioActivateAudioListener2D` | `listener:activate(...)` | `function listener:activate(activate) end` | function |
| `Moss_AudioActivateAudioListener3D` | `listener:activate(...)` | `function listener:activate(activate) end` | function |
| `Moss_AudioActivateRayAudioListener2D` | `listener:activate(...)` | `function listener:activate(activate) end` | function |
| `Moss_AudioActivateRayAudioListener3D` | `listener:activate(...)` | `function listener:activate(activate) end` | function |
| `Moss_AudioListenerSetOrientation` | `listener:listener_set_orientation(...)` | `function listener:listener_set_orientation(forward, up) end` | function |
| `Moss_AudioRayListener2DSetPhysics` | `listener:ray_set_physics(...)` | `function listener:ray_set_physics(physics, raycast) end` | function |
| `Moss_AudioRayListener3DSetPhysics` | `listener:ray_set_physics(...)` | `function listener:ray_set_physics(physics, raycast) end` | function |
| `Moss_AudioRayListener2DSetTraceSettings` | `listener:ray_set_trace_settings(...)` | `function listener:ray_set_trace_settings(max_distance, reflection_rays) end` | function |
| `Moss_AudioRayListener3DSetTraceSettings` | `listener:ray_set_trace_settings(...)` | `function listener:ray_set_trace_settings(max_distance, reflection_rays) end` | function |
| `Moss_AudioRayTrace2D` | `moss.audio.ray_trace_dimension2_d(...)` | `function moss.audio.ray_trace_dimension2_d(desc, out_result) end` | function |
| `Moss_AudioRayTrace3D` | `moss.audio.ray_trace_dimension3_d(...)` | `function moss.audio.ray_trace_dimension3_d(desc, out_result) end` | function |
| `Moss_AudioRayTraceFromListener2D` | `listener:ray_trace_from(...)` | `function listener:ray_trace_from(source_position, out_result) end` | function |
| `Moss_AudioRayTraceFromListener3D` | `listener:ray_trace_from(...)` | `function listener:ray_trace_from(source_position, out_result) end` | function |
| `Moss_AudioRayTraceComputeGain` | `result:ray_trace_compute_gain(...)` | `function result:ray_trace_compute_gain() end` | function |
| `Moss_IsSpeakerDeviceReady` | `moss.audio.is_speaker_device_ready(...)` | `function moss.audio.is_speaker_device_ready() end` | function |
| `Moss_AudioSpeakerOpen` | `moss.audio.speaker_open(...)` | `function moss.audio.speaker_open() end` | function |
| `Moss_AudioSpeakerPause` | `moss.audio.speaker_pause(...)` | `function moss.audio.speaker_pause() end` | function |
| `Moss_AudioSpeakerResume` | `moss.audio.speaker_resume(...)` | `function moss.audio.speaker_resume() end` | function |
| `Moss_AudioSpeakerIsPaused` | `moss.audio.speaker_is_paused(...)` | `function moss.audio.speaker_is_paused() end` | function |
| `Moss_AudioSelectSpeakerDevice` | `moss.audio.select_speaker_device(...)` | `function moss.audio.select_speaker_device(id) end` | function |
| `Moss_GetCurrentSpeakerDeviceID` | `moss.audio.get_current_speaker_device_id(...)` | `function moss.audio.get_current_speaker_device_id() end` | function |
| `Moss_GetSpeakerDeviceName` | `moss.audio.get_speaker_device_name(...)` | `function moss.audio.get_speaker_device_name(id) end` | function |
| `Moss_ListSpeakerDevices` | `moss.audio.list_speaker_devices(...)` | `function moss.audio.list_speaker_devices() end` | function |
| `Moss_MicrophoneGetDeviceCount` | `moss.audio.microphone_get_device_count(...)` | `function moss.audio.microphone_get_device_count() end` | function |
| `Moss_MicrophoneGetDeviceName` | `moss.audio.microphone_get_device_name(...)` | `function moss.audio.microphone_get_device_name(device_index) end` | function |
| `Moss_MicrophoneOpen` | `moss.audio.microphone_open(...)` | `function moss.audio.microphone_open(desc) end` | function |
| `Moss_MicrophoneClose` | `microphone:close(...)` | `function microphone:close() end` | function |
| `Moss_MicrophoneStart` | `microphone:start(...)` | `function microphone:start() end` | function |
| `Moss_MicrophoneStop` | `microphone:stop(...)` | `function microphone:stop() end` | function |
| `Moss_MicrophoneRead` | `microphone:read(...)` | `function microphone:read(out_samples, max_frames) end` | function |
| `Moss_MicrophoneSetGain` | `microphone:set_gain(...)` | `function microphone:set_gain(gain) end` | function |
| `Moss_MicrophoneGetSampleRate` | `microphone:get_sample_rate(...)` | `function microphone:get_sample_rate() end` | function |
| `Moss_MicrophoneGetChannels` | `microphone:get_channels(...)` | `function microphone:get_channels() end` | function |
| `Moss_MicrophoneSetCallback` | `microphone:set_callback(...)` | `function microphone:set_callback(callback) end` | function |
| `Moss_MicrophoneGetLevels` | `microphone:get_levels(...)` | `function microphone:get_levels() end` | function |
| `Moss_MicrophoneGetLevelRMS` | `microphone:get_level_rms(...)` | `function microphone:get_level_rms() end` | function |
| `Moss_MicrophoneGetLevelPeak` | `microphone:get_level_peak(...)` | `function microphone:get_level_peak() end` | function |
| `Moss_MicrophoneGetSmoothedVolume` | `microphone:get_smoothed_volume(...)` | `function microphone:get_smoothed_volume() end` | function |
| `Moss_MicrophoneGetVoiceActivity` | `microphone:get_voice_activity(...)` | `function microphone:get_voice_activity() end` | function |
| `Moss_IsMicrophoneDeviceReady` | `moss.audio.is_microphone_device_ready(...)` | `function moss.audio.is_microphone_device_ready() end` | function |
| `Moss_AudioMicrophoneOpen` | `moss.audio.microphone_open(...)` | `function moss.audio.microphone_open() end` | function |
| `Moss_AudioMicrophoneClose` | `moss.audio.microphone_close(...)` | `function moss.audio.microphone_close() end` | function |
| `Moss_AudioMicrophonePlay` | `moss.audio.microphone_play(...)` | `function moss.audio.microphone_play() end` | function |
| `Moss_AudioMicrophoneStop` | `moss.audio.microphone_stop(...)` | `function moss.audio.microphone_stop() end` | function |
| `Moss_AudioMicrophoneID` | `moss.audio.microphone_id(...)` | `function moss.audio.microphone_id() end` | function |
| `Moss_AudioSelectMicrophoneDevice` | `moss.audio.select_microphone_device(...)` | `function moss.audio.select_microphone_device(id) end` | function |
| `Moss_GetMicrophoneDeviceName` | `moss.audio.get_microphone_device_name(...)` | `function moss.audio.get_microphone_device_name(index) end` | function |
| `Moss_ListMicrophoneDevices` | `moss.audio.list_microphone_devices(...)` | `function moss.audio.list_microphone_devices() end` | function |
| `Moss_AudioMicrophoneSetGain` | `microphone:set_gain(...)` | `function microphone:set_gain(gain) end` | function |
| `Moss_AudioMicrophoneGetSampleRate` | `microphone:get_sample_rate(...)` | `function microphone:get_sample_rate() end` | function |
| `Moss_AudioMicrophoneGetChannels` | `microphone:get_channels(...)` | `function microphone:get_channels() end` | function |
| `Moss_AudioStreamSetCallback` | `stream:set_callback(...)` | `function stream:set_callback(callback) end` | function |

### `Moss_GPU.h` → `moss.gpu`

| Native symbol (traceability) | Public API projection | Target-language signature | Kind |
|---|---|---|---|
| `Moss_AcquireGPUCommandBuffer` | `device:acquire_command_buffer(...)` | `function device:acquire_command_buffer() end` | function |
| `Moss_BeginGPUComputePass` | `cmd:begin_compute_pass(...)` | `function cmd:begin_compute_pass() end` | function |
| `Moss_BeginGPUCopyPass` | `cmd:begin_copy_pass(...)` | `function cmd:begin_copy_pass() end` | function |
| `Moss_CreateGPUBuffer` | `device:create_buffer(...)` | `function device:create_buffer(desc) end` | function |
| `Moss_DestroyGPUDevice` | `device:close(...)` | `function device:close() end` | function |
| `Moss_BeginGPURenderPass` | `cmd:begin_render_pass(...)` | `function cmd:begin_render_pass(framebuffer) end` | function |
| `Moss_BindGPUComputeSamplers` | `cmd:bind_compute_samplers(...)` | `function cmd:bind_compute_samplers(first_slot, samplers) end` | function |
| `Moss_BindGPUComputeTextures` | `cmd:bind_compute_textures(...)` | `function cmd:bind_compute_textures(first_slot, textures) end` | function |
| `Moss_BindGPUComputeUniformBuffers` | `cmd:bind_compute_uniform_buffers(...)` | `function cmd:bind_compute_uniform_buffers(first_slot, bindings) end` | function |
| `Moss_BindGPUFragmentSamplers` | `cmd:bind_fragment_samplers(...)` | `function cmd:bind_fragment_samplers(first_slot, samplers) end` | function |
| `Moss_BindGPUFragmentTextures` | `cmd:bind_fragment_textures(...)` | `function cmd:bind_fragment_textures(first_slot, textures) end` | function |
| `Moss_BindGPUFragmentUniformBuffers` | `cmd:bind_fragment_uniform_buffers(...)` | `function cmd:bind_fragment_uniform_buffers(first_slot, bindings) end` | function |
| `Moss_BindGPUIndexBuffer` | `cmd:bind_index(...)` | `function cmd:bind_index(buffer, offset, index_type) end` | function |
| `Moss_BindGPUPipeline` | `cmd:bind_pipeline(...)` | `function cmd:bind_pipeline(pipeline) end` | function |
| `Moss_BindGPUResourceSet` | `cmd:bind_resource_set(...)` | `function cmd:bind_resource_set(set_index, set) end` | function |
| `Moss_BindGPUVertexBuffers` | `cmd:bind_vertex_buffers(...)` | `function cmd:bind_vertex_buffers(first_binding, bindings) end` | function |
| `Moss_BindGPUVertexSamplers` | `cmd:bind_vertex_samplers(...)` | `function cmd:bind_vertex_samplers(first_slot, samplers) end` | function |
| `Moss_BindGPUVertexTextures` | `cmd:bind_vertex_textures(...)` | `function cmd:bind_vertex_textures(first_slot, textures) end` | function |
| `Moss_BindGPUVertexUniformBuffers` | `cmd:bind_vertex_uniform_buffers(...)` | `function cmd:bind_vertex_uniform_buffers(first_slot, bindings) end` | function |
| `Moss_BlitGPUTexture` | `cmd:blit_texture(...)` | `function cmd:blit_texture(src, dst, region) end` | function |
| `Moss_CalculateGPUTextureFormatSize` | `moss.gpu.calculate_texture_format_size(...)` | `function moss.gpu.calculate_texture_format_size(format, width, height, depth) end` | function |
| `Moss_CancelGPUCommandBuffer` | `cmd:cancel(...)` | `function cmd:cancel() end` | function |
| `Moss_CopyGPUBufferToBuffer` | `cmd:copy_to_buffer(...)` | `function cmd:copy_to_buffer(src, dst, src_region, dst_region) end` | function |
| `Moss_CopyGPUBufferToTexture` | `cmd:copy_to_texture(...)` | `function cmd:copy_to_texture(src, dst, src_region, dst_region) end` | function |
| `Moss_CopyGPUTextureToBuffer` | `cmd:copy_texture_to(...)` | `function cmd:copy_texture_to(src, dst, src_region, dst_region) end` | function |
| `Moss_CopyGPUTextureToTexture` | `cmd:copy_texture_to_texture(...)` | `function cmd:copy_texture_to_texture(src, dst, src_region, dst_region) end` | function |
| `Moss_CreateGPUSampler` | `device:create_sampler(...)` | `function device:create_sampler(create_info) end` | function |
| `Moss_CreateGPUShader` | `device:create_shader(...)` | `function device:create_shader(create_info) end` | function |
| `Moss_CreateGPUComputePipeline` | `device:create_compute_pipeline(...)` | `function device:create_compute_pipeline(create_info) end` | function |
| `Moss_CreateGPUTexture` | `device:create_texture(...)` | `function device:create_texture(create_info) end` | function |
| `Moss_CreateGPUTextureView` | `device:create_texture_view(...)` | `function device:create_texture_view(create_info) end` | function |
| `Moss_CreateGPUTransferBuffer` | `device:create_transfer_buffer(...)` | `function device:create_transfer_buffer(create_info) end` | function |
| `Moss_CreateGPUFramebuffer` | `device:create_framebuffer(...)` | `function device:create_framebuffer(create_info) end` | function |
| `Moss_CreateGPUResourceSetLayout` | `device:create_resource_set_layout(...)` | `function device:create_resource_set_layout(create_info) end` | function |
| `Moss_CreateGPUResourceSet` | `device:create_resource_set(...)` | `function device:create_resource_set(create_info) end` | function |
| `Moss_CreateGPUQueryPool` | `device:create_query_pool(...)` | `function device:create_query_pool(create_info) end` | function |
| `Moss_DispatchGPUCompute` | `cmd:dispatch_compute(...)` | `function cmd:dispatch_compute(group_count_x, group_count_y, group_count_z) end` | function |
| `Moss_DispatchGPUComputeIndirect` | `cmd:dispatch_compute_indirect(...)` | `function cmd:dispatch_compute_indirect(indirect_buffer, offset) end` | function |
| `Moss_DownloadFromGPUBuffer` | `src_buffer:download_from(...)` | `function src_buffer:download_from(dst_data, size, src_offset) end` | function |
| `Moss_DownloadFromGPUTexture` | `src_texture:download_from(...)` | `function src_texture:download_from(src_region, dst_data, dst_row_pitch, dst_slice_pitch) end` | function |
| `Moss_DrawGPUIndexedPrimitives` | `cmd:draw_indexed_primitives(...)` | `function cmd:draw_indexed_primitives(index_count, instance_count, first_index, vertex_offset, first_instance) end` | function |
| `Moss_DrawGPUIndexedPrimitivesIndirect` | `cmd:draw_indexed_primitives_indirect(...)` | `function cmd:draw_indexed_primitives_indirect(indirect_buffer, offset, draw_count, stride) end` | function |
| `Moss_DrawGPUPrimitives` | `cmd:draw_primitives(...)` | `function cmd:draw_primitives(vertex_count, instance_count, first_vertex, first_instance) end` | function |
| `Moss_DrawGPUPrimitivesIndirect` | `cmd:draw_primitives_indirect(...)` | `function cmd:draw_primitives_indirect(indirect_buffer, offset, draw_count, stride) end` | function |
| `Moss_EndGPUComputePass` | `cmd:end_compute_pass(...)` | `function cmd:end_compute_pass() end` | function |
| `Moss_EndGPUCopyPass` | `cmd:end_copy_pass(...)` | `function cmd:end_copy_pass() end` | function |
| `Moss_EndGPURenderPass` | `cmd:end_render_pass(...)` | `function cmd:end_render_pass() end` | function |
| `Moss_GDKResumeGPU` | `device:gdk_resume(...)` | `function device:gdk_resume() end` | function |
| `Moss_GDKSuspendGPU` | `device:gdk_suspend(...)` | `function device:gdk_suspend() end` | function |
| `Moss_GenerateMipmapsForGPUTexture` | `cmd:generate_mipmaps_for_texture(...)` | `function cmd:generate_mipmaps_for_texture(texture) end` | function |
| `Moss_GetGPUDriver` | `moss.gpu.get_driver(...)` | `function moss.gpu.get_driver(driver_index) end` | function |
| `Moss_GetCompiledGPUBackendType` | `moss.gpu.get_compiled_backend_type(...)` | `function moss.gpu.get_compiled_backend_type() end` | function |
| `Moss_GetGPUDeviceBackendType` | `device:get_backend_type(...)` | `function device:get_backend_type() end` | function |
| `Moss_GPUBackendGetName` | `moss.gpu.backend_get_name(...)` | `function moss.gpu.backend_get_name(backend) end` | function |
| `Moss_GPUBackendGetShaderFormats` | `moss.gpu.backend_get_shader_formats(...)` | `function moss.gpu.backend_get_shader_formats(backend) end` | function |
| `Moss_GPUBackendIsCompiled` | `moss.gpu.backend_is_compiled(...)` | `function moss.gpu.backend_is_compiled(backend) end` | function |
| `Moss_GPUBackendIsSupportedOnPlatform` | `moss.gpu.backend_is_supported_on_platform(...)` | `function moss.gpu.backend_is_supported_on_platform(backend) end` | function |
| `Moss_GetGPUBackendInfo` | `moss.gpu.get_backend_info(...)` | `function moss.gpu.get_backend_info(backend, out_info) end` | function |
| `Moss_RegisterNativeGPUBackend` | `moss.gpu.register_native_backend(...)` | `function moss.gpu.register_native_backend(backend, native_table) end` | function |
| `Moss_HasNativeGPUBackend` | `moss.gpu.has_native_backend(...)` | `function moss.gpu.has_native_backend(backend) end` | function |
| `Moss_GetGPUShaderFormats` | `device:get_shader_formats(...)` | `function device:get_shader_formats() end` | function |
| `Moss_GetGPUTextureFormatFromPixelFormat` | `moss.gpu.get_texture_format_from_pixel_format(...)` | `function moss.gpu.get_texture_format_from_pixel_format(pixel_format) end` | function |
| `Moss_GetNumGPUDrivers` | `moss.gpu.get_num_drivers(...)` | `function moss.gpu.get_num_drivers() end` | function |
| `Moss_GetPixelFormatFromGPUTextureFormat` | `moss.gpu.get_pixel_format_from_texture_format(...)` | `function moss.gpu.get_pixel_format_from_texture_format(texture_format) end` | function |
| `Moss_GPUSupportsProperties` | `device:supports_properties(...)` | `function device:supports_properties() end` | function |
| `Moss_GPUSupportsShaderFormats` | `device:supports_shader_formats(...)` | `function device:supports_shader_formats(shader_format_mask) end` | function |
| `Moss_GPUTextureFormatTexelBlockSize` | `moss.gpu.texture_format_texel_block_size(...)` | `function moss.gpu.texture_format_texel_block_size(format) end` | function |
| `Moss_GPUTextureSupportsFormat` | `device:texture_supports_format(...)` | `function device:texture_supports_format(format, usage) end` | function |
| `Moss_GPUTextureSupportsSampleCount` | `device:texture_supports_sample_count(...)` | `function device:texture_supports_sample_count(format, sample_count) end` | function |
| `Moss_InsertGPUDebugLabel` | `cmd:insert_debug_label(...)` | `function cmd:insert_debug_label(label) end` | function |
| `Moss_MapGPUTransferBuffer` | `transfer_buffer:map(...)` | `function transfer_buffer:map() end` | function |
| `Moss_PopGPUDebugGroup` | `cmd:pop_debug_group(...)` | `function cmd:pop_debug_group() end` | function |
| `Moss_PushGPUComputeUniformData` | `cmd:push_compute_uniform_data(...)` | `function cmd:push_compute_uniform_data(slot, data, size) end` | function |
| `Moss_PushGPUDebugGroup` | `cmd:push_debug_group(...)` | `function cmd:push_debug_group(label) end` | function |
| `Moss_PushGPUFragmentUniformData` | `cmd:push_fragment_uniform_data(...)` | `function cmd:push_fragment_uniform_data(slot, data, size) end` | function |
| `Moss_PushGPUVertexUniformData` | `cmd:push_vertex_uniform_data(...)` | `function cmd:push_vertex_uniform_data(slot, data, size) end` | function |
| `Moss_QueryGPUFence` | `fence:query(...)` | `function fence:query() end` | function |
| `Moss_ReleaseGPUBuffer` | `buffer:release(...)` | `function buffer:release() end` | function |
| `Moss_ReleaseGPUFence` | `fence:release(...)` | `function fence:release() end` | function |
| `Moss_ReleaseGPUSampler` | `sampler:release(...)` | `function sampler:release() end` | function |
| `Moss_ReleaseGPUShader` | `shader:release(...)` | `function shader:release() end` | function |
| `Moss_ReleaseGPUComputePipeline` | `device:release_compute_pipeline(...)` | `function device:release_compute_pipeline(pipeline) end` | function |
| `Moss_ReleaseGPUTexture` | `texture:release(...)` | `function texture:release() end` | function |
| `Moss_ReleaseGPUTextureView` | `texture_view:release(...)` | `function texture_view:release() end` | function |
| `Moss_ReleaseGPUTransferBuffer` | `transfer_buffer:release(...)` | `function transfer_buffer:release() end` | function |
| `Moss_ReleaseGPUFramebuffer` | `framebuffer:release(...)` | `function framebuffer:release() end` | function |
| `Moss_ReleaseGPUResourceSetLayout` | `layout:release(...)` | `function layout:release() end` | function |
| `Moss_ReleaseGPUResourceSet` | `set:release(...)` | `function set:release() end` | function |
| `Moss_ReleaseGPUQueryPool` | `query_pool:release(...)` | `function query_pool:release() end` | function |
| `Moss_SetGPUAllowedFramesInFlight` | `device:set_allowed_frames_in_flight(...)` | `function device:set_allowed_frames_in_flight(frames_in_flight) end` | function |
| `Moss_SetGPUBlendConstants` | `cmd:set_blend_constants(...)` | `function cmd:set_blend_constants(r, g, b, a) end` | function |
| `Moss_SetGPUBufferName` | `buffer:set_name(...)` | `function buffer:set_name(name) end` | function |
| `Moss_SetGPUScissor` (overload 1) | `cmd:set_scissor(...)` | `function cmd:set_scissor(scissor) end` | function |
| `Moss_SetGPUStencilReference` | `cmd:set_stencil_reference(...)` | `function cmd:set_stencil_reference(reference) end` | function |
| `Moss_SetGPUTextureName` | `texture:set_name(...)` | `function texture:set_name(name) end` | function |
| `Moss_SetGPUViewport` | `cmd:set_viewport(...)` | `function cmd:set_viewport(viewport) end` | function |
| `Moss_SetGPUScissor` (overload 2) | `cmd:set_scissor(...)` | `function cmd:set_scissor(scissors) end` | function |
| `Moss_SubmitGPUCommandBuffer` | `cmd:submit(...)` | `function cmd:submit() end` | function |
| `Moss_SubmitGPUCommandBufferAndAcquireFence` | `cmd:submit_and_acquire_fence(...)` | `function cmd:submit_and_acquire_fence() end` | function |
| `Moss_BindGPUComputePipeline` | `cmd:bind_compute_pipeline(...)` | `function cmd:bind_compute_pipeline(pipeline) end` | function |
| `Moss_BarrierGPUResources` | `cmd:barrier_resources(...)` | `function cmd:barrier_resources(barrier_info) end` | function |
| `Moss_ResetGPUQueryPool` | `cmd:reset_query_pool(...)` | `function cmd:reset_query_pool(query_pool, first_query, query_count) end` | function |
| `Moss_WriteGPUTimestamp` | `cmd:write_timestamp(...)` | `function cmd:write_timestamp(query_pool, query_index) end` | function |
| `Moss_BeginGPUQuery` | `cmd:begin_query(...)` | `function cmd:begin_query(query_pool, query_index) end` | function |
| `Moss_EndGPUQuery` | `cmd:end_query(...)` | `function cmd:end_query(query_pool, query_index) end` | function |
| `Moss_GetGPUQueryResults` | `query_pool:get_query_results(...)` | `function query_pool:get_query_results(first_query, query_count, data, data_size, stride, flags) end` | function |
| `Moss_TransitionGPUBuffer` | `cmd:transition(...)` | `function cmd:transition(buffer, old_state, new_state) end` | function |
| `Moss_TransitionGPUTexture` | `cmd:transition_texture(...)` | `function cmd:transition_texture(texture, old_state, new_state) end` | function |
| `Moss_UnmapGPUTransferBuffer` | `transfer_buffer:unmap(...)` | `function transfer_buffer:unmap() end` | function |
| `Moss_UploadToGPUBuffer` | `dst_buffer:upload_to(...)` | `function dst_buffer:upload_to(src_data, size, dst_offset) end` | function |
| `Moss_UploadToGPUTexture` | `dst_texture:upload_to(...)` | `function dst_texture:upload_to(transfer_info) end` | function |
| `Moss_WaitForGPUFences` | `device:wait_for_fences(...)` | `function device:wait_for_fences(fences, wait_all, timeout_ns) end` | function |
| `Moss_WaitForGPUIdle` | `device:wait_for_idle(...)` | `function device:wait_for_idle() end` | function |
| `Moss_WindowSupportsGPUPresentMode` | `window:supports_present_mode(...)` | `function window:supports_present_mode(present_mode) end` | function |
| `Moss_GPUBlitInfo` | `moss.gpu.new_blit_info(...)` | `function moss.gpu.new_blit_info() -> GpuBlitInfo` | constructor |
| `Moss_GPUBlitRegion` | `moss.gpu.new_blit_region(...)` | `function moss.gpu.new_blit_region() -> GpuBlitRegion` | constructor |
| `Moss_GPUBufferBinding` | `moss.gpu.new_buffer_binding(...)` | `function moss.gpu.new_buffer_binding() -> GpuBufferBinding` | constructor |
| `Moss_GPUBufferCreateInfo` | `moss.gpu.new_buffer_create_info(...)` | `function moss.gpu.new_buffer_create_info() -> GpuBufferCreateInfo` | constructor |
| `Moss_GPUBufferLocation` | `moss.gpu.new_buffer_location(...)` | `function moss.gpu.new_buffer_location() -> GpuBufferLocation` | constructor |
| `Moss_GPUBufferRegion` | `moss.gpu.new_buffer_region(...)` | `function moss.gpu.new_buffer_region() -> GpuBufferRegion` | constructor |
| `Moss_GPUColorTargetBlendState` | `moss.gpu.new_color_target_blend_state(...)` | `function moss.gpu.new_color_target_blend_state() -> GpuColorTargetBlendState` | constructor |
| `Moss_GPUColorTargetDescription` | `moss.gpu.new_color_target_description(...)` | `function moss.gpu.new_color_target_description() -> GpuColorTargetDescription` | constructor |
| `Moss_GPUColorTargetInfo` | `moss.gpu.new_color_target_info(...)` | `function moss.gpu.new_color_target_info() -> GpuColorTargetInfo` | constructor |
| `Moss_GPUDepthStencilState` | `moss.gpu.new_depth_stencil_state(...)` | `function moss.gpu.new_depth_stencil_state() -> GpuDepthStencilState` | constructor |
| `Moss_GPUDepthStencilTargetInfo` | `moss.gpu.new_depth_stencil_target_info(...)` | `function moss.gpu.new_depth_stencil_target_info() -> GpuDepthStencilTargetInfo` | constructor |
| `Moss_GPUIndexedIndirectDrawCommand` | `moss.gpu.new_indexed_indirect_draw_command(...)` | `function moss.gpu.new_indexed_indirect_draw_command() -> GpuIndexedIndirectDrawCommand` | constructor |
| `Moss_GPUIndirectDispatchCommand` | `moss.gpu.new_indirect_dispatch_command(...)` | `function moss.gpu.new_indirect_dispatch_command() -> GpuIndirectDispatchCommand` | constructor |
| `Moss_GPUIndirectDrawCommand` | `moss.gpu.new_indirect_draw_command(...)` | `function moss.gpu.new_indirect_draw_command() -> GpuIndirectDrawCommand` | constructor |
| `Moss_GPUMultisampleState` | `moss.gpu.new_multisample_state(...)` | `function moss.gpu.new_multisample_state() -> GpuMultisampleState` | constructor |
| `Moss_GPURasterizerState` | `moss.gpu.new_rasterizer_state(...)` | `function moss.gpu.new_rasterizer_state() -> GpuRasterizerState` | constructor |
| `Moss_GPUSamplerCreateInfo` | `moss.gpu.new_sampler_create_info(...)` | `function moss.gpu.new_sampler_create_info() -> GpuSamplerCreateInfo` | constructor |
| `Moss_GPUShaderCreateInfo` | `moss.gpu.new_shader_create_info(...)` | `function moss.gpu.new_shader_create_info() -> GpuShaderCreateInfo` | constructor |
| `Moss_GPUStencilOpState` | `moss.gpu.new_stencil_op_state(...)` | `function moss.gpu.new_stencil_op_state() -> GpuStencilOpState` | constructor |
| `Moss_GPUTextureCreateInfo` | `moss.gpu.new_texture_create_info(...)` | `function moss.gpu.new_texture_create_info() -> GpuTextureCreateInfo` | constructor |
| `Moss_GPUTextureLocation` | `moss.gpu.new_texture_location(...)` | `function moss.gpu.new_texture_location() -> GpuTextureLocation` | constructor |
| `Moss_GPUTextureRegion` | `moss.gpu.new_texture_region(...)` | `function moss.gpu.new_texture_region() -> GpuTextureRegion` | constructor |
| `Moss_GPUTextureSamplerBinding` | `moss.gpu.new_texture_sampler_binding(...)` | `function moss.gpu.new_texture_sampler_binding() -> GpuTextureSamplerBinding` | constructor |
| `Moss_GPUTextureTransferInfo` | `moss.gpu.new_texture_transfer_info(...)` | `function moss.gpu.new_texture_transfer_info() -> GpuTextureTransferInfo` | constructor |
| `Moss_GPUTransferBufferCreateInfo` | `moss.gpu.new_transfer_buffer_create_info(...)` | `function moss.gpu.new_transfer_buffer_create_info() -> GpuTransferBufferCreateInfo` | constructor |
| `Moss_GPUTransferBufferLocation` | `moss.gpu.new_transfer_buffer_location(...)` | `function moss.gpu.new_transfer_buffer_location() -> GpuTransferBufferLocation` | constructor |
| `Moss_GPUVertexAttribute` | `moss.gpu.new_vertex_attribute(...)` | `function moss.gpu.new_vertex_attribute() -> GpuVertexAttribute` | constructor |
| `Moss_GPUVertexBufferDescription` | `moss.gpu.new_vertex_buffer_description(...)` | `function moss.gpu.new_vertex_buffer_description() -> GpuVertexBufferDescription` | constructor |
| `Moss_GPUVertexInputState` | `moss.gpu.new_vertex_input_state(...)` | `function moss.gpu.new_vertex_input_state() -> GpuVertexInputState` | constructor |
| `Moss_GPUViewport` | `moss.gpu.new_viewport(...)` | `function moss.gpu.new_viewport() -> GpuViewport` | constructor |
| `Moss_BindGPUComputeStorageBuffers` | `cmd:bind_compute_storage_buffers(...)` | `function commands:bind_compute_storage_buffers(first_slot, bindings) end` | function |
| `Moss_BindGPUComputeStorageTextures` | `cmd:bind_compute_storage_textures(...)` | `function cmd:bind_compute_storage_textures(first_slot, bindings) end` | function |
| `Moss_BindGPUFragmentStorageBuffers` | `cmd:bind_fragment_storage_buffers(...)` | `function cmd:bind_fragment_storage_buffers(first_slot, bindings) end` | function |
| `Moss_BindGPUFragmentStorageTextures` | `cmd:bind_fragment_storage_textures(...)` | `function cmd:bind_fragment_storage_textures(first_slot, bindings) end` | function |
| `Moss_BindGPUVertexStorageBuffers` | `cmd:bind_vertex_storage_buffers(...)` | `function cmd:bind_vertex_storage_buffers(first_slot, bindings) end` | function |
| `Moss_BindGPUVertexStorageTextures` | `cmd:bind_vertex_storage_textures(...)` | `function cmd:bind_vertex_storage_textures(first_slot, bindings) end` | function |
| `Moss_GetGPUDeviceDriver` | `device:get_driver(...)` | `function device:get_driver() end` | function |
| `Moss_GetGPUDeviceProperties` | `device:get_properties(...)` | `function device:get_properties() end` | function |
| `Moss_ReleaseWindowFromGPUDevice` | `device:release_window_from(...)` | `function device:release_window_from(window) end` | function |
| `Moss_ClaimWindowForGPUDevice` | `device:claim_window_for(...)` | `function device:claim_window_for(window) end` | function |
| `Moss_UpdateGPUResourceSet` | `set:update(...)` | `function set:update(bindings) end` | function |
| `Moss_CreateGPUDevice` | `moss.gpu.create_device(...)` | `moss.gpu.create_device(descriptor) -> GpuDevice \| nil, error?` | function |
| `Moss_CreateGPUDeviceWithProperties` | `moss.gpu.create_device_with_properties(...)` | `moss.gpu.create_device_with_properties(descriptor, requested_properties) -> GpuDevice \| nil, error?` | function |
| `Moss_GPUBufferCreate` | `device:create_buffer(...)` | `function device:create_buffer(descriptor) -> GpuBuffer \| nil, error? end` | function |
| `Moss_GPUBufferUpload` | `buffer:upload(...)` | `function buffer:upload(data, offset) end` | function |
| `Moss_GPUBufferMap` | `buffer:map(...)` | `function buffer:map() end` | function |
| `Moss_GPUBufferUnmap` | `buffer:unmap(...)` | `function buffer:unmap() end` | function |
| `Moss_GPUBufferDestroy` | `buffer:close(...)` | `function buffer:close() end` | function |
| `Moss_AcquireGPUSwapchainTexture` | `texture:acquire_swapchain(...)` | `function texture:acquire_swapchain(window) end` | function |
| `Moss_AcquireGPUSwapchainTextureStatus` | `texture:acquire_swapchain_status(...)` | `function texture:acquire_swapchain_status(window) end` | function |
| `Moss_GetGPUSwapchainTextureFormat` | `device:get_swapchain_texture_format(...)` | `function device:get_swapchain_texture_format(window) end` | function |
| `Moss_SetGPUSwapchainParameters` | `device:set_swapchain_parameters(...)` | `function device:set_swapchain_parameters(window, width, height, present_mode) end` | function |
| `Moss_ResizeGPUSwapchain` | `device:resize_swapchain(...)` | `function device:resize_swapchain(window, width, height) end` | function |
| `Moss_WaitAndAcquireGPUSwapchainTexture` | `texture:wait_and_acquire_swapchain(...)` | `function texture:wait_and_acquire_swapchain(window, timeout_ns) end` | function |
| `Moss_WaitAndAcquireGPUSwapchainTextureStatus` | `texture:wait_and_acquire_swapchain_status(...)` | `function texture:wait_and_acquire_swapchain_status(window, timeout_ns) end` | function |
| `Moss_WaitForGPUSwapchain` | `device:wait_for_swapchain(...)` | `function device:wait_for_swapchain(window, timeout_ns) end` | function |
| `Moss_WindowSupportsGPUSwapchainComposition` | `window:supports_swapchain_composition(...)` | `function window:supports_swapchain_composition() end` | function |
| `Moss_PresentGPUSwapchain` | `device:present_swapchain(...)` | `function device:present_swapchain(window, texture) end` | function |
| `Moss_PresentGPUSwapchainStatus` | `device:present_swapchain_status(...)` | `function device:present_swapchain_status(window, texture) end` | function |
| `Moss_GPUClear` | `cmd:clear(...)` | `function cmd:clear(r, g, b, a) end` | function |
| `Moss_GPUSamplerCreate` | `renderer:create_sampler(...)` | `function renderer:create_sampler(desc) end` | function |
| `Moss_GPUSamplerDestroy` | `sampler:close(...)` | `function sampler:close() end` | function |
| `Moss_TextureCreate` | `device:create_texture(...)` | `function device:create_texture(desc) end` | function |
| `Moss_TextureDestroy` | `texture:close(...)` | `function texture:close() end` | function |
| `Moss_TextureUpload` | `texture:upload(...)` | `function texture:upload(desc) end` | function |
| `Moss_TextureGenerateMips` | `texture:generate_mips(...)` | `function texture:generate_mips() end` | function |
| `Moss_GPUGetExternalFeatureMask` | `moss.gpu.get_external_feature_mask(...)` | `function moss.gpu.get_external_feature_mask() end` | function |
| `Moss_GPUExternalFeatureAvailable` | `moss.gpu.external_feature_available(...)` | `function moss.gpu.external_feature_available(feature) end` | function |
| `Moss_GPUGetExternalFeatures` | `moss.gpu.get_external_features(...)` | `function moss.gpu.get_external_features(out_features) end` | function |
| `Moss_GPUExternalFeatureName` | `moss.gpu.external_feature_name(...)` | `function moss.gpu.external_feature_name(feature) end` | function |
| `Moss_FSR1BuildEasuConstants` | `moss.gpu.fsr1_build_easu_constants(...)` | `function moss.gpu.fsr1_build_easu_constants(out_constants16, input_viewport_width, input_viewport_height, input_width, input_height, output_width, output_height) end` | function |
| `Moss_FSR2GetUpscaleRatio` | `moss.gpu.fsr2_get_upscale_ratio(...)` | `function moss.gpu.fsr2_get_upscale_ratio(quality) end` | function |
| `Moss_FSR2GetRenderResolution` | `moss.gpu.fsr2_get_render_resolution(...)` | `function moss.gpu.fsr2_get_render_resolution(display_width, display_height, quality, out_resolution) end` | function |
| `Moss_FSR2GetJitterPhaseCount` | `moss.gpu.fsr2_get_jitter_phase_count(...)` | `function moss.gpu.fsr2_get_jitter_phase_count(render_width, display_width) end` | function |
| `Moss_FSR2GetJitterOffset` | `moss.gpu.fsr2_get_jitter_offset(...)` | `function moss.gpu.fsr2_get_jitter_offset(out_x, out_y, index, phase_count) end` | function |
| `Moss_MeshGenerateVertexRemap` | `moss.gpu.mesh_generate_vertex_remap(...)` | `function moss.gpu.mesh_generate_vertex_remap(destination, indices, vertices, vertex_count, vertex_size) end` | function |
| `Moss_MeshOptimizeVertexCache` | `moss.gpu.mesh_optimize_vertex_cache(...)` | `function moss.gpu.mesh_optimize_vertex_cache(destination, indices, vertex_count) end` | function |
| `Moss_MeshOptimizeOverdraw` | `moss.gpu.mesh_optimize_overdraw(...)` | `function moss.gpu.mesh_optimize_overdraw(destination, indices, vertex_positions, vertex_positions_stride, threshold) end` | function |
| `Moss_MeshOptimizeVertexFetch` | `moss.gpu.mesh_optimize_vertex_fetch(...)` | `function moss.gpu.mesh_optimize_vertex_fetch(destination, indices, index_count, vertices, vertex_count, vertex_size) end` | function |
| `Moss_SMOLVEncode` | `moss.gpu.smolv_encode(...)` | `function moss.gpu.smolv_encode(spirv_data, spirv_size, out_smolv_data, inout_smolv_size, flags) end` | function |
| `Moss_SMOLVGetDecodedSize` | `moss.gpu.smolv_get_decoded_size(...)` | `function moss.gpu.smolv_get_decoded_size(smolv_data, smolv_size) end` | function |
| `Moss_SMOLVDecode` | `moss.gpu.smolv_decode(...)` | `function moss.gpu.smolv_decode(smolv_data, smolv_size, out_spirv_data, spirv_size, flags) end` | function |
| `Moss_GPUVulkanMemoryAllocatorAvailable` | `moss.gpu.memory_allocator_available(...)` | `function moss.gpu.memory_allocator_available() end` | function |
| `Moss_GPUClothSolverCreate` | `device:create_cloth_solver(...)` | `function device:create_cloth_solver(desc) end` | function |
| `Moss_GPUClothSolverDestroy` | `solver:close(...)` | `function solver:close() end` | function |
| `Moss_GPUClothSolverDispatch` | `cmd:cloth_solver_dispatch(...)` | `function cmd:cloth_solver_dispatch(solver, desc) end` | function |
| `Moss_GPUCommandBufferBegin` | `device:create_command_buffer(...)` | `function device:create_command_buffer(queue) -> GpuCommandBuffer \| nil, error? end` | function |
| `Moss_GPUCommandBufferEnd` | `cmd:end(...)` | `function cmd:end() end` | function |
| `Moss_GPUCommandBufferSubmit` | `cmd:submit(...)` | `function cmd:submit() end` | function |
| `Moss_CmdBeginRenderPass` | `cmd:cmd_begin_render_pass(...)` | `function cmd:cmd_begin_render_pass(fb) end` | function |
| `Moss_CmdEndRenderPass` | `cmd:cmd_end_render_pass(...)` | `function cmd:cmd_end_render_pass() end` | function |
| `Moss_CmdBindPipeline` | `cmd:cmd_bind_pipeline(...)` | `function cmd:cmd_bind_pipeline(pipeline) end` | function |
| `Moss_CmdBindResourceSet` | `cmd:cmd_bind_resource_set(...)` | `function cmd:cmd_bind_resource_set(set_index, set) end` | function |
| `Moss_CmdBindVertexBuffer` | `cmd:cmd_bind_vertex(...)` | `function cmd:cmd_bind_vertex(buffer, offset) end` | function |
| `Moss_CmdBindIndexBuffer` | `cmd:cmd_bind_index(...)` | `function cmd:cmd_bind_index(buffer, offset) end` | function |
| `Moss_CmdDraw` | `cmd:cmd_draw(...)` | `function cmd:cmd_draw(vertex_count, first_vertex) end` | function |
| `Moss_CmdDrawIndexed` | `cmd:cmd_draw_indexed(...)` | `function cmd:cmd_draw_indexed(index_count, first_index, vertex_offset) end` | function |
| `Moss_CmdDispatch` | `cmd:cmd_dispatch(...)` | `function cmd:cmd_dispatch(x, y, z) end` | function |
| `Moss_RGPassReadTexture` | `pass:read_texture(...)` | `function pass:read_texture(texture, state) end` | function |
| `Moss_RGPassWriteTexture` | `pass:write_texture(...)` | `function pass:write_texture(texture, state) end` | function |
| `Moss_GPUDeviceSupportsAsyncCompute` | `renderer:supports_async_compute(...)` | `function renderer:supports_async_compute() end` | function |
| `Moss_TextureSetResidency` | `texture:set_residency(...)` | `function texture:set_residency(state) end` | function |
| `Moss_ShaderCreate` | `device:create_shader(...)` | `function device:create_shader(desc) end` | function |
| `Moss_ShaderDestroy` | `shader:close(...)` | `function shader:close() end` | function |
| `Moss_BindlessRegisterTexture` | `renderer:bindless_register_texture(...)` | `function renderer:bindless_register_texture(texture) end` | function |
| `Moss_BindlessRegisterBuffer` | `buffer:bindless_register(...)` | `function buffer:bindless_register() end` | function |
| `Moss_BindlessUnregister` | `renderer:bindless_unregister(...)` | `function renderer:bindless_unregister(handle) end` | function |
| `Moss_PipelineCreate` | `device:create_pipeline(...)` | `function device:create_pipeline(desc) end` | function |
| `Moss_PipelineDestroy` | `device:pipeline_destroy(...)` | `function device:pipeline_destroy(pipeline) end` | function |
| `Moss_WindowResizeVK` | `moss.gpu.window_resize_vk(...)` | `function moss.gpu.window_resize_vk(physical_device, device, swapchain, swapchain_extent, swapchain_framebuffers, swapchain_image_views, depth_image, depth_image_memory, depth_image_view) end` | function |
| `Moss_GPUFatalError` | `moss.gpu.fatal_error(...)` | `function moss.gpu.fatal_error(result) end` | function |
| `Moss_SetGPUViewports` | `cmd:set_viewports(...)` | `function cmd:set_viewports(viewports) end` | function |
| `Moss_SetGPUScissors` | `cmd:set_scissors(...)` | `function cmd:set_scissors(scissors) end` | function |
| `Moss_ClearGPUTexture` | `cmd:clear_texture(...)` | `function cmd:clear_texture(texture, color) end` | function |
| `Moss_ClearGPUDepthStencilTexture` | `cmd:clear_depth_stencil_texture(...)` | `function cmd:clear_depth_stencil_texture(texture, depth, stencil) end` | function |
| `Moss_DrawGPUIndexedPrimitivesIndirectCount` | `cmd:draw_indexed_primitives_indirect_count(...)` | `function cmd:draw_indexed_primitives_indirect_count(indirect_buffer, offset, count_buffer, count_offset, max_draw_count, stride) end` | function |
| `Moss_DrawGPUPrimitivesIndirectCount` | `cmd:draw_primitives_indirect_count(...)` | `function cmd:draw_primitives_indirect_count(indirect_buffer, offset, count_buffer, count_offset, max_draw_count, stride) end` | function |
| `Moss_SetGPUDepthBias` | `cmd:set_depth_bias(...)` | `function cmd:set_depth_bias(constant_factor, clamp, slope_factor) end` | function |
| `Moss_CreateGPUSemaphore` | `device:create_semaphore(...)` | `function device:create_semaphore() end` | function |
| `Moss_ReleaseGPUSemaphore` | `semaphore:release(...)` | `function semaphore:release() end` | function |
| `Moss_SignalGPUSemaphoreValue` | `semaphore:signal_value(...)` | `function semaphore:signal_value(value) end` | function |
| `Moss_WaitGPUSemaphoreValue` | `semaphore:wait_value(...)` | `function semaphore:wait_value(value, timeout_ns) end` | function |
| `Moss_SubmitGPUCommandBufferWithSemaphores` | `cmd:submit_with_semaphores(...)` | `function cmd:submit_with_semaphores(wait_semaphores, wait_values, signal_semaphores, signal_values) end` | function |
| `Moss_GetGPUBindlessCapacity` | `device:get_bindless_capacity(...)` | `function device:get_bindless_capacity(type) end` | function |
| `Moss_CreateGPUPipelineCache` | `device:create_pipeline_cache(...)` | `function device:create_pipeline_cache(initial_data, initial_size) end` | function |
| `Moss_GetGPUPipelineCacheData` | `cache:get_data(...)` | `function cache:get_data(out_data, out_size) end` | function |
| `Moss_ReleaseGPUPipelineCache` | `cache:release(...)` | `function cache:release() end` | function |

### `Moss_GUI.h` → `moss.gui`

| Native symbol (traceability) | Public API projection | Target-language signature | Kind |
|---|---|---|---|
| `CreateContext` | `moss.gui.create_context(...)` | `function moss.gui.create_context(shared_font_atlas) end` | function |
| `DestroyContext` | `ctx:close(...)` | `function ctx:close() end` | function |
| `GetCurrentContext` | `moss.gui.get_current_context(...)` | `function moss.gui.get_current_context() end` | function |
| `SetCurrentContext` | `ctx:set_current(...)` | `function ctx:set_current() end` | function |
| `BeginWindow` | `ui:begin_window(...)` | `function ui:begin_window(name, p_open, flags) end` | function |
| `EndWindow` | `ui:end_window(...)` | `function ui:end_window() end` | function |
| `BeginWindowChild` (overload 1) | `ui:begin_window_child(...)` | `function ui:begin_window_child(str_id, size, child_flags, window_flags) end` | function |
| `BeginWindowChild` (overload 2) | `ui:begin_window_child(...)` | `function ui:begin_window_child(id, size, child_flags, window_flags) end` | function |
| `EndWindowChild` | `ui:end_window_child(...)` | `function ui:end_window_child() end` | function |
| `BeginContainer` | `ui:begin_container(...)` | `function ui:begin_container(rect) end` | function |
| `EndContainer` | `ui:end_container(...)` | `function ui:end_container() end` | function |
| `BeginVContainer` | `ui:begin_v_container(...)` | `function ui:begin_v_container(rect, spacing) end` | function |
| `EndVContainer` | `ui:end_v_container(...)` | `function ui:end_v_container() end` | function |
| `BeginHContainer` | `ui:begin_h_container(...)` | `function ui:begin_h_container(rect, spacing) end` | function |
| `EndHContainer` | `ui:end_h_container(...)` | `function ui:end_h_container() end` | function |
| `BeginCenterContainer` | `ui:begin_center_container(...)` | `function ui:begin_center_container(rect) end` | function |
| `EndCenterContainer` | `ui:end_center_container(...)` | `function ui:end_center_container() end` | function |
| `BeginMarginContainer` | `ui:begin_margin_container(...)` | `function ui:begin_margin_container(left_margin, right_margin, top_margin, bottom_margin) end` | function |
| `EndMarginContainer` | `ui:end_margin_container(...)` | `function ui:end_margin_container() end` | function |
| `BeginVScrollContainer` | `ui:begin_v_scroll_container(...)` | `function ui:begin_v_scroll_container() end` | function |
| `EndVScrollContainer` | `ui:end_v_scroll_container(...)` | `function ui:end_v_scroll_container() end` | function |
| `BeginHScrollContainer` | `ui:begin_h_scroll_container(...)` | `function ui:begin_h_scroll_container() end` | function |
| `EndHScrollContainer` | `ui:end_h_scroll_container(...)` | `function ui:end_h_scroll_container() end` | function |
| `BeginGridContainer` | `ui:begin_grid_container(...)` | `function ui:begin_grid_container() end` | function |
| `EndGridContainer` | `ui:end_grid_container(...)` | `function ui:end_grid_container() end` | function |
| `ColorRect` | `ui:color_rect(...)` | `function ui:color_rect(color, size) end` | function |
| `Text` | `ui:text(...)` | `function ui:text(fmt, args) end` | function |
| `RichText` | `ui:rich_text(...)` | `function ui:rich_text(label) end` | function |
| `ProgressBar` | `ui:progress_bar(...)` | `function ui:progress_bar() end` | function |
| `TextLink` | `ui:text_link(...)` | `function ui:text_link(label) end` | function |
| `TextLinkOpenURL` | `ui:text_link_open_url(...)` | `function ui:text_link_open_url(label, url) end` | function |
| `Button` | `ui:button(...)` | `function ui:button(label, rect_ptr, vp_matrix, default_width, default_height) end` | function |
| `ToggleSwitch` | `ui:toggle_switch(...)` | `function ui:toggle_switch(active) end` | function |
| `RadioButton` | `ui:radio_button(...)` | `function ui:radio_button(label, active) end` | function |
| `Checkbox` | `ui:checkbox(...)` | `function ui:checkbox(label, value, rect_ptr, vp_matrix, default_size) end` | function |
| `SliderFloat` | `ui:slider_float(...)` | `function ui:slider_float(min_value, max_value, value) end` | function |
| `SliderDouble` | `ui:slider_double(...)` | `function ui:slider_double(min_value, max_value, value) end` | function |
| `SliderInt` | `ui:slider_int(...)` | `function ui:slider_int(min_value, max_value, value) end` | function |
| `SliderFloat2` | `ui:slider_float2(...)` | `function ui:slider_float2(min_value, max_value, value) end` | function |
| `SliderDouble2` | `ui:slider_double2(...)` | `function ui:slider_double2(min_value, max_value, value) end` | function |
| `SliderInt2` | `ui:slider_int2(...)` | `function ui:slider_int2(min_value, max_value, value) end` | function |
| `SliderFloat3` | `ui:slider_float3(...)` | `function ui:slider_float3(min_value, max_value, value) end` | function |
| `SliderDouble3` | `ui:slider_double3(...)` | `function ui:slider_double3(min_value, max_value, value) end` | function |
| `SliderInt3` | `ui:slider_int3(...)` | `function ui:slider_int3(min_value, max_value, value) end` | function |
| `SliderFloat4` | `ui:slider_float4(...)` | `function ui:slider_float4(min_value, max_value, value) end` | function |
| `SliderDouble4` | `ui:slider_double4(...)` | `function ui:slider_double4(min_value, max_value, value) end` | function |
| `SliderInt4` | `ui:slider_int4(...)` | `function ui:slider_int4(min_value, max_value, value) end` | function |
| `DragFloat` | `ui:drag_float(...)` | `function ui:drag_float(label, value, v_speed, v_min, v_max, format, flags) end` | function |
| `DragFloat2` | `ui:drag_float2(...)` | `function ui:drag_float2(label, value, v_speed, v_min, v_max, format, flags) end` | function |
| `DragFloat3` | `ui:drag_float3(...)` | `function ui:drag_float3(label, value, v_speed, v_min, v_max, format, flags) end` | function |
| `DragFloat4` | `ui:drag_float4(...)` | `function ui:drag_float4(label, value, v_speed, v_min, v_max, format, flags) end` | function |
| `DragFloatRange2` | `ui:drag_float_range2(...)` | `function ui:drag_float_range2(label, v_current_min, v_current_max, v_speed, v_min, v_max, format, format_max, flags) end` | function |
| `DragInt` | `ui:drag_int(...)` | `function ui:drag_int(label, v, v_speed, v_min, v_max, format, flags) end` | function |
| `DragInt2` | `ui:drag_int2(...)` | `function ui:drag_int2(label, v, v_speed, v_min, v_max, format, flags) end` | function |
| `DragInt3` | `ui:drag_int3(...)` | `function ui:drag_int3(label, v, v_speed, v_min, v_max, format, flags) end` | function |
| `DragInt4` | `ui:drag_int4(...)` | `function ui:drag_int4(label, v, v_speed, v_min, v_max, format, flags) end` | function |
| `DragIntRange2` | `ui:drag_int_range2(...)` | `function ui:drag_int_range2(label, v_current_min, v_current_max, v_speed, v_min, v_max, format, format_max, flags) end` | function |
| `DragScalar` | `ui:drag_scalar(...)` | `function ui:drag_scalar(label, data_type, p_data, v_speed, p_min, p_max, format, flags) end` | function |
| `DragScalarN` | `ui:drag_scalar_n(...)` | `function ui:drag_scalar_n(label, data_type, p_data, components, v_speed, p_min, p_max, format, flags) end` | function |
| `InputText` | `ui:input_text(...)` | `function ui:input_text(label, buf, buf_size, flags, callback) end` | function |
| `InputTextMultiline` | `ui:input_text_multiline(...)` | `function ui:input_text_multiline(label, buf, buf_size, size, flags, callback) end` | function |
| `InputTextWithHint` | `ui:input_text_with_hint(...)` | `function ui:input_text_with_hint(label, hint, buf, buf_size, flags, callback) end` | function |
| `InputFloat` | `ui:input_float(...)` | `function ui:input_float(label, v, step, step_fast, format, flags) end` | function |
| `InputFloat2` | `ui:input_float2(...)` | `function ui:input_float2(label, v, format, flags) end` | function |
| `InputFloat3` | `ui:input_float3(...)` | `function ui:input_float3(label, v, format, flags) end` | function |
| `InputFloat4` | `ui:input_float4(...)` | `function ui:input_float4(label, v, format, flags) end` | function |
| `InputInt` | `ui:input_int(...)` | `function ui:input_int(label, v, step, step_fast, flags) end` | function |
| `InputInt2` | `ui:input_int2(...)` | `function ui:input_int2(label, v, flags) end` | function |
| `InputInt3` | `ui:input_int3(...)` | `function ui:input_int3(label, v, flags) end` | function |
| `InputInt4` | `ui:input_int4(...)` | `function ui:input_int4(label, v, flags) end` | function |
| `InputDouble` | `ui:input_double(...)` | `function ui:input_double(label, v, step, step_fast, format, flags) end` | function |
| `InputScalar` | `ui:input_scalar(...)` | `function ui:input_scalar(label, data_type, p_data, p_step, p_step_fast, format, flags) end` | function |
| `InputScalarN` | `ui:input_scalar_n(...)` | `function ui:input_scalar_n(label, data_type, p_data, components, p_step, p_step_fast, format, flags) end` | function |

### `Moss_Network.h` → `moss.network`

| Native symbol (traceability) | Public API projection | Target-language signature | Kind |
|---|---|---|---|
| `Moss_Init_Network` | `moss.network.initialize(...)` | `function moss.network.initialize() end` | function |
| `enet_initialize_with_callbacks` | `inits:initialize_with(...)` | `function inits:initialize_with() end` | function |
| `Moss_TerminateNetwork` | `moss.network.shutdown(...)` | `function moss.network.shutdown() end` | function |
| `enet_list_clear` | `value:clear(...)` | `function value:clear() end` | function |
| `enet_list_insert` | `moss.network.list_insert(...)` | `function moss.network.list_insert(net_list_iterator, value) end` | function |
| `enet_list_remove` | `moss.network.list_remove(...)` | `function moss.network.list_remove(net_list_iterator) end` | function |
| `enet_list_move` | `moss.network.list_move(...)` | `function moss.network.list_move(net_list_iterator, value, value) end` | function |
| `enet_list_size` | `value:size(...)` | `function value:size() end` | function |
| `enet_time_get` | `moss.network.time_get(...)` | `function moss.network.time_get() end` | function |
| `enet_time_set` | `moss.network.time_set(...)` | `function moss.network.time_set(int32_t) end` | function |
| `enet_socket_create` | `moss.network.create_socket(...)` | `function moss.network.create_socket(adress_type, socket_type) end` | function |
| `enet_socket_bind` | `value:socket_bind(...)` | `function value:socket_bind(net_socket) end` | function |
| `enet_socket_get_address` | `value:socket_get(...)` | `function value:socket_get(net_socket) end` | function |
| `enet_socket_listen` | `moss.network.socket_listen(...)` | `function moss.network.socket_listen(net_socket, nt) end` | function |
| `enet_socket_accept` | `value:socket_accept(...)` | `function value:socket_accept(net_socket) end` | function |
| `enet_socket_connect` | `value:socket_connect(...)` | `function value:socket_connect(net_socket) end` | function |
| `enet_socket_send` | `value:socket_send(...)` | `function value:socket_send(net_socket, value, ize_t) end` | function |
| `enet_socket_receive` | `value:socket_receive(...)` | `function value:socket_receive(net_socket, value, ize_t) end` | function |
| `enet_socket_wait` | `moss.network.socket_wait(...)` | `function moss.network.socket_wait(net_socket, value, int32_t) end` | function |
| `enet_socket_set_option` | `moss.network.socket_set_option(...)` | `function moss.network.socket_set_option(net_socket, net_socket_option, nt) end` | function |
| `enet_socket_get_option` | `moss.network.socket_get_option(...)` | `function moss.network.socket_get_option(net_socket, net_socket_option, value) end` | function |
| `enet_socket_shutdown` | `moss.network.socket_shutdown(...)` | `function moss.network.socket_shutdown(net_socket, net_socket_shutdown) end` | function |
| `enet_socket_destroy` | `moss.network.socket_destroy(...)` | `function moss.network.socket_destroy(net_socket) end` | function |
| `enet_socketset_select` | `value:socketset_select(...)` | `function value:socketset_select(net_socket, value, int32_t) end` | function |
| `enet_address_set_host_ip` | `address:set_host_ip(...)` | `function address:set_host_ip(host_name) end` | function |
| `enet_address_set_host` | `address:set_host(...)` | `function address:set_host(type, host_name) end` | function |
| `enet_address_get_host_ip` | `address:get_host_ip(...)` | `function address:get_host_ip(host_name, name_length) end` | function |
| `enet_address_get_host` | `address:get_host(...)` | `function address:get_host(host_name, name_length) end` | function |
| `enet_address_equal_host` | `firstaddress:equal_host(...)` | `function firstaddress:equal_host(second_address) end` | function |
| `enet_address_is_broadcast` | `address:is_broadcast(...)` | `function address:is_broadcast() end` | function |
| `enet_address_build_any` | `address:build_any(...)` | `function address:build_any(type) end` | function |
| `enet_address_build_loopback` | `address:build_loopback(...)` | `function address:build_loopback(type) end` | function |
| `enet_address_convert_ipv6` | `address:convert_ipv6(...)` | `function address:convert_ipv6() end` | function |
| `enet_crc32` | `value:crc32(...)` | `function value:crc32(ize_t) end` | function |
| `enet_host_create` | `value:create_host(...)` | `function value:create_host(type, ize_t, ize_t, int32_t, int32_t) end` | function |
| `enet_host_destroy` | `value:close(...)` | `function value:close() end` | function |
| `enet_host_check_events` | `value:check_events(...)` | `function value:check_events(value) end` | function |
| `enet_host_service` | `value:service(...)` | `function value:service(value, int32_t) end` | function |
| `enet_host_flush` (overload 1) | `value:flush(...)` | `function value:flush() end` | function |
| `enet_host_broadcast` (overload 1) | `value:broadcast(...)` | `function value:broadcast(int8_t, value) end` | function |
| `enet_host_compress` | `value:compress(...)` | `function value:compress(value) end` | function |
| `enet_host_compress_with_range_coder` | `host:compress_with_range_coder(...)` | `function host:compress_with_range_coder() end` | function |
| `enet_host_channel_limit` | `value:channel_limit(...)` | `function value:channel_limit(ize_t) end` | function |
| `enet_host_bandwidth_limit` | `value:bandwidth_limit(...)` | `function value:bandwidth_limit(int32_t, int32_t) end` | function |
| `enet_host_bandwidth_throttle` | `value:bandwidth_throttle(...)` | `function value:bandwidth_throttle() end` | function |
| `enet_host_random_seed` | `moss.network.host_random_seed(...)` | `function moss.network.host_random_seed() end` | function |
| `enet_host_random` | `value:random(...)` | `function value:random() end` | function |
| `enet_host_connect` (overload 1) | `value:connect(...)` | `function value:connect(value, ize_t, int32_t) end` | function |
| `enet_peer_send` | `value:send(...)` | `function value:send(int8_t, value) end` | function |
| `enet_peer_ping` (overload 1) | `value:ping(...)` | `function value:ping() end` | function |
| `enet_peer_ping_interval` | `value:ping_interval(...)` | `function value:ping_interval(int32_t) end` | function |
| `enet_peer_timeout` (overload 1) | `value:timeout(...)` | `function value:timeout(int32_t, int32_t, int32_t) end` | function |
| `enet_peer_reset` (overload 1) | `value:reset(...)` | `function value:reset() end` | function |
| `enet_peer_disconnect` (overload 1) | `value:disconnect(...)` | `function value:disconnect(int32_t) end` | function |
| `enet_peer_disconnect_now` (overload 1) | `value:disconnect_now(...)` | `function value:disconnect_now(int32_t) end` | function |
| `enet_peer_disconnect_later` | `value:disconnect_later(...)` | `function value:disconnect_later(int32_t) end` | function |
| `enet_peer_throttle_configure` | `value:throttle_configure(...)` | `function value:throttle_configure(int32_t, int32_t, int32_t) end` | function |
| `enet_peer_throttle` | `value:throttle(...)` | `function value:throttle(int32_t) end` | function |
| `enet_peer_reset_queues` | `value:reset_queues(...)` | `function value:reset_queues() end` | function |
| `enet_peer_has_outgoing_commands` | `value:has_outgoing_commands(...)` | `function value:has_outgoing_commands() end` | function |
| `enet_peer_setup_outgoing_command` | `value:peer_setup(...)` | `function value:peer_setup(value) end` | function |
| `enet_peer_queue_outgoing_command` | `value:queue_outgoing_command(...)` | `function value:queue_outgoing_command(value, value, int32_t, int16_t) end` | function |
| `enet_peer_queue_incoming_command` | `value:queue_incoming_command(...)` | `function value:queue_incoming_command(value, value, ize_t, int32_t, int32_t) end` | function |
| `enet_peer_queue_acknowledgement` | `value:queue_acknowledgement(...)` | `function value:queue_acknowledgement(value, int16_t) end` | function |
| `enet_peer_dispatch_incoming_unreliable_commands` | `value:peer_dispatch_incoming_unreliable_commands(...)` | `function value:peer_dispatch_incoming_unreliable_commands(value, value) end` | function |
| `enet_peer_dispatch_incoming_reliable_commands` | `value:peer_dispatch_incoming_reliable_commands(...)` | `function value:peer_dispatch_incoming_reliable_commands(value, value) end` | function |
| `enet_peer_on_connect` | `value:on_connect(...)` | `function value:on_connect() end` | function |
| `enet_peer_on_disconnect` | `value:on_disconnect(...)` | `function value:on_disconnect() end` | function |
| `enet_packet_create` | `moss.network.create_packet(...)` | `function moss.network.create_packet(value, ize_t, int32_t) end` | function |
| `enet_packet_destroy` | `value:close(...)` | `function value:close() end` | function |
| `enet_packet_resize` | `value:resize(...)` | `function value:resize(ize_t) end` | function |
| `enet_peer_receive` | `value:receive(...)` | `function value:receive(channel_id) end` | function |
| `enet_range_coder_create` | `moss.network.create_range_coder(...)` | `function moss.network.create_range_coder() end` | function |
| `enet_range_coder_destroy` | `moss.network.range_coder_destroy(...)` | `function moss.network.range_coder_destroy(value) end` | function |
| `enet_range_coder_compress` | `value:range_coder_compress(...)` | `function value:range_coder_compress(value, ize_t, ize_t, value, ize_t) end` | function |
| `enet_range_coder_decompress` | `moss.network.range_coder_decompress(...)` | `function moss.network.range_coder_decompress(value, value, ize_t, value, ize_t) end` | function |
| `enet_protocol_command_size` | `moss.network.protocol_command_size(...)` | `function moss.network.protocol_command_size(int8_t) end` | function |
| `enet_packet_get_data` | `value:get_data(...)` | `function value:get_data() end` | function |
| `enet_packet_get_user_data` | `value:get_user_data(...)` | `function value:get_user_data() end` | function |
| `enet_packet_set_user_data` | `value:set_user_data(...)` | `function value:set_user_data() end` | function |
| `enet_packet_get_length` | `value:get_length(...)` | `function value:get_length() end` | function |
| `enet_packet_set_acknowledge_callback` | `value:set_acknowledge_callback(...)` | `function value:set_acknowledge_callback(net_packet_acknowledged_callback) end` | function |
| `enet_packet_set_free_callback` | `value:set_free_callback(...)` | `function value:set_free_callback(net_packet_free_callback) end` | function |
| `enet_packet_check_references` | `value:check_references(...)` | `function value:check_references() end` | function |
| `enet_packet_dispose` | `value:dispose(...)` | `function value:dispose() end` | function |
| `enet_host_get_peers_count` | `value:get_peers_count(...)` | `function value:get_peers_count() end` | function |
| `enet_host_get_packets_sent` | `value:get_packets_sent(...)` | `function value:get_packets_sent() end` | function |
| `enet_host_get_packets_received` | `value:get_packets_received(...)` | `function value:get_packets_received() end` | function |
| `enet_host_get_bytes_sent` | `value:get_bytes_sent(...)` | `function value:get_bytes_sent() end` | function |
| `enet_host_get_bytes_received` | `value:get_bytes_received(...)` | `function value:get_bytes_received() end` | function |
| `enet_host_set_max_duplicate_peers` | `value:set_max_duplicate_peers(...)` | `function value:set_max_duplicate_peers(int16_t) end` | function |
| `enet_host_set_intercept_callback` | `value:set_intercept_callback(...)` | `function value:set_intercept_callback(net_intercept_callback) end` | function |
| `enet_host_set_checksum_callback` | `value:set_checksum_callback(...)` | `function value:set_checksum_callback(net_checksum_callback) end` | function |
| `enet_peer_get_id` | `value:get_id(...)` | `function value:get_id() end` | function |
| `enet_peer_get_ip` | `value:get_ip(...)` | `function value:get_ip(value, ize_t) end` | function |
| `enet_peer_get_port` | `value:get_port(...)` | `function value:get_port() end` | function |
| `enet_peer_get_mtu` | `value:get_mtu(...)` | `function value:get_mtu() end` | function |
| `enet_peer_get_state` | `value:get_state(...)` | `function value:get_state() end` | function |
| `enet_peer_get_rtt` | `value:get_rtt(...)` | `function value:get_rtt() end` | function |
| `enet_peer_get_last_rtt` | `peer:get_last_rtt(...)` | `function peer:get_last_rtt() end` | function |
| `enet_peer_get_lastsendtime` | `value:get_lastsendtime(...)` | `function value:get_lastsendtime() end` | function |
| `enet_peer_get_lastreceivetime` | `value:get_lastreceivetime(...)` | `function value:get_lastreceivetime() end` | function |
| `enet_peer_get_packets_throttle` | `value:get_packets_throttle(...)` | `function value:get_packets_throttle() end` | function |
| `enet_peer_get_data` | `value:get_data(...)` | `function value:get_data() end` | function |
| `enet_peer_set_data` | `value:set_data(...)` | `function value:set_data(value) end` | function |
| `enet_host_create_with_transport` (overload 1) | `address:host_create_with_transport(...)` | `function address:host_create_with_transport(peer_count, channel_limit, incoming_bandwidth, outgoing_bandwidth, transport) end` | function |
| `enet_host_connect` (overload 2) | `host:connect(...)` | `function host:connect(address, channel_count, data) end` | function |
| `enet_host_connect_ex` (overload 1) | `host:connect_ex(...)` | `function host:connect_ex(endpoint, channel_count, data) end` | function |
| `enet_host_transport_caps` (overload 1) | `host:transport_caps(...)` | `function host:transport_caps() end` | function |
| `enet_transport_handle_event` | `tevent:transport_handle(...)` | `function tevent:transport_handle(host) end` | function |
| `enet_peer_reset` (overload 2) | `peer:reset(...)` | `function peer:reset() end` | function |
| `enet_peer_timeout` (overload 2) | `peer:timeout(...)` | `function peer:timeout() end` | function |
| `enet_peer_ping` (overload 2) | `peer:ping(...)` | `function peer:ping() end` | function |
| `enet_packet_encode` | `packet:encode(...)` | `function packet:encode(out_buffer, buffer_size) end` | function |
| `enet_packet_decode` | `buffer:packet_decode(...)` | `function buffer:packet_decode(out_packet, buffer_size) end` | function |
| `enet_fragment_send` | `peer:fragment_send(...)` | `function peer:fragment_send(channel_id, data) end` | function |
| `enet_fragment_receive` | `peer:fragment_receive(...)` | `function peer:fragment_receive(fragment, out_packet) end` | function |
| `enet_bandwidth_update` | `host:bandwidth_update(...)` | `function host:bandwidth_update(delta_time) end` | function |
| `enet_peer_send_window` | `peer:send_window(...)` | `function peer:send_window() end` | function |
| `enet_peer_create_channel` | `peer:create_channel(...)` | `function peer:create_channel(reliable, ordered) end` | function |
| `enet_peer_reset_channels` | `peer:reset_channels(...)` | `function peer:reset_channels() end` | function |
| `enet_peer_disconnect` (overload 2) | `peer:disconnect(...)` | `function peer:disconnect(reason) end` | function |
| `enet_peer_disconnect_now` (overload 2) | `peer:disconnect_now(...)` | `function peer:disconnect_now() end` | function |
| `enet_peer_get_stats` | `peer:get_stats(...)` | `function peer:get_stats(rtt, packet_loss, sent, received) end` | function |
| `enet_host_flush` (overload 2) | `host:flush(...)` | `function host:flush() end` | function |
| `enet_host_broadcast` (overload 2) | `host:broadcast(...)` | `function host:broadcast(channel_id, data) end` | function |
| `enet_host_set_protocol_id` | `host:set_protocol_id(...)` | `function host:set_protocol_id(protocol_id) end` | function |
| `enet_host_get_protocol_id` | `host:get_protocol_id(...)` | `function host:get_protocol_id() end` | function |
| `enet_host_create_with_transport` (overload 2) | `value:host_create_with_transport(...)` | `function value:host_create_with_transport(peer_count, channel_limit, incoming_bandwidth, outgoing_bandwidth, transport) end` | function |
| `enet_host_connect` (overload 3) | `value:connect(...)` | `function value:connect(value, ize_t, int32_t) end` | function |
| `enet_host_connect_ex` (overload 2) | `value:connect_ex(...)` | `function value:connect_ex(endpoint, ize_t, int32_t) end` | function |
| `enet_host_transport_caps` (overload 2) | `value:transport_caps(...)` | `function value:transport_caps() end` | function |
| `enet_http_server_start` | `moss.network.http_server_start(...)` | `function moss.network.http_server_start(bind_address, port) end` | function |
| `enet_http_server_stop` | `server:stop(...)` | `function server:stop() end` | function |
| `enet_http_register_endpoint` | `server:http_register_endpoint(...)` | `function server:http_register_endpoint(path, handler) end` | function |
| `enet_host_create_secure` | `address:host_create_secure(...)` | `function address:host_create_secure(peer_count, channel_limit, incoming_bandwidth, outgoing_bandwidth, transport, security) end` | function |
| `enet_http_request_async` | `moss.network.http_request_async(...)` | `function moss.network.http_request_async(desc, cb) end` | function |
| `enet_http_request_cancel` | `request:cancel(...)` | `function request:cancel() end` | function |
| `enet_http_request_release` | `request:release(...)` | `function request:release() end` | function |
| `enet_host_set_connect_validator` | `host:set_connect_validator(...)` | `function host:set_connect_validator(validator) end` | function |
| `enet_transport_send_stream` | `moss.network.transport_send_stream(...)` | `function moss.network.transport_send_stream(args) end` | function |
| `enet_transport_receive_stream` | `moss.network.transport_receive_stream(...)` | `function moss.network.transport_receive_stream(args) end` | function |
| `enet_transport_ws_close` | `value:transport_ws_close(...)` | `function value:transport_ws_close(code) end` | function |
| `enet_transport_webrtc_signal` | `value:transport_webrtc(...)` | `function value:transport_webrtc(value, value) end` | function |
| `enet_host_set_webrtc_signal_callback` | `value:set_webrtc_signal_callback(...)` | `function value:set_webrtc_signal_callback(net_web_rtc_signal_callback, value) end` | function |
| `enet_webrtc_handle_signal` | `value:webrtc_handle(...)` | `function value:webrtc_handle(value, value) end` | function |

### `Moss_Physics.h` → `moss.physics`

| Native symbol (traceability) | Public API projection | Target-language signature | Kind |
|---|---|---|---|
| `CollideShapeResult_FreeMembers` | `result:free_members(...)` | `function result:free_members() end` | function |
| `CollisionEstimationResult_FreeMembers` | `result:free_members(...)` | `function result:free_members() end` | function |
| `BroadPhaseLayerInterfaceMask_Create` | `moss.physics.create_broad_phase_layer_interface_mask(...)` | `function moss.physics.create_broad_phase_layer_interface_mask(num_broad_phase_layers) end` | function |
| `BroadPhaseLayerInterfaceMask_ConfigureLayer` | `bpinterface:mask_configure_layer(...)` | `function bpinterface:mask_configure_layer(broad_phase_layer, groups_to_include, groups_to_exclude) end` | function |
| `BroadPhaseLayerInterfaceTable_Create` | `moss.physics.create_broad_phase_layer_interface_table(...)` | `function moss.physics.create_broad_phase_layer_interface_table(num_object_layers, num_broad_phase_layers) end` | function |
| `BroadPhaseLayerInterfaceTable_MapObjectToBroadPhaseLayer` | `bpinterface:table_map_object_to_broad_phase_layer(...)` | `function bpinterface:table_map_object_to_broad_phase_layer(object_layer, broad_phase_layer) end` | function |
| `ObjectLayerPairFilterMask_Create` | `moss.physics.create_object_layer_pair_filter_mask(...)` | `function moss.physics.create_object_layer_pair_filter_mask() end` | function |
| `ObjectLayerPairFilterMask_GetObjectLayer` | `moss.physics.object_layer_pair_filter_mask_get_object_layer(...)` | `function moss.physics.object_layer_pair_filter_mask_get_object_layer(group, mask) end` | function |
| `ObjectLayerPairFilterMask_GetGroup` | `moss.physics.object_layer_pair_filter_mask_get_group(...)` | `function moss.physics.object_layer_pair_filter_mask_get_group(layer) end` | function |
| `ObjectLayerPairFilterMask_GetMask` | `moss.physics.object_layer_pair_filter_mask_get_mask(...)` | `function moss.physics.object_layer_pair_filter_mask_get_mask(layer) end` | function |
| `ObjectLayerPairFilterTable_Create` | `moss.physics.create_object_layer_pair_filter_table(...)` | `function moss.physics.create_object_layer_pair_filter_table(num_object_layers) end` | function |
| `ObjectLayerPairFilterTable_DisableCollision` | `objectfilter:table_disable_collision(...)` | `function objectfilter:table_disable_collision(layer1, layer2) end` | function |
| `ObjectLayerPairFilterTable_EnableCollision` | `objectfilter:table_enable_collision(...)` | `function objectfilter:table_enable_collision(layer1, layer2) end` | function |
| `ObjectLayerPairFilterTable_ShouldCollide` | `objectfilter:table_should_collide(...)` | `function objectfilter:table_should_collide(layer1, layer2) end` | function |
| `ObjectVsBroadPhaseLayerFilterMask_Create` | `broadphaselayerinterface:create_object_vs_broad_phase_layer_filter_mask(...)` | `function broadphaselayerinterface:create_object_vs_broad_phase_layer_filter_mask() end` | function |
| `ObjectVsBroadPhaseLayerFilterTable_Create` | `broadphaselayerinterface:create_object_vs_broad_phase_layer_filter_table(...)` | `function broadphaselayerinterface:create_object_vs_broad_phase_layer_filter_table(num_broad_phase_layers, object_layer_pair_filter, num_object_layers) end` | function |
| `DrawSettings_InitDefault` | `settings:init_default(...)` | `function settings:init_default() end` | function |
| `ContactManifold_GetWorldSpaceNormal` | `manifold:get_world_space_normal(...)` | `function manifold:get_world_space_normal(result) end` | function |
| `ContactManifold_GetPenetrationDepth` | `manifold:get_penetration_depth(...)` | `function manifold:get_penetration_depth() end` | function |
| `ContactManifold_GetSubShapeID1` | `manifold:get_sub_shape_id1(...)` | `function manifold:get_sub_shape_id1() end` | function |
| `ContactManifold_GetSubShapeID2` | `manifold:get_sub_shape_id2(...)` | `function manifold:get_sub_shape_id2() end` | function |
| `ContactManifold_GetPointCount` | `manifold:get_point_count(...)` | `function manifold:get_point_count() end` | function |
| `ContactManifold_GetWorldSpaceContactPointOn1` | `manifold:get_world_space_contact_point_on1(...)` | `function manifold:get_world_space_contact_point_on1(index, result) end` | function |
| `ContactManifold_GetWorldSpaceContactPointOn2` | `manifold:get_world_space_contact_point_on2(...)` | `function manifold:get_world_space_contact_point_on2(index, result) end` | function |
| `CollisionDispatch_CollideShapeVsShape` | `callback:collision_dispatch_collide_shape_vs_shape(...)` | `function callback:collision_dispatch_collide_shape_vs_shape(shape1, shape2, scale1, scale2, center_of_mass_transform1, center_of_mass_transform2, collide_shape_settings, shape_filter) end` | function |
| `CollisionDispatch_CastShapeVsShapeLocalSpace` | `callback:collision_dispatch_cast_shape_vs_shape_local_space(...)` | `function callback:collision_dispatch_cast_shape_vs_shape_local_space(direction, shape1, shape2, scale1_in_shape2_local_space, scale2, center_of_mass_transform1_in_shape2_local_space, center_of_mass_world_transform2, shape_cast_settings, shape_filter) end` | function |
| `CollisionDispatch_CastShapeVsShapeWorldSpace` | `callback:collision_dispatch_cast_shape_vs_shape_world_space(...)` | `function callback:collision_dispatch_cast_shape_vs_shape_world_space(direction, shape1, shape2, scale1, in_scale2, center_of_mass_world_transform1, center_of_mass_world_transform2, shape_cast_settings, shape_filter) end` | function |
| `Skeleton_Create` | `moss.physics.create_skeleton(...)` | `function moss.physics.create_skeleton() end` | function |
| `Skeleton_Destroy` | `skeleton:close(...)` | `function skeleton:close() end` | function |
| `Skeleton_AddJoint` | `skeleton:add_joint(...)` | `function skeleton:add_joint(name) end` | function |
| `Skeleton_AddJoint2` | `skeleton:add_joint2(...)` | `function skeleton:add_joint2(name, parent_index) end` | function |
| `Skeleton_AddJoint3` | `skeleton:add_joint3(...)` | `function skeleton:add_joint3(name, parent_name) end` | function |
| `Skeleton_GetJointCount` | `skeleton:get_joint_count(...)` | `function skeleton:get_joint_count() end` | function |
| `Skeleton_GetJoint` | `joint:skeleton_get(...)` | `function joint:skeleton_get(skeleton, index) end` | function |
| `Skeleton_GetJointIndex` | `skeleton:get_joint_index(...)` | `function skeleton:get_joint_index(name) end` | function |
| `Skeleton_CalculateParentJointIndices` | `skeleton:calculate_parent_joint_indices(...)` | `function skeleton:calculate_parent_joint_indices() end` | function |
| `Skeleton_AreJointsCorrectlyOrdered` | `skeleton:are_joints_correctly_ordered(...)` | `function skeleton:are_joints_correctly_ordered() end` | function |
| `Indexify` | `moss.physics.indexify(...)` | `function moss.physics.indexify(in_triangles, out_vertices, out_triangles, in_vertex_weld_distance) end` | function |
| `Deindexify` | `moss.physics.deindexify(...)` | `function moss.physics.deindexify(in_vertices, in_triangles, out_triangles) end` | function |

### `Moss_Platform.h` → `moss.platform`

| Native symbol (traceability) | Public API projection | Target-language signature | Kind |
|---|---|---|---|
| `Moss_CreateWindow` | `moss.platform.create_window(...)` | `moss.platform.create_window(options: WindowOptions) -> Window \| nil, error?` | function |
| `Moss_TerminateWindow` | `window:close(...)` | `function window:close() end` | function |
| `Moss_CreateMessageBox` | `window:show_message_box(...)` | `function window:show_message_box(title, message, flags) end` | function |
| `Moss_ShouldWindowClose` | `window:should_close()` | `function window:should_close() -> boolean end` | function |
| `Moss_PollEvents` | `moss.platform.poll_events(...)` | `function moss.platform.poll_events() end` | function |
| `Moss_GetWindowWidth` | `moss.platform.get_window_width(...)` | `function moss.platform.get_window_width() end` | function |
| `Moss_GetWindowHeight` | `moss.platform.get_window_height(...)` | `function moss.platform.get_window_height() end` | function |
| `Moss_SetWindowTitle` | `window:get_title() / window:set_title(...)` | `window:get_title() / window:set_title(title)` | function |
| `Moss_SetWindowIcon` | `window:set_icon(...)` | `function window:set_icon(image) end` | function |
| `Moss_CloseWindow` | `window:close(...)` | `function window:close() end` | function |
| `Moss_MonitorGetPrimary` | `moss.platform.monitors.monitor_get_primary(...)` | `function moss.platform.monitors.monitor_get_primary() end` | function |
| `Moss_MonitorGetSecondary` | `moss.platform.monitors.monitor_get_secondary(...)` | `function moss.platform.monitors.monitor_get_secondary() end` | function |
| `Moss_MonitorGetPhysicalSize` | `monitor:get_physical_size(...)` | `function monitor:get_physical_size(width_mm, height_mm) end` | function |
| `Moss_MonitorGetContentScale` | `monitor:get_content_scale(...)` | `function monitor:get_content_scale(xscale, yscale) end` | function |
| `Moss_MonitorGetPosition` | `monitor:get_position(...)` | `function monitor:get_position(x, y) end` | function |
| `Moss_MonitorGetName` | `monitor:get_name(...)` | `function monitor:get_name() end` | function |
| `Moss_MonitorSetGammaRamp` | `gammaramp:monitor_set(...)` | `function gammaramp:monitor_set(monitor) end` | function |
| `Moss_MonitorGetGammaRamp` | `monitor:get_gamma_ramp(...)` | `function monitor:get_gamma_ramp() end` | function |
| `Moss_MonitorSetGamma` | `monitor:set_gamma(...)` | `function monitor:set_gamma(gamma) end` | function |
| `Moss_IsKeyPressed` | `moss.platform.input.is_key_pressed(...)` | `function moss.platform.input.is_key_pressed(key) end` | function |
| `Moss_IsReleased` | `moss.platform.is_released(...)` | `function moss.platform.is_released(key) end` | function |
| `Moss_IsKeyJustPressed` | `moss.platform.input.is_key_just_pressed(...)` | `function moss.platform.input.is_key_just_pressed(key) end` | function |
| `Moss_IsKeyJustReleased` | `moss.platform.input.is_key_just_released(...)` | `function moss.platform.input.is_key_just_released(key) end` | function |
| `Moss_InputGetKey` | `moss.platform.input.input_get_key(...)` | `function moss.platform.input.input_get_key() end` | function |
| `Moss_IsMousePressed` | `moss.platform.input.is_mouse_pressed(...)` | `function moss.platform.input.is_mouse_pressed(button) end` | function |
| `Moss_IsMouseReleased` | `moss.platform.input.is_mouse_released(...)` | `function moss.platform.input.is_mouse_released(button) end` | function |
| `Moss_IsMouseJustPressed` | `moss.platform.input.is_mouse_just_pressed(...)` | `function moss.platform.input.is_mouse_just_pressed(button) end` | function |
| `Moss_IsMouseJustReleased` | `moss.platform.input.is_mouse_just_released(...)` | `function moss.platform.input.is_mouse_just_released(button) end` | function |
| `Moss_InputGetMouseButton` | `moss.platform.input.input_get_mouse_button(...)` | `function moss.platform.input.input_get_mouse_button() end` | function |
| `Moss_GetMousePosition` | `moss.platform.input.mouse_position(...)` | `local x, y = moss.platform.input.mouse_position()` | function |
| `Moss_SetMousePosition` | `moss.platform.input.set_mouse_position(...)` | `function moss.platform.input.set_mouse_position(x, y) end` | function |
| `Moss_SetMouseVisible` | `moss.platform.input.set_mouse_visible(...)` | `function moss.platform.input.set_mouse_visible(visible) end` | function |
| `Moss_GetNumGamepads` | `moss.platform.get_num_gamepads(...)` | `function moss.platform.get_num_gamepads() end` | function |
| `Moss_OpenGamepad` | `moss.platform.gamepads.open_gamepad(...)` | `function moss.platform.gamepads.open_gamepad(id) end` | function |
| `Moss_CloseGamepad` | `gp:close(...)` | `function gp:close() end` | function |
| `Moss_GamepadConnected` | `gp:connected(...)` | `function gp:connected() end` | function |
| `Moss_UpdateGamepads` | `moss.platform.update_gamepads(...)` | `function moss.platform.update_gamepads() end` | function |
| `Moss_IsGamepadButtonPressed` | `gp:is_button_pressed(...)` | `function gp:is_button_pressed(button) end` | function |
| `Moss_IsGamepadButtonJustPressed` | `gp:is_button_just_pressed(...)` | `function gp:is_button_just_pressed(button) end` | function |
| `Moss_IsGamepadButtonJustReleased` | `gp:is_button_just_released(...)` | `function gp:is_button_just_released(button) end` | function |
| `Moss_GetGamepadAxis` | `gp:get_axis(...)` | `function gp:get_axis(axis) end` | function |
| `Moss_GetGamepadOrientation` | `gp:get_orientation(...)` | `function gp:get_orientation(w, x, y, z) end` | function |
| `Moss_GetGamepadPlayerSpaceGyro` | `gp:get_player_space_gyro(...)` | `function gp:get_player_space_gyro(out_x, out_y, yaw_relax_factor) end` | function |
| `Moss_GetGamepadWorldSpaceGyro` | `gp:get_world_space_gyro(...)` | `function gp:get_world_space_gyro(out_x, out_y, side_reduction_threshold) end` | function |
| `Moss_ResetGamepadCalibration` | `gp:reset_calibration(...)` | `function gp:reset_calibration() end` | function |
| `Moss_RumbleGamepad` | `gp:rumble(...)` | `function gp:rumble(low, high, duration_ms) end` | function |
| `Moss_RumbleGamepadTriggers` | `gp:rumble_triggers(...)` | `function gp:rumble_triggers(left, right, duration_ms) end` | function |
| `Moss_SetGamepadLED` | `gp:set_led(...)` | `function gp:set_led(r, g, b) end` | function |
| `Moss_GetGamepadName` | `gp:get_name(...)` | `function gp:get_name() end` | function |
| `Moss_GetGamepadID` | `gp:get_id(...)` | `function gp:get_id() end` | function |
| `Moss_GetGamepadPlayerIndex` | `gp:get_player_index(...)` | `function gp:get_player_index() end` | function |
| `Moss_GetGamepadPowerInfo` | `gp:get_power_info(...)` | `function gp:get_power_info(percent) end` | function |
| `Moss_GetNumGamepadTouchpads` | `gp:get_num_touchpads(...)` | `function gp:get_num_touchpads() end` | function |
| `Moss_GetNumGamepadTouchpadFingers` | `gp:get_num_touchpad_fingers(...)` | `function gp:get_num_touchpad_fingers() end` | function |
| `Moss_GetGamepadTouchpadFinger` | `gp:get_touchpad_finger(...)` | `function gp:get_touchpad_finger(pad, finger, down, x, y, pressure) end` | function |
| `Moss_GetGamepadMapping` | `gp:get_mapping(...)` | `function gp:get_mapping() end` | function |
| `Moss_SetGamepadMapping` | `gp:set_mapping(...)` | `function gp:set_mapping(mapping) end` | function |
| `Moss_ReloadGamepadMappings` | `moss.platform.gamepads.reload_gamepad_mappings(...)` | `function moss.platform.gamepads.reload_gamepad_mappings() end` | function |
| `Moss_InputGetGamepadButton` | `moss.platform.gamepads.input_get_gamepad_button(...)` | `function moss.platform.gamepads.input_get_gamepad_button() end` | function |
| `Moss_InputGetGamepadAxis` | `moss.platform.gamepads.input_get_gamepad_axis(...)` | `function moss.platform.gamepads.input_get_gamepad_axis() end` | function |
| `Moss_GetPenDeviceType` | `moss.platform.input.get_pen_device_type(...)` | `function moss.platform.input.get_pen_device_type(instance_id) end` | function |
| `Moss_GetTouchDeviceName` | `moss.platform.input.get_touch_device_name(...)` | `function moss.platform.input.get_touch_device_name(touch_id) end` | function |
| `Moss_GetTouchDevices` | `moss.platform.input.get_touch_devices(...)` | `function moss.platform.input.get_touch_devices(count) end` | function |
| `Moss_GetTouchDeviceType` | `moss.platform.input.get_touch_device_type(...)` | `function moss.platform.input.get_touch_device_type(touch_id) end` | function |
| `Moss_GetTouchFingers` | `moss.platform.input.get_touch_fingers(...)` | `function moss.platform.input.get_touch_fingers(touch_id, count) end` | function |
| `Moss_GamepadHasSensor` | `gp:has_sensor(...)` | `function gp:has_sensor(sensor) end` | function |
| `Moss_SetGamepadSensorEnabled` | `gp:set_sensor_enabled(...)` | `function gp:set_sensor_enabled(sensor, enabled) end` | function |
| `Moss_GetGamepadSensorData` | `data:get(...)` | `function data:get(gp, sensor) end` | function |
| `Moss_OpenHaptic` | `moss.platform.haptics.open_haptic(...)` | `function moss.platform.haptics.open_haptic(id) end` | function |
| `Moss_CloseHaptic` | `haptic:close(...)` | `function haptic:close() end` | function |
| `Moss_CreateHapticEffect` | `haptic:create_effect(...)` | `function haptic:create_effect() end` | function |
| `Moss_DestroyHapticEffect` | `haptic:destroy_effect(...)` | `function haptic:destroy_effect() end` | function |
| `Moss_GetHapticEffectStatus` | `haptic:get_effect_status(...)` | `function haptic:get_effect_status() end` | function |
| `Moss_GetHapticFeatures` | `haptic:get_features(...)` | `function haptic:get_features() end` | function |
| `Moss_GetHapticFromID` | `haptic:get_from_id(...)` | `function haptic:get_from_id() end` | function |
| `Moss_GetHapticID` | `haptic:get_id(...)` | `function haptic:get_id() end` | function |
| `Moss_GetHapticName` | `haptic:get_name(...)` | `function haptic:get_name() end` | function |
| `Moss_GetHapticNameForID` | `haptic:get_name_for_id(...)` | `function haptic:get_name_for_id() end` | function |
| `Moss_GetHaptics` | `haptic:get_haptics(...)` | `function haptic:get_haptics() end` | function |
| `Moss_GetMaxHapticEffects` | `haptic:get_max_effects(...)` | `function haptic:get_max_effects() end` | function |
| `Moss_GetMaxHapticEffectsPlaying` | `haptic:get_max_effects_playing(...)` | `function haptic:get_max_effects_playing() end` | function |
| `Moss_GetNumHapticAxes` | `haptic:get_num_axes(...)` | `function haptic:get_num_axes() end` | function |
| `Moss_HapticEffectSupported` | `haptic:effect_supported(...)` | `function haptic:effect_supported() end` | function |
| `Moss_HapticRumbleSupported` | `haptic:rumble_supported(...)` | `function haptic:rumble_supported() end` | function |
| `Moss_InitHapticRumble` | `joystick:init_rumble(...)` | `function joystick:init_rumble() end` | function |
| `Moss_IsJoystickHaptic` | `joystick:is_joystick_haptic(...)` | `function joystick:is_joystick_haptic() end` | function |
| `Moss_IsMouseHaptic` | `moss.platform.haptics.is_mouse_haptic(...)` | `function moss.platform.haptics.is_mouse_haptic() end` | function |
| `Moss_OpenHapticFromJoystick` | `joystick:open_haptic_from_joystick(...)` | `function joystick:open_haptic_from_joystick() end` | function |
| `Moss_OpenHapticFromMouse` | `moss.platform.haptics.open_haptic_from_mouse(...)` | `function moss.platform.haptics.open_haptic_from_mouse() end` | function |
| `Moss_PauseHaptic` | `haptic:pause(...)` | `function haptic:pause() end` | function |
| `Moss_PlayHapticRumble` | `haptic:play_rumble(...)` | `function haptic:play_rumble(strength, length) end` | function |
| `Moss_ResumeHaptic` | `haptic:resume(...)` | `function haptic:resume() end` | function |
| `Moss_RunHapticEffect` | `haptic:run_effect(...)` | `function haptic:run_effect(iterations) end` | function |
| `Moss_SetHapticAutocenter` | `haptic:set_autocenter(...)` | `function haptic:set_autocenter(center) end` | function |
| `Moss_SetHapticGain` | `haptic:set_gain(...)` | `function haptic:set_gain(gain) end` | function |
| `Moss_StopHapticEffect` | `haptic:stop_effect(...)` | `function haptic:stop_effect(effect) end` | function |
| `Moss_StopHapticEffects` | `haptic:stop_effects(...)` | `function haptic:stop_effects() end` | function |
| `Moss_StopHapticRumble` | `haptic:stop_rumble(...)` | `function haptic:stop_rumble() end` | function |
| `Moss_UpdateHapticEffect` | `data:update(...)` | `function data:update(haptic, effect) end` | function |
| `Moss_GetAvailableCPUCores` | `moss.platform.get_available_cpu_cores(...)` | `function moss.platform.get_available_cpu_cores() end` | function |
| `Moss_GetCPUCacheLineSize` | `moss.platform.get_cpu_cache_line_size(...)` | `function moss.platform.get_cpu_cache_line_size() end` | function |
| `Moss_GetSystemRAM` | `moss.platform.get_system_ram(...)` | `function moss.platform.get_system_ram() end` | function |
| `Moss_OpenURL` | `moss.platform.open_url(...)` | `function moss.platform.open_url(url) end` | function |
| `Moss_GetLocale` | `moss.platform.get_locale(...)` | `function moss.platform.get_locale() end` | function |
| `Moss_GetPowerInfo` | `moss.platform.get_power_info(...)` | `function moss.platform.get_power_info(seconds, percent) end` | function |
| `Moss_IsProcessRunningByName` | `moss.platform.is_process_running_by_name(...)` | `function moss.platform.is_process_running_by_name(executable_path) end` | function |
| `Moss_LoadDynamicLibrary` | `moss.platform.load_dynamic_library(...)` | `function moss.platform.load_dynamic_library(lib_path) end` | function |
| `Moss_GetLibrarySymbol` | `moss.platform.get_library_symbol(...)` | `function moss.platform.get_library_symbol(handle, symbol_name) end` | function |
| `Moss_UnloadDynamicLibrary` | `moss.platform.unload_dynamic_library(...)` | `function moss.platform.unload_dynamic_library(handle) end` | function |
| `Moss_SetFramebufferResizeCallback` | `moss.platform.set_framebuffer_resize_callback(...)` | `function moss.platform.set_framebuffer_resize_callback(callback) end` | function |
| `Moss_SetWindowSizeCallback` | `moss.platform.set_window_size_callback(...)` | `function moss.platform.set_window_size_callback(callback) end` | function |
| `Moss_SetWindowResizeCallback` | `moss.platform.set_window_resize_callback(...)` | `function moss.platform.set_window_resize_callback(callback) end` | function |
| `Moss_SetWindowPositionCallback` | `moss.platform.set_window_position_callback(...)` | `function moss.platform.set_window_position_callback(callback) end` | function |
| `Moss_SetWindowFocusCallback` | `moss.platform.set_window_focus_callback(...)` | `function moss.platform.set_window_focus_callback(callback) end` | function |
| `Moss_SetWindowContentScaleCallback` | `moss.platform.set_window_content_scale_callback(...)` | `function moss.platform.set_window_content_scale_callback(callback) end` | function |
| `Moss_SetMonitorCallback` | `moss.platform.monitors.set_monitor_callback(...)` | `function moss.platform.monitors.set_monitor_callback(callback) end` | function |
| `Moss_GetCameras` | `moss.platform.get_cameras(...)` | `function moss.platform.get_cameras(count) end` | function |
| `Moss_GetCameraName` | `moss.platform.cameras.get_camera_name(...)` | `function moss.platform.cameras.get_camera_name(camera_id) end` | function |
| `Moss_GetCameraPosition` | `moss.platform.cameras.get_camera_position(...)` | `function moss.platform.cameras.get_camera_position(camera_id) end` | function |
| `Moss_GetCurrentCameraDriver` | `moss.platform.cameras.get_current_camera_driver(...)` | `function moss.platform.cameras.get_current_camera_driver() end` | function |
| `Moss_GetNumCameraDrivers` | `moss.platform.cameras.get_num_camera_drivers(...)` | `function moss.platform.cameras.get_num_camera_drivers() end` | function |
| `Moss_GetCameraSupportedFormats` | `moss.platform.cameras.get_camera_supported_formats(...)` | `function moss.platform.cameras.get_camera_supported_formats(camera_id, count) end` | function |
| `Moss_AcquireCameraFrame` | `camera:acquire_camera_frame(...)` | `function camera:acquire_camera_frame(timestamp_ns) end` | function |
| `Moss_ReleaseCameraFrame` | `camera:release_camera_frame(...)` | `function camera:release_camera_frame(frame) end` | function |
| `Moss_GetCameraFormat` | `camera:get_camera_format(...)` | `function camera:get_camera_format(out_spec) end` | function |
| `Moss_GetCameraPermissionState` | `camera:get_camera_permission_state(...)` | `function camera:get_camera_permission_state() end` | function |
| `Moss_GetCameraProperties` (overload 1) | `camera:get_camera_properties(...)` | `function camera:get_camera_properties() end` | function |
| `Moss_CloseCamera` | `camera:close_camera(...)` | `function camera:close_camera() end` | function |
| `Moss_GetCameraID` | `camera:get_camera_id(...)` | `function camera:get_camera_id() end` | function |
| `Moss_GetCameraProperties` (overload 2) | `camera:get_camera_properties(...)` | `function camera:get_camera_properties() end` | function |
| `Moss_OpenCapture` | `spec:open_capture(...)` | `function spec:open_capture(id) end` | function |
| `Moss_CopyFile` | `moss.platform.storage.copy_file(...)` | `function moss.platform.storage.copy_file(src_path, dst_path, overwrite) end` | function |
| `Moss_CreateDirectory` | `moss.platform.storage.create_directory(...)` | `function moss.platform.storage.create_directory(path, recursive) end` | function |
| `Moss_RemovePath` | `moss.platform.storage.remove_path(...)` | `function moss.platform.storage.remove_path(path, recursive) end` | function |
| `Moss_RenamePath` | `moss.platform.storage.rename_path(...)` | `function moss.platform.storage.rename_path(old_path, new_path, overwrite) end` | function |
| `Moss_GetPathInfo` | `moss.platform.storage.get_path_info(...)` | `function moss.platform.storage.get_path_info(path, out_info) end` | function |
| `Moss_GetCurrentDirectory` | `moss.platform.storage.get_current_directory(...)` | `function moss.platform.storage.get_current_directory(out_path, max_len) end` | function |
| `Moss_GetBasePath` | `moss.platform.storage.get_base_path(...)` | `function moss.platform.storage.get_base_path(out_path, max_len) end` | function |
| `Moss_GetUserFolder` | `moss.platform.get_user_folder(...)` | `function moss.platform.get_user_folder(folder, out_path, max_len) end` | function |
| `Moss_GetPrefPath` | `moss.platform.storage.get_pref_path(...)` | `function moss.platform.storage.get_pref_path(org_name, app_name, out_path, max_len) end` | function |
| `Moss_EnumerateDirectory` | `moss.platform.storage.enumerate_directory(...)` | `function moss.platform.storage.enumerate_directory(path, recursive, callback) end` | function |
| `Moss_GlobDirectory` | `moss.platform.storage.glob_directory(...)` | `function moss.platform.storage.glob_directory(pattern, callback) end` | function |
| `Moss_ShowFileDialogWithProperties` | `filters:show_file_dialog_with_properties(...)` | `function filters:show_file_dialog_with_properties(callback, window, nfilters, default_location) end` | function |
| `Moss_ShowOpenFolderDialog` | `filters:show_open_folder_dialog(...)` | `function filters:show_open_folder_dialog(callback, window, nfilters, default_location, allow_many) end` | function |
| `Moss_ShowOpenFileDialog` | `window:show_open_file_dialog(...)` | `function window:show_open_file_dialog(callback, default_location, allow_many) end` | function |
| `Moss_ShowSaveFileDialog` | `moss.platform.dialogs.show_save_file_dialog(...)` | `function moss.platform.dialogs.show_save_file_dialog(type, callback, props) end` | function |
| `Moss_CloseStorage` | `storage:close(...)` | `function storage:close() end` | function |
| `Moss_CopyStorageFile` | `storage:copy_file(...)` | `function storage:copy_file(oldpath, newpath) end` | function |
| `Moss_CreateStorageDirectory` | `storage:create_directory(...)` | `function storage:create_directory(path) end` | function |
| `Moss_EnumerateStorageDirectory` | `storage:enumerate_directory(...)` | `function storage:enumerate_directory(path, callback) end` | function |
| `Moss_GetStorageFileSize` | `storage:get_file_size(...)` | `function storage:get_file_size(path, length) end` | function |
| `Moss_GetStorageSpaceRemaining` | `storage:get_space_remaining(...)` | `function storage:get_space_remaining() end` | function |
| `Moss_GetStoragePathInfo` | `storage:get_path_info(...)` | `function storage:get_path_info(path, info) end` | function |
| `Moss_GlobStorageDirectory` | `storage:glob_directory(...)` | `function storage:glob_directory(path, pattern, flags, count) end` | function |
| `Moss_OpenFileStorage` | `moss.platform.storage.open_file_storage(...)` | `function moss.platform.storage.open_file_storage(path) end` | function |
| `Moss_OpenStorage` | `iface:open(...)` | `function iface:open() end` | function |
| `Moss_OpenTitleStorage` | `moss.platform.storage.open_title_storage(...)` | `function moss.platform.storage.open_title_storage(override, props) end` | function |
| `Moss_OpenUserStorage` | `moss.platform.storage.open_user_storage(...)` | `function moss.platform.storage.open_user_storage(org, app, props) end` | function |
| `Moss_ReadStorageFile` | `storage:read_file(...)` | `function storage:read_file(path, destination, length) end` | function |
| `Moss_RemoveStoragePath` | `storage:remove_path(...)` | `function storage:remove_path(path) end` | function |
| `Moss_RenameStoragePath` | `storage:rename_path(...)` | `function storage:rename_path(oldpath, newpath) end` | function |
| `Moss_StorageReady` | `storage:ready(...)` | `function storage:ready() end` | function |
| `Moss_WriteStorageFile` | `storage:write_file(...)` | `function storage:write_file(path, source, length) end` | function |
| `Moss_CreateEmbeddedWindow` | `moss.platform.create_embedded_window(...)` | `function moss.platform.create_embedded_window() end` | function |
| `Moss_Terminate_EmbeddedWindow` | `moss.platform.terminate_embedded_window(...)` | `function moss.platform.terminate_embedded_window() end` | function |
| `Moss_EmbeddedWindow_OnOrientationChanged` | `moss.platform.embedded_window_on_orientation_changed(...)` | `function moss.platform.embedded_window_on_orientation_changed(focused) end` | function |
| `Moss_EmbeddedWindow_OnResume` | `moss.platform.embedded_window_on_resume(...)` | `function moss.platform.embedded_window_on_resume() end` | function |
| `Moss_EmbeddedWindow_OnPause` | `moss.platform.embedded_window_on_pause(...)` | `function moss.platform.embedded_window_on_pause() end` | function |
| `Moss_MakeContextCurrent` | `window:make_context_current(...)` | `function window:make_context_current() end` | function |
| `Moss_SwapBuffers` | `moss.platform.swap_buffers(...)` | `function moss.platform.swap_buffers() end` | function |
| `Moss_SwapBuffersInterval` | `moss.platform.swap_buffers_interval(...)` | `function moss.platform.swap_buffers_interval(interval) end` | function |
| `Moss_GetProcAddress` | `moss.platform.get_proc_address(...)` | `function moss.platform.get_proc_address(procname) end` | function |
| `Moss_CreateWindowSurface` | `window:create_vulkan_surface(...)` | `function window:create_vulkan_surface(instance, allocator) -> VkSurfaceKHR \| nil, error? end` | function |
| `Moss_VulkanSupported` | `moss.platform.supported(...)` | `function moss.platform.supported() end` | function |
| `Moss_GetRequiredInstanceExtensions` | `moss.platform.get_required_instance_extensions(...)` | `function moss.platform.get_required_instance_extensions(count) end` | function |
| `Moss_GetInstanceProcAddress` | `moss.platform.get_instance_proc_address(...)` | `function moss.platform.get_instance_proc_address(instance, procname) end` | function |
| `Moss_GetPhysicalDevicePresentationSupport` | `window:get_physical_device_presentation_support(...)` | `function window:get_physical_device_presentation_support(device, queuefamily) end` | function |
| `Moss_Metal_CreateView` | `window:create_view(...)` | `function window:create_view() end` | function |
| `Moss_Metal_DestroyView` | `moss.platform.destroy_view(...)` | `function moss.platform.destroy_view(view) end` | function |
| `Moss_Metal_GetLayer` | `moss.platform.get_layer(...)` | `function moss.platform.get_layer(view) end` | function |
| `Moss_Metal_Resize` | `moss.platform.resize(...)` | `function moss.platform.resize(handle, width, height) end` | function |

### `Moss_Renderer.h` → `moss.renderer`

| Native symbol (traceability) | Public API projection | Target-language signature | Kind |
|---|---|---|---|
| `Moss_RendererCreateMaterial` | `renderer:create_material(...)` | `function renderer:create_material(desc) end` | function |
| `Moss_RendererDestroyMaterial` | `renderer:destroy_material(...)` | `function renderer:destroy_material(material) end` | function |
| `Moss_MaterialSetUniform` | `material:set_uniform(...)` | `function material:set_uniform(name, data, size) end` | function |
| `Moss_MaterialSetTexture` | `material:set_texture(...)` | `function material:set_texture(type, texture, sampler) end` | function |
| `Moss_RendererCreatePipeline` | `renderer:create_pipeline(...)` | `function renderer:create_pipeline(desc) end` | function |
| `Moss_PipelineBind` | `moss.renderer.pipeline_bind(...)` | `function moss.renderer.pipeline_bind(pipeline) end` | function |
| `Moss_PipelineUnbind` | `moss.renderer.pipeline_unbind(...)` | `function moss.renderer.pipeline_unbind(pipeline) end` | function |
| `Moss_PipelineSetUniform` | `moss.renderer.pipeline_set_uniform(...)` | `function moss.renderer.pipeline_set_uniform(pipeline, name, data, size) end` | function |
| `Moss_PipelineFlush` | `moss.renderer.pipeline_flush(...)` | `function moss.renderer.pipeline_flush(pipeline) end` | function |
| `Moss_PipelineValidate` | `moss.renderer.pipeline_validate(...)` | `function moss.renderer.pipeline_validate(pipeline) end` | function |
| `Moss_RendererCreateResourceSet` | `renderer:create_resource_set(...)` | `function renderer:create_resource_set(desc) end` | function |
| `Moss_RendererDestroyResourceSet` | `renderer:destroy_resource_set(...)` | `function renderer:destroy_resource_set(set) end` | function |
| `Moss_ResourceSetBindUniformBuffer` | `set:bind_uniform_buffer(...)` | `function set:bind_uniform_buffer(binding, buffer) end` | function |
| `Moss_ResourceSetBindUniformBufferRange` | `set:bind_uniform_buffer_range(...)` | `function set:bind_uniform_buffer_range(binding, buffer, offset, size) end` | function |
| `Moss_ResourceSetBindStorageBuffer` | `set:bind_storage_buffer(...)` | `function set:bind_storage_buffer(binding, buffer) end` | function |
| `Moss_ResourceSetBindTexture` | `set:bind_texture(...)` | `function set:bind_texture(binding, texture) end` | function |
| `Moss_ResourceSetBindSampler` | `set:bind_sampler(...)` | `function set:bind_sampler(binding, sampler) end` | function |
| `Moss_ResourceSetBind` | `renderer:resource_set_bind(...)` | `function renderer:resource_set_bind(pipeline, set_index, set) end` | function |
| `Moss_RendererCreateFramebuffer` | `renderer:create_framebuffer(...)` | `function renderer:create_framebuffer(desc) end` | function |
| `Moss_RendererDestroyFramebuffer` | `renderer:destroy_framebuffer(...)` | `function renderer:destroy_framebuffer(framebuffer) end` | function |
| `Moss_FramebufferResize` | `framebuffer:resize(...)` | `function framebuffer:resize(width, height) end` | function |
| `Moss_PostProcessExecute` | `renderer:post_process_execute(...)` | `function renderer:post_process_execute(pass) end` | function |
| `Moss_FramebufferBegin` | `renderer:framebuffer_begin(...)` | `function renderer:framebuffer_begin(framebuffer) end` | function |
| `Moss_FramebufferEnd` | `renderer:framebuffer_end(...)` | `function renderer:framebuffer_end(framebuffer) end` | function |
| `Moss_TextureBarrier` | `renderer:texture_barrier(...)` | `function renderer:texture_barrier(texture, old_state, new_state) end` | function |
| `Moss_RendererGetBackbuffer` | `renderer:get_backbuffer(...)` | `function renderer:get_backbuffer() end` | function |
| `Moss_RenderSubViewport` | `sub_viewport:render(...)` | `function sub_viewport:render(cmd, record) end` | function |
| `Moss_RendererCreate` | `moss.renderer.create(...)` | `function moss.renderer.create(desc) end` | function |
| `Moss_CreateRenderer` | `moss.renderer.create_renderer(...)` | `function moss.renderer.create_renderer(window) end` | function |
| `Moss_RendererDestroy` | `renderer:close(...)` | `function renderer:close() end` | function |
| `Moss_TerminateRenderer` | `renderer:close(...)` | `function renderer:close() end` | function |
| `Moss_RendererBeginFrame` | `renderer:begin_frame(...)` | `function renderer:begin_frame() end` | function |
| `Moss_RendererBeginFrameEx` | `renderer:begin_frame_ex(...)` | `function renderer:begin_frame_ex(desc) end` | function |
| `Moss_RendererEndFrame` | `renderer:end_frame(...)` | `function renderer:end_frame() end` | function |
| `Moss_RendererGetGPUDevice` | `renderer:get_gpu_device(...)` | `function renderer:get_gpu_device() end` | function |
| `Moss_MeshLoadFBX` | `moss.renderer.mesh_load_fbx(...)` | `function moss.renderer.mesh_load_fbx(file_path) end` | function |
| `Moss_MeshLoadOBJ` | `moss.renderer.mesh_load_obj(...)` | `function moss.renderer.mesh_load_obj(file_path) end` | function |
| `Moss_MeshLoadGLB` | `moss.renderer.mesh_load_glb(...)` | `function moss.renderer.mesh_load_glb(file_path) end` | function |
| `Moss_ModelLoadFBX` | `moss.renderer.model_load_fbx(...)` | `function moss.renderer.model_load_fbx(file_path) end` | function |
| `Moss_ModelLoadOBJ` | `moss.renderer.model_load_obj(...)` | `function moss.renderer.model_load_obj(file_path) end` | function |
| `Moss_ModelLoadGLB` | `moss.renderer.model_load_glb(...)` | `function moss.renderer.model_load_glb(file_path) end` | function |
| `Moss_ModelLoad` | `moss.renderer.model_load(...)` | `function moss.renderer.model_load(file_path) end` | function |
| `Moss_ModelGetLastLoadError` | `moss.renderer.model_get_last_load_error(...)` | `function moss.renderer.model_get_last_load_error() end` | function |
| `Moss_ModelGetLastLoadErrorMessage` | `moss.renderer.model_get_last_load_error_message(...)` | `function moss.renderer.model_get_last_load_error_message() end` | function |
| `Moss_ModelFindBone` | `model:find_bone(...)` | `function model:find_bone(name) end` | function |
| `Moss_ModelFindMorphTarget` | `model:find_morph_target(...)` | `function model:find_morph_target(name) end` | function |
| `Moss_ModelSetBoneTransform` | `model:set_bone_transform(...)` | `function model:set_bone_transform(bone_index, transform16) end` | function |
| `Moss_ModelSetMorphWeight` | `model:set_morph_weight(...)` | `function model:set_morph_weight(morph_index, weight) end` | function |
| `Moss_RendererDrawMesh` | `renderer:draw_mesh(...)` | `function renderer:draw_mesh(desc) end` | function |
| `Moss_RendererDrawModelDesc` | `renderer:draw_model_desc(...)` | `function renderer:draw_model_desc(desc) end` | function |
| `Moss_RendererDrawModel` | `renderer:draw_model(...)` | `function renderer:draw_model(model, transform) end` | function |
| `Moss_RendererCreateFont` | `renderer:create_font(...)` | `function renderer:create_font(desc) end` | function |
| `Moss_RendererDestroyFont` | `renderer:destroy_font(...)` | `function renderer:destroy_font(font) end` | function |
| `Moss_FontLoad` | `moss.renderer.font_load(...)` | `function moss.renderer.font_load(path, pixel_size) end` | function |
| `Moss_FontCreateFromMemory` | `moss.renderer.font_create_from_memory(...)` | `function moss.renderer.font_create_from_memory(data, data_size, pixel_size) end` | function |
| `Moss_FontDestroy` | `font:close(...)` | `function font:close() end` | function |
| `Moss_FontGetMetrics` | `font:get_metrics(...)` | `function font:get_metrics() end` | function |
| `Moss_FontGetGlyph` | `font:get_glyph(...)` | `function font:get_glyph(codepoint) end` | function |
| `Moss_FontGetAtlasPixels` | `font:get_atlas_pixels(...)` | `function font:get_atlas_pixels(width, height, stride) end` | function |
| `Moss_FontGetAtlasTexture` | `font:get_atlas_texture(...)` | `function font:get_atlas_texture() end` | function |
| `Moss_FontUploadAtlas` | `renderer:font_upload_atlas(...)` | `function renderer:font_upload_atlas(font) end` | function |
| `Moss_FontMeasureText` | `font:measure_text(...)` | `function font:measure_text(text, scale) end` | function |
| `Moss_FontBuildTextQuads` | `moss.renderer.font_build_text_quads(...)` | `function moss.renderer.font_build_text_quads(desc, out_vertices, vertex_capacity) end` | function |
| `Moss_RendererDrawText` | `renderer:draw_text(...)` | `function renderer:draw_text(desc) end` | function |
| `Moss_RendererGetFontBackendMask` | `moss.renderer.get_font_backend_mask(...)` | `function moss.renderer.get_font_backend_mask() end` | function |
| `Moss_RendererBackendSupportsFonts` | `moss.renderer.backend_supports_fonts(...)` | `function moss.renderer.backend_supports_fonts(backend) end` | function |
| `Moss_RendererDrawSprite` | `renderer:draw_sprite(...)` | `function renderer:draw_sprite(desc) end` | function |
| `Moss_RendererCreateSpriteBatch` | `renderer:create_sprite_batch(...)` | `function renderer:create_sprite_batch(capacity) end` | function |
| `Moss_RendererDestroySpriteBatch` | `renderer:destroy_sprite_batch(...)` | `function renderer:destroy_sprite_batch(batch) end` | function |
| `Moss_SpriteBatchClear` | `batch:clear(...)` | `function batch:clear() end` | function |
| `Moss_SpriteBatchAdd` | `batch:add(...)` | `function batch:add(desc) end` | function |
| `Moss_RendererDrawSpriteBatch` | `renderer:draw_sprite_batch(...)` | `function renderer:draw_sprite_batch(batch) end` | function |
| `Moss_RendererCreateDebugDrawList` | `renderer:create_debug_draw_list(...)` | `function renderer:create_debug_draw_list(capacity) end` | function |
| `Moss_RendererDestroyDebugDrawList` | `renderer:destroy_debug_draw_list(...)` | `function renderer:destroy_debug_draw_list(list) end` | function |
| `Moss_DebugDrawListClear` | `list:clear(...)` | `function list:clear() end` | function |
| `Moss_DebugDrawLine` | `list:debug_draw_line(...)` | `function list:debug_draw_line(desc) end` | function |
| `Moss_DebugDrawBox` | `list:debug_draw_box(...)` | `function list:debug_draw_box(desc) end` | function |
| `Moss_DebugDrawSphere` | `list:debug_draw_sphere(...)` | `function list:debug_draw_sphere(desc) end` | function |
| `Moss_DebugDrawNavmesh` | `list:debug_draw_navmesh(...)` | `function list:debug_draw_navmesh(desc) end` | function |
| `Moss_DebugDrawPhysicsContact` | `list:debug_draw_physics_contact(...)` | `function list:debug_draw_physics_contact(desc) end` | function |
| `Moss_RendererDrawDebugList` | `renderer:draw_debug_list(...)` | `function renderer:draw_debug_list(list) end` | function |
| `Moss_CameraCreate2D` | `moss.renderer.camera_create_dimension2_d(...)` | `function moss.renderer.camera_create_dimension2_d(desc) end` | function |
| `Moss_CameraCreate3D` | `moss.renderer.camera_create_dimension3_d(...)` | `function moss.renderer.camera_create_dimension3_d(desc) end` | function |
| `Moss_CameraCreateEditor` | `moss.renderer.camera_create_editor(...)` | `function moss.renderer.camera_create_editor(desc) end` | function |
| `Moss_CameraDestroy` | `camera:close(...)` | `function camera:close() end` | function |
| `Moss_RendererSetCamera` | `renderer:set_camera(...)` | `function renderer:set_camera(camera) end` | function |
| `Moss_RendererCreateShadowMap` | `renderer:create_shadow_map(...)` | `function renderer:create_shadow_map(desc) end` | function |
| `Moss_RendererDestroyShadowMap` | `renderer:destroy_shadow_map(...)` | `function renderer:destroy_shadow_map(shadow_map) end` | function |
| `Moss_RendererBeginShadowPass` | `renderer:begin_shadow_pass(...)` | `function renderer:begin_shadow_pass(shadow_map) end` | function |
| `Moss_RendererEndShadowPass` | `renderer:end_shadow_pass(...)` | `function renderer:end_shadow_pass(shadow_map) end` | function |
| `Moss_MeshDraw` | `mesh:draw(...)` | `function mesh:draw() end` | function |
| `Moss_ModelDraw` | `model:draw(...)` | `function model:draw() end` | function |
| `Moss_MeshRemove` | `mesh:close(...)` | `function mesh:close() end` | function |
| `Moss_ModelRemove` | `model:close(...)` | `function model:close() end` | function |
| `Moss_ResourceSetCreate` | `renderer:create_resource_set(...)` | `function renderer:create_resource_set(desc) end` | function |
| `Moss_ResourceSetDestroy` | `set:close(...)` | `function set:close() end` | function |
| `Moss_FramebufferCreate` | `renderer:create_framebuffer(...)` | `function renderer:create_framebuffer(desc) end` | function |
| `Moss_FramebufferDestroy` | `framebuffer:close(...)` | `function framebuffer:close() end` | function |
| `Moss_RendererSetUpscaler` | `renderer:set_upscaler(...)` | `function renderer:set_upscaler(upscaler) end` | function |
| `Moss_RendererSetUpscalerDesc` | `renderer:set_upscaler_desc(...)` | `function renderer:set_upscaler_desc(desc) end` | function |
| `Moss_GPUGetPhysicalDeviceName` | `moss.renderer.get_physical_device_name(...)` | `function moss.renderer.get_physical_device_name() end` | function |
| `Moss_GPUAPIGetName` | `moss.renderer.gpuapi_get_name(...)` | `function moss.renderer.gpuapi_get_name() end` | function |
| `Moss_GPUAPIGetVersion` | `moss.renderer.gpuapi_get_version(...)` | `function moss.renderer.gpuapi_get_version() end` | function |
| `Moss_RendererDrawLine2D` | `renderer:draw_line_dimension2_d(...)` | `function renderer:draw_line_dimension2_d(from, to, color, thickness) end` | function |
| `Moss_RendererDrawLines2D` | `renderer:draw_lines_dimension2_d(...)` | `function renderer:draw_lines_dimension2_d(positions, color, thickness) end` | function |
| `Moss_RendererDrawBox2D` | `renderer:draw_box_dimension2_d(...)` | `function renderer:draw_box_dimension2_d(center, rotation, color) end` | function |
| `Moss_RendererDrawCircle2D` | `renderer:draw_circle_dimension2_d(...)` | `function renderer:draw_circle_dimension2_d(center, radius, color) end` | function |
| `Moss_RendererDrawCylinder2D` | `renderer:draw_cylinder_dimension2_d(...)` | `function renderer:draw_cylinder_dimension2_d(matrix, half_height, radius, color) end` | function |
| `Moss_RendererDrawTriangle2D` | `renderer:draw_triangle_dimension2_d(...)` | `function renderer:draw_triangle_dimension2_d(p0, p1, p2, color) end` | function |
| `Moss_RendererDrawArrow2D` | `renderer:draw_arrow_dimension2_d(...)` | `function renderer:draw_arrow_dimension2_d(from, to, color, head_size) end` | function |
| `Moss_RendererDrawMarker2D` | `renderer:draw_marker_dimension2_d(...)` | `function renderer:draw_marker_dimension2_d(position, color, size) end` | function |
| `Moss_RendererDrawBone2D` | `renderer:draw_bone_dimension2_d(...)` | `function renderer:draw_bone_dimension2_d(transform, length, color) end` | function |
| `Moss_RendererDrawCoordinateSystem2D` | `renderer:draw_coordinate_system_dimension2_d(...)` | `function renderer:draw_coordinate_system_dimension2_d(matrix, size) end` | function |
| `Moss_RendererDrawCapsule2D` | `renderer:draw_capsule_dimension2_d(...)` | `function renderer:draw_capsule_dimension2_d(matrix, half_height_of_cylinder, radius, color) end` | function |
| `Moss_RendererDrawConvex2D` | `renderer:draw_convex_dimension2_d(...)` | `function renderer:draw_convex_dimension2_d(points, color) end` | function |
| `Moss_RendererDrawConvexHull2D` | `renderer:draw_convex_hull_dimension2_d(...)` | `function renderer:draw_convex_hull_dimension2_d(points, color) end` | function |
| `Moss_RendererDrawPolygonShape2D` | `renderer:draw_polygon_shape_dimension2_d(...)` | `function renderer:draw_polygon_shape_dimension2_d(points, color) end` | function |
| `Moss_RendererDrawGizmo2D` | `renderer:draw_gizmo_dimension2_d(...)` | `function renderer:draw_gizmo_dimension2_d(transform, size) end` | function |
| `Moss_RendererDrawLineBatch2D` | `renderer:draw_line_batch_dimension2_d(...)` | `function renderer:draw_line_batch_dimension2_d(transform, colour, count) end` | function |
| `Moss_RendererDrawBoxBatch2D` | `renderer:draw_box_batch_dimension2_d(...)` | `function renderer:draw_box_batch_dimension2_d(transform, colour, count) end` | function |
| `Moss_RendererDrawCircleBatch2D` | `renderer:draw_circle_batch_dimension2_d(...)` | `function renderer:draw_circle_batch_dimension2_d(transform, colour, count) end` | function |
| `Moss_RendererDrawArrowBatch2D` | `renderer:draw_arrow_batch_dimension2_d(...)` | `function renderer:draw_arrow_batch_dimension2_d(transform, colour, count) end` | function |
| `Moss_RendererDrawMarkerBatch2D` | `renderer:draw_marker_batch_dimension2_d(...)` | `function renderer:draw_marker_batch_dimension2_d(transform, colour, count) end` | function |
| `Moss_RendererDrawBoxInstanced2D` | `renderer:draw_box_instanced_dimension2_d(...)` | `function renderer:draw_box_instanced_dimension2_d(size, transform, colour, count) end` | function |
| `Moss_RendererDrawCircleInstanced2D` | `renderer:draw_circle_instanced_dimension2_d(...)` | `function renderer:draw_circle_instanced_dimension2_d(radius, transform, colour, count) end` | function |
| `Moss_RendererDrawCapsuleInstanced2D` | `renderer:draw_capsule_instanced_dimension2_d(...)` | `function renderer:draw_capsule_instanced_dimension2_d(half_height_of_cylinder, radius, transform, colour, count) end` | function |
| `Moss_RendererDrawMarkerInstanced2D` | `renderer:draw_marker_instanced_dimension2_d(...)` | `function renderer:draw_marker_instanced_dimension2_d(size, transform, colour, count) end` | function |
| `Moss_RendererDrawGizmoInstanced2D` | `renderer:draw_gizmo_instanced_dimension2_d(...)` | `function renderer:draw_gizmo_instanced_dimension2_d(size, transform, count) end` | function |
| `Moss_RendererDrawLine3D` | `renderer:draw_line_dimension3_d(...)` | `function renderer:draw_line_dimension3_d(from, to, color, thickness) end` | function |
| `Moss_RendererDrawLines3D` | `renderer:draw_lines_dimension3_d(...)` | `function renderer:draw_lines_dimension3_d(positions, color, thickness) end` | function |
| `Moss_RendererDrawBox3D` | `renderer:draw_box_dimension3_d(...)` | `function renderer:draw_box_dimension3_d(center, rotation, color) end` | function |
| `Moss_RendererDrawSphere3D` | `renderer:draw_sphere_dimension3_d(...)` | `function renderer:draw_sphere_dimension3_d(center, radius, color) end` | function |
| `Moss_RendererDrawCylinder3D` | `renderer:draw_cylinder_dimension3_d(...)` | `function renderer:draw_cylinder_dimension3_d(base, top, radius, color) end` | function |
| `Moss_RendererDrawCone3D` | `renderer:draw_cone_dimension3_d(...)` | `function renderer:draw_cone_dimension3_d(base, tip, radius, color) end` | function |
| `Moss_RendererDrawTriangle3D` | `renderer:draw_triangle_dimension3_d(...)` | `function renderer:draw_triangle_dimension3_d(p0, p1, p2, color) end` | function |
| `Moss_RendererDrawArrow3D` | `renderer:draw_arrow_dimension3_d(...)` | `function renderer:draw_arrow_dimension3_d(from, to, color, head_size) end` | function |
| `Moss_RendererDrawMarker3D` | `renderer:draw_marker_dimension3_d(...)` | `function renderer:draw_marker_dimension3_d(position, color, size) end` | function |
| `Moss_RendererDrawPlane3D` | `renderer:draw_plane_dimension3_d(...)` | `function renderer:draw_plane_dimension3_d(center, normal, color) end` | function |
| `Moss_RendererDrawBone3D` | `renderer:draw_bone_dimension3_d(...)` | `function renderer:draw_bone_dimension3_d(transform, length, color) end` | function |
| `Moss_RendererDrawCoordinateSystem3D` | `renderer:draw_coordinate_system_dimension3_d(...)` | `function renderer:draw_coordinate_system_dimension3_d(matrix, size) end` | function |
| `Moss_RendererDrawCapsule3D` | `renderer:draw_capsule_dimension3_d(...)` | `function renderer:draw_capsule_dimension3_d(matrix, half_height_of_cylinder, radius, color) end` | function |
| `Moss_RendererDrawConvex3D` | `renderer:draw_convex_dimension3_d(...)` | `function renderer:draw_convex_dimension3_d(points, color) end` | function |
| `Moss_RendererDrawConvexHull3D` | `renderer:draw_convex_hull_dimension3_d(...)` | `function renderer:draw_convex_hull_dimension3_d(points, color) end` | function |
| `Moss_RendererDrawPolygonShape3D` | `renderer:draw_polygon_shape_dimension3_d(...)` | `function renderer:draw_polygon_shape_dimension3_d(points, color) end` | function |
| `Moss_RendererDrawGizmo3D` | `renderer:draw_gizmo_dimension3_d(...)` | `function renderer:draw_gizmo_dimension3_d(transform, size) end` | function |
| `Moss_RendererDrawLineBatch3D` | `renderer:draw_line_batch_dimension3_d(...)` | `function renderer:draw_line_batch_dimension3_d(transform, colour, count) end` | function |
| `Moss_RendererDrawBoxBatch3D` | `renderer:draw_box_batch_dimension3_d(...)` | `function renderer:draw_box_batch_dimension3_d(transform, colour, count) end` | function |
| `Moss_RendererDrawSphereBatch3D` | `renderer:draw_sphere_batch_dimension3_d(...)` | `function renderer:draw_sphere_batch_dimension3_d(transform, colour, count) end` | function |
| `Moss_RendererDrawArrowBatch3D` | `renderer:draw_arrow_batch_dimension3_d(...)` | `function renderer:draw_arrow_batch_dimension3_d(transform, colour, count) end` | function |
| `Moss_RendererDrawMarkerBatch3D` | `renderer:draw_marker_batch_dimension3_d(...)` | `function renderer:draw_marker_batch_dimension3_d(transform, colour, count) end` | function |
| `Moss_RendererDrawBoxInstanced3D` | `renderer:draw_box_instanced_dimension3_d(...)` | `function renderer:draw_box_instanced_dimension3_d(size, transform, colour, count) end` | function |
| `Moss_RendererDrawSphereInstanced3D` | `renderer:draw_sphere_instanced_dimension3_d(...)` | `function renderer:draw_sphere_instanced_dimension3_d(radius, transform, colour, count) end` | function |
| `Moss_RendererDrawCapsuleInstanced3D` | `renderer:draw_capsule_instanced_dimension3_d(...)` | `function renderer:draw_capsule_instanced_dimension3_d(half_height_of_cylinder, radius, transform, colour, count) end` | function |
| `Moss_RendererDrawCylinderInstanced3D` | `renderer:draw_cylinder_instanced_dimension3_d(...)` | `function renderer:draw_cylinder_instanced_dimension3_d(half_height, radius, transform, colour, count) end` | function |
| `Moss_RendererDrawConeInstanced3D` | `renderer:draw_cone_instanced_dimension3_d(...)` | `function renderer:draw_cone_instanced_dimension3_d(height, radius, transform, colour, count) end` | function |
| `Moss_RendererDrawMarkerInstanced3D` | `renderer:draw_marker_instanced_dimension3_d(...)` | `function renderer:draw_marker_instanced_dimension3_d(size, transform, colour, count) end` | function |
| `Moss_RendererDrawGizmoInstanced3D` | `renderer:draw_gizmo_instanced_dimension3_d(...)` | `function renderer:draw_gizmo_instanced_dimension3_d(size, transform, count) end` | function |
| `Moss_VulkanGetDevice` | `renderer:get_device(...)` | `function renderer:get_device() end` | function |
| `Moss_VulkanGetDescriptorPool` | `renderer:get_descriptor_pool(...)` | `function renderer:get_descriptor_pool() end` | function |
| `Moss_VulkanGetDescriptorSetLayoutTexture` | `renderer:get_descriptor_set_layout_texture(...)` | `function renderer:get_descriptor_set_layout_texture() end` | function |
| `Moss_VulkanGetTextureSamplerRepeat` | `renderer:get_texture_sampler_repeat(...)` | `function renderer:get_texture_sampler_repeat() end` | function |
| `Moss_VulkanGetTextureSamplerShadow` | `renderer:get_texture_sampler_shadow(...)` | `function renderer:get_texture_sampler_shadow() end` | function |
| `Moss_VulkanGetRenderPassShadow` | `renderer:get_render_pass_shadow(...)` | `function renderer:get_render_pass_shadow() end` | function |
| `Moss_VulkanGetRenderPass` | `renderer:get_render_pass(...)` | `function renderer:get_render_pass() end` | function |
| `Moss_VulkanGetPipelineLayout` | `renderer:get_pipeline_layout(...)` | `function renderer:get_pipeline_layout() end` | function |
| `Moss_VulkanGetCommandBuffer` | `renderer:get_command_buffer(...)` | `function renderer:get_command_buffer() end` | function |
| `Moss_VulkanStartTempCommandBuffer` | `renderer:start_temp_command_buffer(...)` | `function renderer:start_temp_command_buffer() end` | function |
| `Moss_VulkanEndTempCommandBuffer` | `renderer:end_temp_command_buffer(...)` | `function renderer:end_temp_command_buffer(command_buffer) end` | function |
| `Moss_VulkanAllocateMemory` | `renderer:allocate_memory(...)` | `function renderer:allocate_memory(size, memory_type_bits, properties, out_memory) end` | function |
| `Moss_VulkanFreeMemory` | `renderer:free_memory(...)` | `function renderer:free_memory(memory, size) end` | function |
| `Moss_VulkanCreateBuffer` | `renderer:create_buffer(...)` | `function renderer:create_buffer(size, usage, properties, out_buffer) end` | function |
| `Moss_VulkanCopyBuffer` | `renderer:copy_buffer(...)` | `function renderer:copy_buffer(src, dst, size) end` | function |
| `Moss_VulkanCreateDeviceLocalBuffer` | `renderer:create_device_local_buffer(...)` | `function renderer:create_device_local_buffer(data, size, usage, out_buffer) end` | function |
| `Moss_VulkanFreeBuffer` | `renderer:free_buffer(...)` | `function renderer:free_buffer(buffer) end` | function |
| `Moss_VulkanCreateConstantBuffer` | `renderer:create_constant_buffer(...)` | `function renderer:create_constant_buffer(buffer_size) end` | function |
| `Moss_VulkanCreateImage` | `renderer:create_image(...)` | `function renderer:create_image(width, height, format, tiling, usage, properties, out_image, out_memory) end` | function |
| `Moss_VulkanDestroyImage` | `renderer:destroy_image(...)` | `function renderer:destroy_image(image, memory) end` | function |
| `Moss_VulkanCreateImageView` | `renderer:create_image_view(...)` | `function renderer:create_image_view(image, format, aspect_flags) end` | function |
| `Moss_VulkanFindDepthFormat` | `renderer:find_depth_format(...)` | `function renderer:find_depth_format() end` | function |
| `Moss_Vulkan_EncodeSPIRV` | `moss.renderer.encode_spirv(...)` | `function moss.renderer.encode_spirv(path) end` | function |
| `Moss_Vulkan_DecodeSPIRV` | `moss.renderer.decode_spirv(...)` | `function moss.renderer.decode_spirv(path) end` | function |
| `Moss_DX12GetDevice` | `renderer:dx12_get_device(...)` | `function renderer:dx12_get_device() end` | function |
| `Moss_DX12GetRootSignature` | `renderer:dx12_get_root_signature(...)` | `function renderer:dx12_get_root_signature() end` | function |
| `Moss_DX12GetCommandList` | `renderer:dx12_get_command_list(...)` | `function renderer:dx12_get_command_list() end` | function |
| `Moss_DX12GetUploadQueue` | `renderer:dx12_get_upload_queue(...)` | `function renderer:dx12_get_upload_queue() end` | function |
| `Moss_DX12GetDSVHeap` | `renderer:dx12_get_dsv_heap(...)` | `function renderer:dx12_get_dsv_heap() end` | function |
| `Moss_DX12GetSRVHeap` | `renderer:dx12_get_srv_heap(...)` | `function renderer:dx12_get_srv_heap() end` | function |
| `Moss_DX_CompileHLSL` | `moss.renderer.compile_hlsl(...)` | `function moss.renderer.compile_hlsl(input_path, output_path, entry_point, target) end` | function |
| `Moss_DX_EnableDebugLayer` | `moss.renderer.enable_debug_layer(...)` | `function moss.renderer.enable_debug_layer(enable) end` | function |
| `Moss_DX_GetShaderModel` | `moss.renderer.get_shader_model(...)` | `function moss.renderer.get_shader_model() end` | function |
| `Moss_MetalGetView` | `renderer:get_view(...)` | `function renderer:get_view() end` | function |
| `Moss_MetalGetDevice` | `renderer:get_device(...)` | `function renderer:get_device() end` | function |
| `Moss_MetalGetRenderEncoder` | `renderer:get_render_encoder(...)` | `function renderer:get_render_encoder() end` | function |
| `Moss_Metal_CompileMSL` | `moss.renderer.compile_msl(...)` | `function moss.renderer.compile_msl(input_path, output_path) end` | function |
| `Moss_Metal_SupportsFamily` | `moss.renderer.supports_family(...)` | `function moss.renderer.supports_family(family) end` | function |

### `Moss_stdinc.h` → `moss.core`

| Native symbol (traceability) | Public API projection | Target-language signature | Kind |
|---|---|---|---|
| `Allocate` | `moss.core.allocator.allocate` | `moss.core.allocator.allocate = function(...) end` | callback |
| `Reallocate` | `moss.core.allocator.reallocate` | `moss.core.allocator.reallocate = function(...) end` | callback |
| `Free` | `moss.core.allocator.free` | `moss.core.allocator.free = function(...) end` | callback |
| `AlignedAllocate` | `moss.core.allocator.aligned_allocate` | `moss.core.allocator.aligned_allocate = function(...) end` | callback |
| `AlignedFree` | `moss.core.allocator.aligned_free` | `moss.core.allocator.aligned_free = function(...) end` | callback |
| `RegisterDefaultAllocator` | `moss.core.register_default_allocator(...)` | `function moss.core.register_default_allocator() end` | function |
| `Allocate` | `moss.core.allocate(...)` | `function moss.core.allocate(in_size) end` | function |
| `Reallocate` | `moss.core.reallocate(...)` | `function moss.core.reallocate(in_block, in_old_size, in_new_size) end` | function |
| `Free` | `moss.core.free(...)` | `function moss.core.free(in_block) end` | function |
| `AlignedAllocate` | `moss.core.aligned_allocate(...)` | `function moss.core.aligned_allocate(in_size, in_alignment) end` | function |
| `AlignedFree` | `moss.core.aligned_free(...)` | `function moss.core.aligned_free(in_block) end` | function |
| `Moss_rand` | `moss.core.rand(...)` | `function moss.core.rand(n) end` | function |
| `Moss_randf` | `moss.core.randf(...)` | `function moss.core.randf() end` | function |
| `Moss_randi` | `moss.core.randi(...)` | `function moss.core.randi() end` | function |
| `Moss_srand` | `moss.core.srand(...)` | `function moss.core.srand(seed) end` | function |
| `Moss_randi_range` | `moss.core.randi_range(...)` | `function moss.core.randi_range(from, to) end` | function |
| `Moss_randf_range` | `moss.core.randf_range(...)` | `function moss.core.randf_range(from, to) end` | function |
| `Moss_rand_gaussian` | `moss.core.rand_gaussian(...)` | `function moss.core.rand_gaussian(mean, stddev) end` | function |
| `Moss_rand_unit` | `moss.core.rand_unit(...)` | `function moss.core.rand_unit() end` | function |
| `Moss_GetTicks` | `moss.core.get_ticks(...)` | `function moss.core.get_ticks() end` | function |
| `Moss_GetSeconds` | `moss.core.get_seconds(...)` | `function moss.core.get_seconds(time) end` | function |
| `Moss_GetMilliseconds` | `moss.core.get_milliseconds(...)` | `function moss.core.get_milliseconds(ticks) end` | function |
| `Moss_GetMillisecondsAndReset` | `moss.core.get_milliseconds_and_reset(...)` | `function moss.core.get_milliseconds_and_reset(ticks) end` | function |
| `Moss_Yield` | `moss.core.yield(...)` | `function moss.core.yield() end` | function |
| `Moss_Delay` | `moss.core.delay(...)` | `function moss.core.delay(delay) end` | function |
| `Moss_LocalTime` | `moss.core.local_time(...)` | `function moss.core.local_time() end` | function |
| `Moss_TimeStamp` | `moss.core.time_stamp(...)` | `function moss.core.time_stamp() end` | function |
| `Moss_TimeNow` | `moss.core.time_now(...)` | `function moss.core.time_now() end` | function |
| `Moss_CTimeNow` | `moss.core.c_time_now(...)` | `function moss.core.c_time_now() end` | function |
| `Moss_TimeStampET` | `moss.core.time_stamp_et(...)` | `function moss.core.time_stamp_et() end` | function |
| `Moss_TimeStampCT` | `moss.core.time_stamp_ct(...)` | `function moss.core.time_stamp_ct() end` | function |
| `Moss_TimeStampMT` | `moss.core.time_stamp_mt(...)` | `function moss.core.time_stamp_mt() end` | function |
| `Moss_TimeStampPT` | `moss.core.time_stamp_pt(...)` | `function moss.core.time_stamp_pt() end` | function |
| `Moss_FormatTime` | `moss.core.format_time(...)` | `function moss.core.format_time(fmt) end` | function |

### `Moss_XR.h` → `moss.xr`

| Native symbol (traceability) | Public API projection | Target-language signature | Kind |
|---|---|---|---|
| `Moss_XR_Initialize` | `moss.xr.initialize(...)` | `function moss.xr.initialize() end` | function |
| `Moss_XR_Shutdown` | `moss.xr.shutdown(...)` | `function moss.xr.shutdown() end` | function |
| `Moss_XR_GetCapabilities` | `moss.xr.get_capabilities(...)` | `function moss.xr.get_capabilities() end` | function |
| `Moss_XR_EnableValidationLayers` | `moss.xr.enable_validation_layers(...)` | `function moss.xr.enable_validation_layers() end` | function |
| `Moss_XR_CreateSession` | `moss.xr.create_session(...)` | `function moss.xr.create_session() end` | function |
| `Moss_XR_DestroySession` | `session:close(...)` | `function session:close() end` | function |
| `Moss_XR_GetSessionState` | `session:get_state(...)` | `function session:get_state() end` | function |
| `Moss_XR_BeginFrame` | `session:begin_frame(...)` | `function session:begin_frame() -> boolean end` | function |
| `Moss_XR_EndFrame` | `session:end_frame(...)` | `function session:end_frame() end` | function |
| `Moss_XR_GetPredictedDisplayTime` | `session:get_predicted_display_time(...)` | `function session:get_predicted_display_time() end` | function |
| `Moss_XR_GetDeltaSeconds` | `moss.xr.get_delta_seconds(...)` | `function moss.xr.get_delta_seconds() end` | function |
| `Moss_XR_HandleEvents` | `session:handle_events(...)` | `function session:handle_events() end` | function |
| `Moss_XR_GetViewCount` | `session:get_view_count(...)` | `function session:get_view_count() end` | function |
| `Moss_XR_GetView` | `session:get_view(...)` | `function session:get_view(index, out_view) end` | function |
| `Moss_XR_CreateActionSet` | `moss.xr.create_action_set(...)` | `function moss.xr.create_action_set(name) end` | function |
| `Moss_XR_DestroyActionSet` | `set:close(...)` | `function set:close() end` | function |
| `Moss_XR_CreateAction` | `set:create_action(...)` | `function set:create_action(name, type) end` | function |
| `Moss_XR_DestroyAction` | `action:close(...)` | `function action:close() end` | function |
| `Moss_XR_AttachActionSet` | `set:attach(...)` | `function set:attach(session) end` | function |
| `Moss_XR_SyncActions` | `session:sync_actions(...)` | `function session:sync_actions() end` | function |
| `Moss_XR_GetActionBoolean` | `action:get_boolean(...)` | `function action:get_boolean() end` | function |
| `Moss_XR_GetActionFloat` | `action:get_float(...)` | `function action:get_float() end` | function |
| `Moss_XR_GetActionPose` | `action:get_pose(...)` | `function action:get_pose(out_pose) end` | function |
| `Moss_XR_StopHaptic` | `action:stop_haptic(...)` | `function action:stop_haptic() end` | function |
| `Moss_XR_CreateHand` | `moss.xr.create_hand(...)` | `function moss.xr.create_hand() end` | function |
| `Moss_XR_GetHandJointPose` | `hand:get_hand_joint_pose(...)` | `function hand:get_hand_joint_pose(joint, out_pose) end` | function |
| `Moss_XR_DestroyHand` | `hand:destroy_hand(...)` | `function hand:destroy_hand() end` | function |
| `Moss_XR_GetHandPose` | `hand:get_hand_pose(...)` | `function hand:get_hand_pose(out_pose) end` | function |
| `Moss_XR_CreateBody` | `moss.xr.create_body(...)` | `function moss.xr.create_body() end` | function |
| `Moss_XR_GetBodyJointPose` | `body:get_body_joint_pose(...)` | `function body:get_body_joint_pose(joint, out_pose) end` | function |
| `Moss_XR_DestroyBody` | `body:destroy_body(...)` | `function body:destroy_body() end` | function |
| `Moss_XR_CreateFace` | `moss.xr.create_face(...)` | `function moss.xr.create_face() end` | function |
| `Moss_XR_DestroyFace` | `face:destroy_face(...)` | `function face:destroy_face() end` | function |
| `Moss_XR_UpdateBodyTracking` | `session:update_body_tracking(...)` | `function session:update_body_tracking() end` | function |
| `Moss_XR_UpdateHandTracking` | `session:update_hand_tracking(...)` | `function session:update_hand_tracking() end` | function |
| `Moss_XR_EnablePassthrough` | `moss.xr.enable_passthrough(...)` | `function moss.xr.enable_passthrough(enable) end` | function |
| `Moss_XR_EnableFoveatedRendering` | `moss.xr.enable_foveated_rendering(...)` | `function moss.xr.enable_foveated_rendering(enable) end` | function |
| `Moss_XR_GetBackendName` | `moss.xr.get_backend_name(...)` | `function moss.xr.get_backend_name() end` | function |
| `Moss_XR_GetBackendVersion` | `moss.xr.get_backend_version(...)` | `function moss.xr.get_backend_version() end` | function |
| `Moss_XR_BeginDebugLabel` | `moss.xr.begin_debug_label(...)` | `function moss.xr.begin_debug_label(label) end` | function |
| `Moss_XR_EndDebugLabel` | `moss.xr.end_debug_label(...)` | `function moss.xr.end_debug_label() end` | function |


## 8. Native header and feature coverage

The public binding must cover the complete Moss header tree, not only the exported function index. Many Core, Geometry, Physics, Renderer, Resources, Skeleton, TriangleSplitter and Variants APIs are class members, templates, inline methods, or declarations not represented by the top-level free-function catalogue.

## Complete header inventory

Each link opens the corresponding source file in `include/Moss/`. The rightmost count shows declarations beginning with `MOSS_API` in that header; zero does not mean the header has no public API (it may expose class members, templates, inline methods, typedefs, or preprocessor declarations).

### Core (40 headers)

Foundational C++ utilities.

| Header | Export-marked declarations |
|---|---:|
| [`Core/BinaryHeap.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/BinaryHeap.h) | 0 |
| [`Core/ByteBuffer.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/ByteBuffer.h) | 0 |
| [`Core/FPControlWord.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/FPControlWord.h) | 0 |
| [`Core/FPException.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/FPException.h) | 0 |
| [`Core/FPFlushDenormals.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/FPFlushDenormals.h) | 0 |
| [`Core/Factory.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/Factory.h) | 0 |
| [`Core/FixedSizeFreeList.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/FixedSizeFreeList.h) | 0 |
| [`Core/FreeBlockAlloc.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/FreeBlockAlloc.h) | 0 |
| [`Core/HashCombine.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/HashCombine.h) | 0 |
| [`Core/HashTable.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/HashTable.h) | 0 |
| [`Core/InsertionSort.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/InsertionSort.h) | 0 |
| [`Core/JobSystem.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/JobSystem.h) | 0 |
| [`Core/JobSystemSingleThreaded.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/JobSystemSingleThreaded.h) | 0 |
| [`Core/JobSystemThreadPool.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/JobSystemThreadPool.h) | 0 |
| [`Core/JobSystemWithBarrier.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/JobSystemWithBarrier.h) | 0 |
| [`Core/LinearCurve.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/LinearCurve.h) | 0 |
| [`Core/LockFreeHashMap.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/LockFreeHashMap.h) | 0 |
| [`Core/Memory.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/Memory.h) | 11 |
| [`Core/Mutex.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/Mutex.h) | 0 |
| [`Core/MutexArray.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/MutexArray.h) | 0 |
| [`Core/NonCopyable.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/NonCopyable.h) | 0 |
| [`Core/Profiler.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/Profiler.h) | 4 |
| [`Core/QuickSort.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/QuickSort.h) | 0 |
| [`Core/RTTI.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/RTTI.h) | 0 |
| [`Core/Reference.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/Reference.h) | 0 |
| [`Core/Result.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/Result.h) | 0 |
| [`Core/STLAlignedAllocator.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/STLAlignedAllocator.h) | 0 |
| [`Core/STLAllocator.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/STLAllocator.h) | 0 |
| [`Core/STLLocalAllocator.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/STLLocalAllocator.h) | 0 |
| [`Core/STLTempAllocator.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/STLTempAllocator.h) | 0 |
| [`Core/ScopeExit.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/ScopeExit.h) | 0 |
| [`Core/Semaphore.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/Semaphore.h) | 0 |
| [`Core/StreamIn.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/StreamIn.h) | 0 |
| [`Core/StreamOut.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/StreamOut.h) | 0 |
| [`Core/StreamUtils.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/StreamUtils.h) | 0 |
| [`Core/StreamWrapper.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/StreamWrapper.h) | 0 |
| [`Core/StridedPtr.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/StridedPtr.h) | 0 |
| [`Core/StringTools.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/StringTools.h) | 6 |
| [`Core/TempAllocator.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/TempAllocator.h) | 0 |
| [`Core/TickCounter.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/TickCounter.h) | 0 |

### Geometry (9 headers)

Geometry algorithms and primitives.

| Header | Export-marked declarations |
|---|---:|
| [`Geometry/ClipPoly.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Geometry/ClipPoly.h) | 0 |
| [`Geometry/ClosestPoint.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Geometry/ClosestPoint.h) | 0 |
| [`Geometry/ConvexHullBuilder.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Geometry/ConvexHullBuilder.h) | 0 |
| [`Geometry/ConvexSupport.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Geometry/ConvexSupport.h) | 0 |
| [`Geometry/EPAConvexHullBuilder.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Geometry/EPAConvexHullBuilder.h) | 0 |
| [`Geometry/EPAPenetrationDepth.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Geometry/EPAPenetrationDepth.h) | 0 |
| [`Geometry/GJKClosestPoint.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Geometry/GJKClosestPoint.h) | 0 |
| [`Geometry/RayCylinder.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Geometry/RayCylinder.h) | 0 |
| [`Geometry/Triangle.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Geometry/Triangle.h) | 0 |

### Physics (69 headers)

Simulation, body, shape, collision, constraint and query implementation headers.

| Header | Export-marked declarations |
|---|---:|
| [`Physics/AABBTree/AABBTreeBuilder.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/AABBTree/AABBTreeBuilder.h) | 0 |
| [`Physics/AABBTree/AABBTreeToBuffer.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/AABBTree/AABBTreeToBuffer.h) | 0 |
| [`Physics/AABBTree/NodeCodec/NodeCodecQuadTreeHalfFloat.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/AABBTree/NodeCodec/NodeCodecQuadTreeHalfFloat.h) | 0 |
| [`Physics/AABBTree/NodeCodec/NodeCodecQuadTreeHalfFloat2D.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/AABBTree/NodeCodec/NodeCodecQuadTreeHalfFloat2D.h) | 0 |
| [`Physics/AABBTree/TriangleCodec/TriangleCodecIndexed8BitPackSOA4Flags.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/AABBTree/TriangleCodec/TriangleCodecIndexed8BitPackSOA4Flags.h) | 0 |
| [`Physics/AABBTree/TriangleCodec/TriangleCodecIndexed8BitPackSOA4Flags2D.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/AABBTree/TriangleCodec/TriangleCodecIndexed8BitPackSOA4Flags2D.h) | 0 |
| [`Physics/Body/Body.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Body/Body.h) | 1 |
| [`Physics/Body/BodyID.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Body/BodyID.h) | 0 |
| [`Physics/Character/Character.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Character/Character.h) | 0 |
| [`Physics/Cloth/Cloth.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Cloth/Cloth.h) | 0 |
| [`Physics/Collision/ActiveEdges.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/ActiveEdges.h) | 0 |
| [`Physics/Collision/BroadPhase/BroadPhase.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/BroadPhase/BroadPhase.h) | 0 |
| [`Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceMask.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceMask.h) | 0 |
| [`Physics/Collision/BroadPhase/QuadTree.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/BroadPhase/QuadTree.h) | 0 |
| [`Physics/Collision/CastConvexVsTriangles.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CastConvexVsTriangles.h) | 0 |
| [`Physics/Collision/CastResult.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CastResult.h) | 0 |
| [`Physics/Collision/CastSphereVsTriangles.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CastSphereVsTriangles.h) | 0 |
| [`Physics/Collision/CollideConvexVsTriangles.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollideConvexVsTriangles.h) | 0 |
| [`Physics/Collision/CollidePointResult.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollidePointResult.h) | 0 |
| [`Physics/Collision/CollideShape.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollideShape.h) | 0 |
| [`Physics/Collision/CollideShapeVsShapePerLeaf.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollideShapeVsShapePerLeaf.h) | 0 |
| [`Physics/Collision/CollideSoftBodyVertexIterator.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollideSoftBodyVertexIterator.h) | 0 |
| [`Physics/Collision/CollideSoftBodyVerticesVsTriangles.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollideSoftBodyVerticesVsTriangles.h) | 0 |
| [`Physics/Collision/CollideSphereVsTriangles.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollideSphereVsTriangles.h) | 0 |
| [`Physics/Collision/CollisionCollector.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollisionCollector.h) | 0 |
| [`Physics/Collision/CollisionCollectorImpl.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollisionCollectorImpl.h) | 0 |
| [`Physics/Collision/CollisionDispatch.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollisionDispatch.h) | 0 |
| [`Physics/Collision/CollisionGroup.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollisionGroup.h) | 0 |
| [`Physics/Collision/ContactListener.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/ContactListener.h) | 0 |
| [`Physics/Collision/EstimateCollisionResponse.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/EstimateCollisionResponse.h) | 1 |
| [`Physics/Collision/GroupFilter.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/GroupFilter.h) | 0 |
| [`Physics/Collision/GroupFilterTable.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/GroupFilterTable.h) | 0 |
| [`Physics/Collision/InternalEdgeRemovingCollector.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/InternalEdgeRemovingCollector.h) | 0 |
| [`Physics/Collision/ManifoldBetweenTwoFaces.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/ManifoldBetweenTwoFaces.h) | 2 |
| [`Physics/Collision/NarrowPhaseQuery.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/NarrowPhaseQuery.h) | 0 |
| [`Physics/Collision/NarrowPhaseStats.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/NarrowPhaseStats.h) | 0 |
| [`Physics/Collision/ObjectLayer.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/ObjectLayer.h) | 0 |
| [`Physics/Collision/ObjectLayerPairFilterMask.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/ObjectLayerPairFilterMask.h) | 0 |
| [`Physics/Collision/ObjectLayerPairFilterTable.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/ObjectLayerPairFilterTable.h) | 0 |
| [`Physics/Collision/PhysicsMaterial.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/PhysicsMaterial.h) | 0 |
| [`Physics/Collision/PhysicsMaterialSimple.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/PhysicsMaterialSimple.h) | 0 |
| [`Physics/Collision/RayCast.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/RayCast.h) | 0 |
| [`Physics/Collision/Shape/CompoundShapeVisitors.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/Shape/CompoundShapeVisitors.h) | 0 |
| [`Physics/Collision/Shape/ConvexHullShape.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/Shape/ConvexHullShape.h) | 0 |
| [`Physics/Collision/Shape/GetTrianglesContext.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/Shape/GetTrianglesContext.h) | 0 |
| [`Physics/Collision/Shape/PolyhedronSubmergedVolumeCalculator.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/Shape/PolyhedronSubmergedVolumeCalculator.h) | 0 |
| [`Physics/Collision/Shape/Shapes.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/Shape/Shapes.h) | 0 |
| [`Physics/Collision/Shape/SubShapeID.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/Shape/SubShapeID.h) | 0 |
| [`Physics/Collision/Shape/SubShapeIDPair.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/Shape/SubShapeIDPair.h) | 0 |
| [`Physics/Collision/ShapeCast.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/ShapeCast.h) | 0 |
| [`Physics/Collision/ShapeFilter.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/ShapeFilter.h) | 0 |
| [`Physics/Collision/SimShapeFilter.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/SimShapeFilter.h) | 0 |
| [`Physics/Collision/SimShapeFilterWrapper.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/SimShapeFilterWrapper.h) | 0 |
| [`Physics/Collision/SortReverseAndStore.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/SortReverseAndStore.h) | 0 |
| [`Physics/Collision/TransformedShape.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/TransformedShape.h) | 0 |
| [`Physics/Constraints/ConstraintManager.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Constraints/ConstraintManager.h) | 0 |
| [`Physics/Constraints/Constraints.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Constraints/Constraints.h) | 0 |
| [`Physics/Hair/Hair.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Hair/Hair.h) | 0 |
| [`Physics/IslandBuilder.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/IslandBuilder.h) | 0 |
| [`Physics/LargeIslandSplitter.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/LargeIslandSplitter.h) | 0 |
| [`Physics/PhysicsLock.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/PhysicsLock.h) | 0 |
| [`Physics/PhysicsScene.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/PhysicsScene.h) | 0 |
| [`Physics/PhysicsSystem.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/PhysicsSystem.h) | 0 |
| [`Physics/PhysicsUpdateContext.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/PhysicsUpdateContext.h) | 0 |
| [`Physics/Ragdoll/Ragdoll.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Ragdoll/Ragdoll.h) | 0 |
| [`Physics/SoftBody/Softbody.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/SoftBody/Softbody.h) | 0 |
| [`Physics/StateRecorder.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/StateRecorder.h) | 0 |
| [`Physics/StateRecorderImpl.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/StateRecorderImpl.h) | 0 |
| [`Physics/Vehicle/Vehicle.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Vehicle/Vehicle.h) | 0 |

### Renderer (65 headers)

Renderer resources and graphics backend headers.

| Header | Export-marked declarations |
|---|---:|
| [`Renderer/DX12/CommandQueueDX12.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DX12/CommandQueueDX12.h) | 0 |
| [`Renderer/DX12/ConstantBufferDX12.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DX12/ConstantBufferDX12.h) | 0 |
| [`Renderer/DX12/DescriptorHeapDX12.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DX12/DescriptorHeapDX12.h) | 0 |
| [`Renderer/DX12/LightingDX12.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DX12/LightingDX12.h) | 0 |
| [`Renderer/DX12/PipelineStateDX12.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DX12/PipelineStateDX12.h) | 0 |
| [`Renderer/DX12/RenderInstancesDX12.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DX12/RenderInstancesDX12.h) | 0 |
| [`Renderer/DX12/RenderPrimitiveDX12.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DX12/RenderPrimitiveDX12.h) | 0 |
| [`Renderer/DX12/RendererDX12.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DX12/RendererDX12.h) | 0 |
| [`Renderer/DX12/ShaderDX12.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DX12/ShaderDX12.h) | 0 |
| [`Renderer/DX12/TextureDX12.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DX12/TextureDX12.h) | 0 |
| [`Renderer/DebugRenderer.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DebugRenderer.h) | 0 |
| [`Renderer/DebugRendererPlayback.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DebugRendererPlayback.h) | 0 |
| [`Renderer/DebugRendererRecorder.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DebugRendererRecorder.h) | 0 |
| [`Renderer/DebugRendererSimple.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DebugRendererSimple.h) | 0 |
| [`Renderer/FogVolume.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/FogVolume.h) | 0 |
| [`Renderer/Font.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Font.h) | 0 |
| [`Renderer/GL/FontGL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/FontGL.h) | 0 |
| [`Renderer/GL/LightingGL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/LightingGL.h) | 0 |
| [`Renderer/GL/MeshGL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/MeshGL.h) | 0 |
| [`Renderer/GL/PipelineStateGL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/PipelineStateGL.h) | 0 |
| [`Renderer/GL/PostProcess.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/PostProcess.h) | 0 |
| [`Renderer/GL/ReflectionProbe.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/ReflectionProbe.h) | 0 |
| [`Renderer/GL/Renderer_GL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/Renderer_GL.h) | 0 |
| [`Renderer/GL/ShaderGL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/ShaderGL.h) | 0 |
| [`Renderer/GL/StorageBufferGL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/StorageBufferGL.h) | 0 |
| [`Renderer/GL/SubViewportGL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/SubViewportGL.h) | 0 |
| [`Renderer/GL/SurfaceGL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/SurfaceGL.h) | 0 |
| [`Renderer/GL/TextureGL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/TextureGL.h) | 0 |
| [`Renderer/GL/glad.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/glad.h) | 0 |
| [`Renderer/MTL/PipelineStateMTL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/MTL/PipelineStateMTL.h) | 0 |
| [`Renderer/MTL/RenderInstancesMTL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/MTL/RenderInstancesMTL.h) | 0 |
| [`Renderer/MTL/RenderPrimitiveMTL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/MTL/RenderPrimitiveMTL.h) | 0 |
| [`Renderer/MTL/RendererMTL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/MTL/RendererMTL.h) | 0 |
| [`Renderer/MTL/ShaderMTL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/MTL/ShaderMTL.h) | 0 |
| [`Renderer/MTL/TextureMTL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/MTL/TextureMTL.h) | 0 |
| [`Renderer/Managers/DecalManager.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Managers/DecalManager.h) | 0 |
| [`Renderer/Managers/FontManager.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Managers/FontManager.h) | 0 |
| [`Renderer/Managers/LightingManager.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Managers/LightingManager.h) | 0 |
| [`Renderer/Managers/MeshManager3D.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Managers/MeshManager3D.h) | 0 |
| [`Renderer/Managers/ParticleEffectManager.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Managers/ParticleEffectManager.h) | 0 |
| [`Renderer/Managers/SurfaceManager.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Managers/SurfaceManager.h) | 0 |
| [`Renderer/Material2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Material2.h) | 0 |
| [`Renderer/Material3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Material3.h) | 0 |
| [`Renderer/Mesh.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Mesh.h) | 0 |
| [`Renderer/Model.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Model.h) | 0 |
| [`Renderer/PipelineState.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/PipelineState.h) | 0 |
| [`Renderer/Renderer.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Renderer.h) | 0 |
| [`Renderer/Shader.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Shader.h) | 0 |
| [`Renderer/SkyBox.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/SkyBox.h) | 0 |
| [`Renderer/StorageBuffer.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/StorageBuffer.h) | 0 |
| [`Renderer/SubViewport.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/SubViewport.h) | 0 |
| [`Renderer/Surface.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Surface.h) | 0 |
| [`Renderer/Texture.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Texture.h) | 0 |
| [`Renderer/VK/ConstantBufferVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/ConstantBufferVK.h) | 0 |
| [`Renderer/VK/LightingManagerVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/LightingManagerVK.h) | 0 |
| [`Renderer/VK/MeshVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/MeshVK.h) | 0 |
| [`Renderer/VK/PipelineStateVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/PipelineStateVK.h) | 0 |
| [`Renderer/VK/RenderInstancesVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/RenderInstancesVK.h) | 0 |
| [`Renderer/VK/RenderPrimitiveVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/RenderPrimitiveVK.h) | 0 |
| [`Renderer/VK/RendererVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/RendererVK.h) | 0 |
| [`Renderer/VK/Renderer_VK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/Renderer_VK.h) | 4 |
| [`Renderer/VK/ShaderVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/ShaderVK.h) | 0 |
| [`Renderer/VK/StorageBufferVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/StorageBufferVK.h) | 0 |
| [`Renderer/VK/SubViewportVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/SubViewportVK.h) | 0 |
| [`Renderer/VK/TextureVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/TextureVK.h) | 0 |

### Resources (21 headers)

File/resource and serialization headers.

| Header | Export-marked declarations |
|---|---:|
| [`Resources/BezierCurve.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/BezierCurve.h) | 0 |
| [`Resources/Config.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/Config.h) | 0 |
| [`Resources/Gradient.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/Gradient.h) | 0 |
| [`Resources/Json.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/Json.h) | 0 |
| [`Resources/ObjectStream/GetPrimitiveTypeOfType.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/GetPrimitiveTypeOfType.h) | 0 |
| [`Resources/ObjectStream/ObjectStream.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/ObjectStream.h) | 4 |
| [`Resources/ObjectStream/ObjectStreamBinaryIn.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/ObjectStreamBinaryIn.h) | 0 |
| [`Resources/ObjectStream/ObjectStreamBinaryOut.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/ObjectStreamBinaryOut.h) | 0 |
| [`Resources/ObjectStream/ObjectStreamIn.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/ObjectStreamIn.h) | 0 |
| [`Resources/ObjectStream/ObjectStreamOut.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/ObjectStreamOut.h) | 0 |
| [`Resources/ObjectStream/ObjectStreamTextIn.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/ObjectStreamTextIn.h) | 0 |
| [`Resources/ObjectStream/ObjectStreamTextOut.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/ObjectStreamTextOut.h) | 0 |
| [`Resources/ObjectStream/ObjectStreamTypes.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/ObjectStreamTypes.h) | 0 |
| [`Resources/ObjectStream/SerializableAttribute.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/SerializableAttribute.h) | 0 |
| [`Resources/ObjectStream/SerializableAttributeEnum.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/SerializableAttributeEnum.h) | 0 |
| [`Resources/ObjectStream/SerializableAttributeTyped.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/SerializableAttributeTyped.h) | 0 |
| [`Resources/ObjectStream/SerializableObject.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/SerializableObject.h) | 0 |
| [`Resources/ObjectStream/TypeDeclarations.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/TypeDeclarations.h) | 0 |
| [`Resources/PCK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/PCK.h) | 0 |
| [`Resources/Timer.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/Timer.h) | 0 |
| [`Resources/Tween.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/Tween.h) | 0 |

### Skeleton (1 headers)

Skeleton and animation declarations.

| Header | Export-marked declarations |
|---|---:|
| [`Skeleton/Skeleton.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Skeleton/Skeleton.h) | 0 |

### TriangleSplitter (1 headers)

Triangle splitting utilities.

| Header | Export-marked declarations |
|---|---:|
| [`TriangleSplitter/TriangleSplitter.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/TriangleSplitter/TriangleSplitter.h) | 0 |

### Variants (53 headers)

Math, geometry and container value types.

| Header | Export-marked declarations |
|---|---:|
| [`Variants/AABB2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/AABB2.h) | 0 |
| [`Variants/AABB3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/AABB3.h) | 0 |
| [`Variants/Basis.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Basis.h) | 0 |
| [`Variants/Color.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Color.h) | 0 |
| [`Variants/Math/EigenValueSymmetric.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Math/EigenValueSymmetric.h) | 0 |
| [`Variants/Math/GaussianElimination.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Math/GaussianElimination.h) | 0 |
| [`Variants/Math/HalfFloat.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Math/HalfFloat.h) | 0 |
| [`Variants/Math/MathTypes.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Math/MathTypes.h) | 0 |
| [`Variants/Math/Real.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Math/Real.h) | 0 |
| [`Variants/Math/Trigonometry.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Math/Trigonometry.h) | 0 |
| [`Variants/Matrix/DMat44.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/DMat44.h) | 0 |
| [`Variants/Matrix/DMat4x3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/DMat4x3.h) | 0 |
| [`Variants/Matrix/DynMatrix.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/DynMatrix.h) | 0 |
| [`Variants/Matrix/Mat22.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/Mat22.h) | 0 |
| [`Variants/Matrix/Mat2x3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/Mat2x3.h) | 0 |
| [`Variants/Matrix/Mat2x4.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/Mat2x4.h) | 0 |
| [`Variants/Matrix/Mat33.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/Mat33.h) | 0 |
| [`Variants/Matrix/Mat3x2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/Mat3x2.h) | 0 |
| [`Variants/Matrix/Mat3x4.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/Mat3x4.h) | 0 |
| [`Variants/Matrix/Mat44.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/Mat44.h) | 0 |
| [`Variants/Matrix/Mat4x2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/Mat4x2.h) | 0 |
| [`Variants/Matrix/Mat4x3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/Mat4x3.h) | 0 |
| [`Variants/Matrix/TMatrix.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/TMatrix.h) | 0 |
| [`Variants/OBB2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/OBB2.h) | 0 |
| [`Variants/OBB3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/OBB3.h) | 0 |
| [`Variants/Plane.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Plane.h) | 0 |
| [`Variants/Quat.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Quat.h) | 0 |
| [`Variants/Rect.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Rect.h) | 0 |
| [`Variants/TArray.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/TArray.h) | 0 |
| [`Variants/TMap.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/TMap.h) | 0 |
| [`Variants/TSet.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/TSet.h) | 0 |
| [`Variants/TStaticArray.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/TStaticArray.h) | 0 |
| [`Variants/Vector/BVec16.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/BVec16.h) | 0 |
| [`Variants/Vector/DVec2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/DVec2.h) | 0 |
| [`Variants/Vector/DVec3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/DVec3.h) | 0 |
| [`Variants/Vector/DVec4.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/DVec4.h) | 0 |
| [`Variants/Vector/Double2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Double2.h) | 0 |
| [`Variants/Vector/Double3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Double3.h) | 0 |
| [`Variants/Vector/Double4.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Double4.h) | 0 |
| [`Variants/Vector/Float2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Float2.h) | 0 |
| [`Variants/Vector/Float3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Float3.h) | 0 |
| [`Variants/Vector/Float4.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Float4.h) | 0 |
| [`Variants/Vector/Int2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Int2.h) | 0 |
| [`Variants/Vector/Int3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Int3.h) | 0 |
| [`Variants/Vector/Int4.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Int4.h) | 0 |
| [`Variants/Vector/TVec.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/TVec.h) | 0 |
| [`Variants/Vector/UVec4.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/UVec4.h) | 0 |
| [`Variants/Vector/Vec2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Vec2.h) | 1 |
| [`Variants/Vector/Vec3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Vec3.h) | 1 |
| [`Variants/Vector/Vec4.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Vec4.h) | 0 |
| [`Variants/Vector/iVec2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/iVec2.h) | 1 |
| [`Variants/Vector/iVec3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/iVec3.h) | 1 |
| [`Variants/Vector/iVec4.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/iVec4.h) | 1 |

### Top-level subsystem headers (11 headers)

| Header | Export-marked declarations |
|---|---:|
| [`Moss.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss.h)` | 0 |
| [`Moss_Audio.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss_Audio.h)` | 113 |
| [`Moss_GPU.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss_GPU.h)` | 241 |
| [`Moss_GUI.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss_GUI.h)` | 73 |
| [`Moss_Navigation.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss_Navigation.h)` | 0 |
| [`Moss_Network.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss_Network.h)` | 146 |
| [`Moss_Physics.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss_Physics.h)` | 39 |
| [`Moss_Platform.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss_Platform.h)` | 181 |
| [`Moss_Renderer.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss_Renderer.h)` | 195 |
| [`Moss_XR.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss_XR.h)` | 41 |
| [`Moss_stdinc.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss_stdinc.h)` | 34 |

## Inline implementation header inventory (19 files)

These files provide inline/template definitions or companion implementation details. They are included here because the public API surface cannot be understood from declarations alone when these files contain required definitions.

| Inline header |
|---|
| [`Core/FixedSizeFreeList.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/FixedSizeFreeList.inl) |
| [`Core/JobSystem.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/JobSystem.inl) |
| [`Core/LockFreeHashMap.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/LockFreeHashMap.inl) |
| [`Core/Profiler.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/Profiler.inl) |
| [`Physics/Body/Body.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Body/Body.inl) |
| [`Variants/Matrix/DMat44.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/DMat44.inl) |
| [`Variants/Matrix/Mat44.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/Mat44.inl) |
| [`Variants/Quat.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Quat.inl) |
| [`Variants/Vector/BVec16.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/BVec16.inl) |
| [`Variants/Vector/DVec2.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/DVec2.inl) |
| [`Variants/Vector/DVec3.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/DVec3.inl) |
| [`Variants/Vector/DVec4.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/DVec4.inl) |
| [`Variants/Vector/UVec4.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/UVec4.inl) |
| [`Variants/Vector/Vec2.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Vec2.inl) |
| [`Variants/Vector/Vec3.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Vec3.inl) |
| [`Variants/Vector/Vec4.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Vec4.inl) |
| [`Variants/Vector/iVec2.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/iVec2.inl) |
| [`Variants/Vector/iVec3.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/iVec3.inl) |
| [`Variants/Vector/iVec4.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/iVec4.inl) |

## 9. Native integration details to preserve

This is a language-level API specification, not a mechanical ABI mirror. When implementing the projection, keep these source-level distinctions intact:

- Convert pointer-plus-count inputs into a single slice/span/list/array/table/sequence; do not expose a redundant count parameter when the collection already carries its length.
- Convert output pointer pairs (for example, mouse coordinates) into a point/tuple/value object. Do not expose C-style `int*` output arguments.
- Resource factories return owned language objects and translate native allocation/backend failure into the language's normal error model. Destroy/terminate functions become deterministic resource disposal (`Dispose`, `Drop`, `close`, or context-manager release), not ordinary public `destroy_*` calls.
- GUI calls should be scoped to a `GuiContext` instance in every managed binding. They must not silently depend on mutable global “current context” state when the public API can retain an explicit context.
- Vulkan handles, callback pointers, allocator callbacks and backend-specific opaque handles stay behind advanced interop APIs unless the user explicitly opts into native interop. They must not be confused with Moss-owned managed resources.
- Where the native declaration lacks information required by the intended safe API (for example a pointer/count pair, output values, or a resource owner), resolve the native declaration or binding-side contract before implementing that projection; do not guess buffer length or object lifetime.

## 10. Build and distribution contract

- Ship matching native binaries and binding artifacts for each supported OS/architecture combination.
- Keep ABI declarations internal; public API users should not need to know calling conventions, struct packing, pointer widths or native library search paths.
- Make backend and optional dependency feature flags visible through capability queries or build metadata.
- Test a clean build, import/load, initialization/shutdown, object lifetime, callback detach, error translation and resource ownership in CI for every supported platform.
- Keep the generated language API index in sync with the native CMoss export catalogue whenever public native declarations change.

## 11. Compatibility and versioning

Treat the binding's public API as a separately versioned language library. A native API addition does not become a public binding API until the type mapping, ownership, failure semantics and example are specified. Avoid leaking raw `Moss_*` function names into application code. Deprecate public methods using the target language's normal annotation mechanism and provide a migration note in the API Reference itself when a breaking change is unavoidable.

---

**Canonical authority:** Moss C++/CMoss headers and the CMoss API Reference supplied for this documentation set. The native symbol column preserves traceability; the public API column is the intended language-specific design.
