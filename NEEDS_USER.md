# Items Requiring User Interaction (NEEDS_USER.md)

This document tracks items that require physical hardware verification, interactive user credentials, or elevated root permissions that cannot/should not be executed autonomously.

---

## Current Items

| ID | Category | Item | Steps for User Verification | Status |
|---|---|---|---|---|
| REQ-001 | Audio/Hardware | In-game microphone input | 1. Launch Roblox via MqBlox.<br>2. Join an experience with Voice Chat enabled.<br>3. Speak into microphone and check indicator.<br>*(Note: launcher intentionally does not manipulate host audio mixers)* | PENDING_USER_TEST |
| REQ-002 | Auth | Interactive Roblox 2FA / Web login | 1. Enter experience link or ticket via launcher.<br>*(Launcher never reads or stores .ROBLOSECURITY cookies directly)* | DOCUMENTED |
| REQ-003 | System/Sudo | Installation of missing system dependencies (if any) | Run command when prompted: `sudo pacman -S --needed <packages>` | NONE_PENDING |
