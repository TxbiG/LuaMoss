# LuaMoss Cheatsheet

> Quick reference for the idiomatic Lua 5.4 public API. The companion [API Reference](API_Reference.md) defines the complete feature map and all exported native-to-language projections. The public calls below are the target design, not a claim that the current binding has implemented every member yet.

## Language design at a glance

The public surface is a Lua 5.4 module. Use lowercase snake_case module functions, tables for option records, userdata for owned native resources, colon syntax for object methods, and idiomatic `nil, error` failure returns.

## Subsystem map

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

## 1. Import Moss

```lua
local moss = require("moss")
local platform = moss.platform
local audio = moss.audio
```

## 2. Create a window and pump events

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

## 3. Create and upload a GPU buffer

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

## 4. Render a frame

```lua
local frame = assert(renderer:begin_frame())
frame:set_camera(camera)
frame:draw_sprite(sprite)
frame:draw_text("Hello Moss", position, text_style)
frame:finish()
```

## 5. Play audio

```lua
assert(moss.audio.initialize())
local source = assert(moss.audio.load_wav("assets/theme.wav"))
source:play()
moss.audio.update(delta_time)
source:close()
moss.audio.shutdown()
```

## 6. Build a GUI

```lua
local ui = moss.gui.create_context()
ui:window("Inspector", function(panel)
    panel:text("Player")
    local enabled = panel:checkbox("Enabled", current_value)
end)
ui:close()
```

## 7. Simulate physics

```lua
local physics = assert(moss.physics.create_system(settings))
local body = assert(physics.bodies:create {
    shape = moss.physics.shape.box(size),
    motion_type = "dynamic",
})
physics:step(delta_time)
local contacts = physics.query:contacts(body)
```

## 8. Networking

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

## 9. XR session

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

## 10. Common type families

| Category | Public types |
|---|---|
| Vectors | `Vec2`, `Vec3`, `Vec4`, float/double/integer variants |
| Matrices and transforms | `Mat22`, `Mat33`, `Mat44`, `DMat44`, `Quat`, `Basis` |
| Geometry | `AABB2`, `AABB3`, `OBB2`, `OBB3`, `Plane`, `Rect`, `Triangle` |
| Colour | `Color` with explicit component range and storage precision |
| Containers | language-native lists/maps/sets or typed Moss containers when allocator semantics matter |
| Results and references | target-language result/error types and explicit owned/borrowed resource wrappers |
| Jobs and synchronization | job system, thread pool, barrier, mutex, semaphore and scoped locks |

## 11. Resource-management rules

Owned resources are full userdata with `__gc` as a safety net and an explicit `:close()` method for deterministic release. `__close` should support Lua 5.4 to enable to-be-closed variables. Borrowed views must not have independent destructors. Use tables for descriptor arguments and retain registry references for callbacks until unregister.

## 12. Error-handling rules

Functions that can fail return `value` on success or `nil, message` on failure. Boolean queries return one boolean value. Do not raise errors for routine missing assets or unavailable devices; reserve `error()` for programmer errors and offer `assert` as a caller choice. Callback errors must be caught at the C boundary and converted to logged Lua errors.

## 13. Feature and platform checks

- A declaration in the reference does not mean every backend implements it. Check the selected GPU backend, platform, XR runtime and optional networking/codec features.
- Keep GPU resources associated with the device that created them; release child resources before the device.
- Pair renderer frame begin/end, GUI begin/end, render-pass begin/end and XR frame begin/end operations. Prefer scoped/structured helpers so early returns cannot leave a subsystem unbalanced.
- Poll platform and network events at the cadence required by the subsystem. Do not block a browser/UI thread waiting for asynchronous I/O.
- Use the API Reference's subsystem and header inventory when looking for advanced Navigation, Geometry, Physics, Resources, Skeleton or Variants APIs.

## 14. Where to look next

See [API_Reference.md](API_Reference.md) for the full native symbol crosswalk, target-language signatures, resource models, threading rules, feature coverage and the complete 270-header / 19-inline-header inventory.
