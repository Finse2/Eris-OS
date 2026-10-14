# ErisOS Troubleshooting

Back to [README](README.md)

## Setup Does Not Appear

First launch setup appears only when `ACCOUNT.TXT` or `ERISCFG.TXT` is missing.

Inside ErisOS:

```text
DELETE ACCOUNT.TXT
```

or:

```text
DELETE ERISCFG.TXT
```

Then reboot.

From the host, delete the QEMU disk image for a fully fresh test:

```powershell
Remove-Item .\bin\hdd.img
.\compile.bat
```

## Mouse Missing In Setup Or Login

The setup and login screens expect the PS/2 mouse path used by QEMU. Rebuild and run through `compile.bat` so QEMU starts with the expected devices.

If the pointer is still missing, keyboard setup remains available:

- Type username/password and press `Enter`
- Press `T` to toggle theme
- Press `R` to cycle display mode
- Press `-` or `+` for timezone
- Press `Enter` to finish

## Login Always Fails

Check `ACCOUNT.TXT`:

```text
READ ACCOUNT.TXT
```

Expected format:

```text
USER=admin
PASS=eris
```

The username is case-insensitive. The password is case-sensitive.

## Layout Is Off Center After Resolution Changes

Use the Settings app or shell command to select a known mode:

```text
VGAMODE AUTO
VGAMODE 1280
VGAMODE 1024
VGAMODE 800
VGAMODE 640
```

Setup, login, loading, and desktop windows are centered from the active VESA width/height. If a mode fails in QEMU, `AUTO` falls back through the safe-mode list.

## Desktop Time Looks Wrong

The taskbar reads the PC CMOS/RTC clock and applies the configured timezone offset. Change timezone in Settings, or recreate setup by deleting `ERISCFG.TXT`.

## Web Access Fails

Networking is experimental. The current stack supports plain HTTP, not HTTPS/TLS.

Recommended QEMU checks:

```text
NETINFO
DHCP
PING 10.0.2.2
DNS neverssl.com
HTTPGET neverssl.com /
```

If DNS fails, try an IP-based HTTP test with `TCPGET` or postpone web testing until the NIC/DNS path is being worked on again.

## Real Hardware Writes Do Not Persist

The GRUB ISO path builds in `ERISOS_REAL_HW_SAFE` mode. That mode blocks ATA writes to protect real disks, so setup/settings persistence is mainly for the normal QEMU `bin\hdd.img` workflow unless you intentionally provide a safe writable disk target.
