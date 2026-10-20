# ColourPass — Design Iteration Log

## Iteration 01: Reset Interaction

**Status:** Proposed — Not Yet Tested

### Initial Design
The physical switch was originally intended to reset the lighting session, while the touch sensor would switch between lighting effects.

### Design Question
Could the touch sensor support both effect switching and session reset without adding another control?

### Revised Design
- Physical switch: Power on/off (subject to hardware testing).
- Single tap: Switch to the next lighting effect.
- Long press (3 seconds): Exit the current mode and return to card scanning.
- Reset feedback: Briefly flash white to confirm the reset.

A double-tap reset was considered, but a long press was selected as the current design to explore whether it reduces unintended resets.

