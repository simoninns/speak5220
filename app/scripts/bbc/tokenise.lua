-- SPDX-FileCopyrightText: (c) 2026 Simon Inns
-- SPDX-License-Identifier: GPL-3.0-or-later
--
-- MAME autoboot script: types each BASIC program on the disc in drive 0
-- into BBC BASIC II with *EXEC, as if typed at the keyboard, and prints
-- the program BASIC stored ("PROGRAM NAME HEX", PAGE to TOP: the bytes
-- SAVE writes). The programs are named, separated by spaces, in
-- BBC_PROGRAMS. Used by bbc-check.sh and app/scripts/golden/basic_programs.py.

local machine = manager.machine
local memory = machine.devices[":maincpu"].spaces["program"]
local keyboard = machine.natkeyboard

local programs = {}
for name in string.gmatch(os.getenv("BBC_PROGRAMS"), "%S+") do
  programs[#programs + 1] = name
end

local function word(address)
  return memory:read_u8(address) + 256 * memory:read_u8(address + 1)
end

-- Busy while keys are being typed, or the MOS's *EXEC file (its handle at
-- &256) is open.
local function busy()
  return keyboard.is_posting or memory:read_u8(0x256) ~= 0
end

local run = coroutine.create(function()
  for _ = 1, 100 do
    coroutine.yield()
  end
  for index, name in ipairs(programs) do
    keyboard:post((index > 1 and "NEW\r" or "") .. "*EXEC " .. name .. "\r")
    for _ = 1, 20 do
      coroutine.yield()
    end
    while busy() do
      coroutine.yield()
    end
    -- Let BASIC store the last line.
    for _ = 1, 50 do
      coroutine.yield()
    end
    -- BBC BASIC II: PAGE's high byte at &18, TOP at &12.
    local bytes = {}
    for address = memory:read_u8(0x18) * 256, word(0x12) - 1 do
      bytes[#bytes + 1] = string.format("%02X", memory:read_u8(address))
    end
    print("PROGRAM " .. name .. " " .. table.concat(bytes))
  end
  machine:exit()
end)

emu.register_frame_done(function()
  if coroutine.status(run) ~= "dead" then
    local ok, problem = coroutine.resume(run)
    if not ok then
      print("ERROR " .. tostring(problem))
      machine:exit()
    end
  end
end)
