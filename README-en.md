A set of tools for working with flash modems on a Qualcom chipset. The set consists of a package of utilities and a set of patched loaders.

qcommand is an interactive terminal for entering commands via the command port. It is replacing the terribly awkward revskills. Allows you to enter byte-by-command packets, edit memory, read and view any sector of flash.

qrmem is a program for reading a dump of the modem's address space.

qrflash - program for reading flash. Able to read as a range of blocks, and sections on the map sections.

qwflash is a program for recording partition images using the bootloader's user partitions mode, similar to QPST.

qwdirect is a program for direct recording of USB flash drive blocks with / without OOV through the controller ports (without involvement of the bootloader logic).

qdload is a program for loading loaders. Requires the modem to be in download mode or PBL emergency boot mode.

dload.sh - a script to put the modem in download mode and send the specified bootloader to it.

The programs require modified versions of the loaders. They are collected in the loaders directory, and the patch source is in cmd_05_write_patched.asm.
