local moss = require("moss")
local window = moss.Window("LuaMoss", 1280, 720)
local renderer = window:create_renderer()
local centre = moss.Vec2(640, 360)
while not window:should_close() do
  moss.poll_events()
  renderer:begin_frame()
  renderer:circle2d(centre, 80, moss.Color(0.95, 0.2, 0.25))
  renderer:line2d(moss.Vec2(40, 40), centre, moss.Color(1, 1, 1), 2)
  renderer:end_frame()
end