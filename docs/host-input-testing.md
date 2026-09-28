# Host input test controls

These optional environment variables support repeatable keyboard navigation when a
test driver can only send short key taps. They are disabled by default.

- `KYTY_HOST_INPUT_MIN_PRESS_MS=0..2000`: keep a mapped key pressed for at least
  this many milliseconds. Longer physical holds remain held until release. A
  repeat press extends the deadline. Focus loss and shutdown release pending keys.
- `KYTY_HOST_INPUT_ONLY=1`: ignore other gamepad connections inside this emulator
  process, preventing their axis reports from replacing keyboard input. This
  does not change devices or configuration in the operating system.
- `KYTY_HOST_INPUT_TRACE=<path>`: append CSV input diagnostics with columns
  `steady_clock_ms,event,a,b,c`. Events are `host-key` and `apply-key`
  (key code, down, unused), `axis` (controller id, axis index, value), and
  `focus-lost`. The clock origin is unspecified.

The minimum-press mode deliberately changes input timing. Do not use it for
input-latency measurements. Flushed input tracing also adds overhead.

Build `host_input_pulse_tests` and run `ctest -R '^host_input_pulse$'` for the
deterministic timing tests. They cover default pass-through, retained short
presses, exact release deadlines, long holds, represses, independent keys,
focus/shutdown release, and unmatched key releases.
