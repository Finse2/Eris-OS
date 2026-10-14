# ErisOS Code Examples

Back to [README](README.md)

## 1) Filesystem Basics

```text
CREATE NOTES.TXT
WRITE NOTES.TXT hello from erisos
READ NOTES.TXT
SIZE NOTES.TXT
LIST
```

## 2) Executable Flow

```text
INSTALL HELLO
EXEC HELLO.BIN
```

Or with assembler:

```text
EDIT HI.ASM
ASM HI.ASM HI.BIN
CHMODX HI.BIN
EXEC HI.BIN
```

## 3) Minimal ASM Program

```asm
push ebp
mov ebp, esp
mov eax, [ebp+8]
mov ebx, [eax]
push msg
call ebx
add esp, 4
xor eax, eax
pop ebp
ret
msg: db "Hi from ASM", 10, 0
```

## 4) Display/Graphics

```text
COLOR MATRIX
VGAMODE TEST
VGAMODE TEXT
DESKTOP
```

In desktop mode, press `ESC` to close overlays or return to shell.

Useful display presets:

```text
VGAMODE AUTO
VGAMODE 1280
VGAMODE 1024
VGAMODE 800
VGAMODE 640
```

The Settings app can persist the preferred display mode in `ERISCFG.TXT`.

## 5) Retest First Launch Setup

Delete one of the setup-created system files, then reboot:

```text
DELETE ACCOUNT.TXT
```

or:

```text
DELETE ERISCFG.TXT
```

On the host, a fully fresh QEMU disk can be created by deleting the ATA image before the next run:

```powershell
Remove-Item .\bin\hdd.img
.\compile.bat
```

## 6) Example Settings Files

Account:

```text
USER=admin
PASS=eris
```

Desktop settings:

```text
THEME=1
FPS=7
MOUSE=1
RES=0
TZ=1
```

## 7) Networking Quick Check

```text
NETINFO
DHCP
PING 10.0.2.2
DNS example.com
HTTPGET example.com /
```

## 8) UDP Example

Terminal A:

```text
UDPBIND 5000
UDPRECV 5000
```

Terminal B / second run:

```text
UDPBIND 5001
UDPSEND 5001 10.0.2.15 5000 hello
```

## 9) TCP Example

```text
TCPCONN 93.184.216.34 80
TCPSEND GET / HTTP/1.0
TCPRECV
TCPCLOSE
```

Or one-shot:

```text
TCPGET 93.184.216.34 80 /
```
