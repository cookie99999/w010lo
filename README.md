# A homebrew MC68010 computer

### Current specs:

- 12MHz 68010
- 1MB RAM
- 256K flash
- 68681 DUART
- A simple monitor ported from my 65816 computer, with XMODEM download support

### Todo:

- ATA interface
- PCB version
- Add the decoupling caps I left out due to laziness

### Toolchain notes

For assembly I am using vasm, since I like the syntax better than GAS. For C, I built a cross-compiling GCC and binutils from source. When configuring GCC for a 68000/68008/68010 target, you have to be careful not to let it use any 68020+ or FPU specific instructions, or else your libgcc won't work. Here's the line I used to configure mine:
```
../gcc-15.2.0/configure --prefix=/home/cookie/.local/ --program-prefix=m68k-elf- --target=m68k-elf --with-cpu=m68000 --enable-languages=c --with-newlib --disable-libssp --disable-libgomp --disable-threads --disable-multilib --disable-nls --disable-libquadmath --without-headers --disable-m68020 --disable-m68881
```
Some of the less obvious options may not be strictly necessary, I haven't tested with and without each one.
With just a compiler, binutils, and libgcc, you have pretty much all you need to comfortably write and use C programs. For a standard C library, baselibc looks the easiest and most lightweight, though I haven't ported it yet.

###

Shoutout to [aslak](https://www.aslak.net/), [rosco-m68k](https://github.com/rosco-m68k), [68-Katy](https://www.bigmessowires.com/68-katy/), and [Jeff Tranter](https://jefftranter.blogspot.com/) for their inspiring projects and helpful logs.