# PMW3610 driver source

Vendored from https://github.com/na-ka-no/zmk-pmw3610-driver at
`5e04553ab803d24405bd45621a41310ea3050e59`.
Original copyright and SPDX notices are retained in each source file.
The original sensor binding is in `../../dts/bindings/sensor/pixart,pmw3610.yml`.

Local changes:

- Add the BobTail include directory in CMake.
- Read the scroll threshold from `bobtail_scroll_tick_get()` in scroll mode.
  Sensor initialization, polling, pointer movement and scroll direction are unchanged.

The runtime speed behavior, bounds and debounced settings storage live in
`../../src/behavior_scroll_speed.c`. Thresholds are 64, 32, 16, 8 and 4 counts;
the middle level matches the previous fixed value of 16.
