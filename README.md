# Cosmo3DS

This is a stripped down version of [ReiNAND](https://github.com/Reisyukaku/ReiNand) 
that does nothing but load FIRM and emuNAND.

It is intended to be used with [3ds_injector](https://github.com/yifanlu/3ds_injector) 
to launch a modified `loader` in FIRM.

## Build

Follow the directions for building ReiNAND

## Injecting FIRM

1. You need a decrypted firmware.bin from a compatible version. One that works 
has SHA1 `2c9b63126e3ed3d402d3e6e01b966675a46a3dae`
2. Find the `loader` NCCH inside firmware.bin. With the image listed above, 
the offset is `0x26600`
3. Make sure your replacement NCCH is the same size. The image listed above 
has a `loader` of size `0x3000`
4. Replace the NCCH with your modified version.
5. Move firmware.bin to the root of your SD card as firmware_cosmo.bin 
