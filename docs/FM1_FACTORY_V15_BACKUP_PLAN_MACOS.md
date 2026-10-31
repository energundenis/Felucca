# FM-1 factory V1.5: safe full-flash backup plan (macOS)

Status: research and preparation only. No device has been queried and no firmware or flash has been written.

## Goal

Create and verify a complete, device-specific backup of the user's own M-VAVE FM-1 while it is still running factory firmware V1.5 / 015. Preserve the stock bootloader, factory-installed app build, settings, Bluetooth data and user resources.

A public FM-1 dump exists, but it belongs to another unit and is not a substitute for this device's own backup. The published factory 015 package also differs from at least one measured factory-installed 015 app build.

## Supported read-only route found

M-Flasher by jvitkauskas, release 0.7.0:
- macOS 11 or newer; Apple silicon and Intel are listed as supported.
- Release page: https://github.com/jvitkauskas/m-flasher/releases
- Usage guide: https://github.com/jvitkauskas/m-flasher/blob/main/docs/usage.md
- Safety notes: https://github.com/jvitkauskas/m-flasher/blob/main/docs/safety.md

The M-Flasher usage guide explicitly says its DOWNLOAD MODE action works with M-VAVE FM-1 015. On a working stock device, connect it over USB MIDI and use M-Flasher's DOWNLOAD MODE action; the stock bootloader does not provide a key combination for entering this mode. A USB_KEY / Raspberry Pi Pico dongle is described as a fallback when the firmware does not start; it is not the first route for a normally booting V1.5 unit.

## Operator procedure — backup only

1. Confirm the FM-1 still boots normally into factory V1.5 / 015. Do not update it.
2. Download the official M-Flasher 0.7.0 macOS DMG from the release page. Install by dragging the app to Applications.
3. Connect the FM-1 directly to the Mac with a known-good USB data cable. Avoid hubs.
4. Start M-Flasher promptly after connection. The guide warns that macOS may leave the device in download mode after about half a minute if its disk driver retains it.
5. Allow the macOS administrator-password prompt and removable-volume access prompt if they appear. These are required by the documented macOS workflow.
6. In M-Flasher, identify the device as FM-1 and confirm the displayed model/firmware information. If the model or firmware is unexpected or uncertain, stop.
7. Use **Back up the whole flash** only. This action is documented as reading/saving the 1 MiB flash and a JSON description; it writes nothing to the device.
8. Do not choose Restore, Install firmware, Install m-bootloader, Put back stock bootloader, or any other write action.
9. Save a second independent full backup. Compare the two BIN files byte-for-byte and compare their SHA-256 hashes. Keep both original files and the accompanying JSON metadata.
10. Copy the verified files to a second storage location. Do not publish the dump: it may contain device-specific data.

M-Flasher's documented backup location is Documents/M-Flasher/backups in the user's home folder. The application reads the flash and checks the read against a CRC32 computed by the chip. The separate public FM-1 backup project also reports two identical reads and matching chip CRC32 for its own device.

## Stop conditions

Stop without proceeding if:
- M-Flasher does not recognize the unit as FM-1 / factory 015;
- the app offers only a firmware/bootloader write path rather than a backup;
- any prompt suggests erase, restore, update, install, or write;
- the two independent backup files differ;
- a read/CRC/model check reports an error.

Do not attempt to force ROM download mode, wire a USB_KEY dongle, restore another unit's dump, or run raw flash commands as part of this first backup session.

## Recovery gate

A verified pair of matching full-flash dumps is necessary but not by itself sufficient to authorize firmware changes. Before any future write:
- establish a tested recovery route for this exact FM-1;
- confirm the backup can be recognized by the restore tool;
- inspect and preserve its hash and metadata;
- review the proposed write's exact flash regions and before/after diff;
- obtain the user's explicit approval.

No flashing or firmware modification is authorized by this document.

## References

- Public full dump of another FM-1, factory 015: https://github.com/jvitkauskas/fm1-backup
- M-Flasher usage: https://github.com/jvitkauskas/m-flasher/blob/main/docs/usage.md
- M-Flasher safety: https://github.com/jvitkauskas/m-flasher/blob/main/docs/safety.md
- M-Flasher releases: https://github.com/jvitkauskas/m-flasher/releases
