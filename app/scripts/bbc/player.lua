-- SPDX-FileCopyrightText: (c) 2026 Simon Inns
-- SPDX-License-Identifier: GPL-3.0-or-later
--
-- MAME autoboot script for the BBC Micro disc check (bbc-check.sh): boots
-- the disc in drive 0 with Shift+Break, then runs the steps in the file
-- named by BBC_STEPS_FILE, one per line:
--
--   wait TEXT    until the MODE 7 screen shows TEXT
--   gone TEXT    until it no longer does
--   blank ROW    until screen row ROW (0-24) is empty
--   key KEYS     types KEYS (MAME's coded form: {ENTER} for RETURN)
--   press KEY    holds the key named KEY (ESCAPE, which the natural
--                keyboard does not type) for a tenth of a second
--   frames N     lets N frames (50 a second) pass
--   screen       prints the screen
--   memory       prints PAGE, TOP, HIMEM and the least free memory seen
--
-- It prints "STEP time step" as each step starts, "STATUS time text"
-- whenever the bottom row (the player's status line) changes, the screens
-- between "SCREEN" and "END", and "TIMEOUT" if a step waits ten emulated
-- minutes. Times are emulated seconds, as in the -wavwrite recording.

local machine = manager.machine
local memory = machine.devices[":maincpu"].spaces["program"]
local keyboard = machine.natkeyboard
local shift = machine.ioport.ports[":kbd:COL0"].fields["SHIFT"]

-- MOS: the address of the screen's top left corner, at &350.
local function row_text(row)
  local start = memory:read_u8(0x350) + 256 * memory:read_u8(0x351)
  local text = {}
  for column = 0, 39 do
    local address = start + row * 40 + column
    -- The MODE 7 screen wraps from &8000 back to &7C00.
    if address >= 0x8000 then
      address = address - 0x400
    end
    local c = memory:read_u8(address) & 0x7F
    text[#text + 1] = string.char(c < 32 and 32 or c)
  end
  return table.concat(text)
end

local function screen_text()
  local rows = {}
  for row = 0, 24 do
    rows[#rows + 1] = row_text(row)
  end
  return table.concat(rows, "\n")
end

local function now()
  return machine.time:as_double()
end

local function word(address)
  return memory:read_u8(address) + 256 * memory:read_u8(address + 1)
end

-- BBC BASIC II keeps the end of its variables at &02, its stack pointer
-- at &04 and HIMEM at &06; the stack grows down from HIMEM. Each write to
-- the stack pointer is checked, once the player has lowered HIMEM to make
-- its buffer, for the least room left between the variables and the
-- stack. A pointer is written a byte at a time, so a pop that carries into
-- the high byte can show for a moment as up to 255 bytes lower than it
-- is: the figure errs on the side of less room.
local least_free = -1
-- A global, so the tap is not collected while the script runs.
stack_tap = nil
stack_tap = memory:install_write_tap(0x04, 0x05, "stack", function(offset, data)
  if word(0x06) >= 0x7C00 then
    return
  end
  local low = offset == 0x04 and data or memory:read_u8(0x04)
  local high = offset == 0x05 and data or memory:read_u8(0x05)
  local free = low + 256 * high - word(0x02)
  if least_free < 0 or free < least_free then
    least_free = free
  end
end)

local steps = {}
for line in io.lines(os.getenv("BBC_STEPS_FILE")) do
  if line ~= "" then
    steps[#steps + 1] = line
  end
end

local function wait_until(condition, step)
  local deadline = now() + 600
  while not condition() do
    if now() > deadline then
      print("TIMEOUT " .. step)
      print("SCREEN\n" .. screen_text() .. "\nEND")
      return false
    end
    coroutine.yield()
  end
  return true
end

local run = coroutine.create(function()
  -- Shift held through the reset runs !BOOT (boot option 3).
  shift:set_value(1)
  for _ = 1, 100 do
    coroutine.yield()
  end
  shift:clear_value()
  for _, step in ipairs(steps) do
    local op, argument = step:match("^(%S+)%s?(.*)$")
    print(string.format("STEP %.3f %s", now(), step))
    local ok = true
    if op == "wait" then
      ok = wait_until(function() return screen_text():find(argument, 1, true) ~= nil end, step)
    elseif op == "gone" then
      ok = wait_until(function() return screen_text():find(argument, 1, true) == nil end, step)
    elseif op == "blank" then
      ok = wait_until(function() return row_text(tonumber(argument)):match("^ *$") ~= nil end, step)
    elseif op == "key" then
      keyboard:post_coded(argument)
      ok = wait_until(function() return not keyboard.is_posting end, step)
    elseif op == "press" then
      local field = nil
      for _, port in pairs(machine.ioport.ports) do
        field = field or port.fields[argument]
      end
      if field == nil then
        print("ERROR no key " .. argument)
        ok = false
      else
        field:set_value(1)
        for _ = 1, 5 do
          coroutine.yield()
        end
        field:clear_value()
      end
    elseif op == "frames" then
      for _ = 1, tonumber(argument) do
        coroutine.yield()
      end
    elseif op == "screen" then
      print("SCREEN\n" .. screen_text() .. "\nEND")
    elseif op == "memory" then
      print(string.format("MEMORY PAGE=%d TOP=%d HIMEM=%d LEAST_FREE=%d",
        memory:read_u8(0x18) * 256, word(0x12), word(0x06), least_free))
    else
      print("ERROR unknown step " .. step)
      ok = false
    end
    if not ok then
      break
    end
  end
  machine:exit()
end)

local status = nil
emu.register_frame_done(function()
  local text = row_text(24)
  if text ~= status then
    status = text
    print(string.format("STATUS %.3f %s", now(), text))
  end
  if coroutine.status(run) ~= "dead" then
    local ok, problem = coroutine.resume(run)
    if not ok then
      print("ERROR " .. tostring(problem))
      machine:exit()
    end
  end
end)
