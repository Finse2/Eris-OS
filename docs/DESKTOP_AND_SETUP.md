# ErisOS Desktop And First Setup

Back to [README](README.md)

## First Launch Setup

The graphical setup flow appears when either of these files is missing from the ATA filesystem:

- `ACCOUNT.TXT`
- `ERISCFG.TXT`

Setup collects:

- username
- password
- light or dark theme
- display preference
- timezone offset from UTC

After setup finishes, ErisOS writes `ACCOUNT.TXT` and `ERISCFG.TXT`, then returns to the normal graphical login screen.

## Setup Controls

Text steps:

- Type username, then press `Enter`
- Type password, then press `Enter`
- Use `Backspace` to edit

Options step:

- Click buttons with the mouse, or use keyboard shortcuts:
- `T`: toggle light/dark theme
- `R`: cycle display mode
- `-`: timezone one hour earlier
- `+`: timezone one hour later
- `Enter`: finish setup

## Retesting First Setup

From the shell or desktop terminal:

```text
DELETE ACCOUNT.TXT
```

or:

```text
DELETE ERISCFG.TXT
```

Then reboot QEMU. The setup flow should appear again.

For a completely fresh test disk, delete `bin\hdd.img` on the host before running:

```powershell
Remove-Item .\bin\hdd.img
.\compile.bat
```

## Account File

`ACCOUNT.TXT` is a small text file:

```text
USER=admin
PASS=eris
```

The username match is case-insensitive. The password match is case-sensitive.

## Settings File

`ERISCFG.TXT` stores persistent desktop settings:

```text
THEME=0
FPS=7
MOUSE=1
RES=0
TZ=0
```

Fields:

- `THEME`: `0` light/classic, `1` dark
- `FPS`: Eris login/loading animation speed, clamped to `3..15`
- `MOUSE`: mouse sensitivity, clamped to `1..3`
- `RES`: display preference
- `TZ`: UTC offset in hours, clamped to `-12..14`

Display preference values:

- `0`: Auto
- `1`: 1280x720
- `2`: 1024x768
- `3`: 800x600
- `4`: 640x480

## Login Flow

After setup, boot enters the graphical login screen.

Successful login:

1. validates against `ACCOUNT.TXT`
2. shows the Eris loading animation
3. launches the desktop

Failed login:

1. shows the Eris loading animation
2. displays an error dialog
3. returns to the login fields after pressing OK or Enter

## Desktop

The desktop includes:

- app icons
- first two filesystem files as desktop file icons
- taskbar with user name and RTC date/time
- Start menu
- Notepad
- File Manager
- Internet app
- Settings app
- Terminal bridge
- shutdown/restart dialog

The desktop date/time is read from the PC CMOS/RTC clock and adjusted by `TZ`.

## Settings App

Settings can change:

- theme
- login animation FPS
- mouse sensitivity
- display preference
- timezone

Changes are saved immediately to `ERISCFG.TXT`.

## Display Behavior

The VESA path chooses the best safe 32-bit mode:

1. 1280x720
2. 1024x768
3. 960x720
4. 800x600
5. 640x480

The selected mode can be changed with:

```text
VGAMODE <AUTO|VESA|1280|1024|800|640|TEST|TEXT>
```

The setup, login, loading, and main desktop windows are centered based on the current resolution.
