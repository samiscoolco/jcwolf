# jcwolf
**jcwolf** is a reimplementation of *Wolfenstein 3D* using the original game files.

---

## Big Idea & Project Goals

- Easy to port to other languages  
- Provide the world with easy examples of reversing [RLEW](https://moddingwiki.shikadi.net/wiki/Id_Software_RLEW_compression) and [Carmack Compression](https://moddingwiki.shikadi.net/wiki/Carmack_compression)  
- Contained in a single file for clarity and porting ease  
- Easy to hack or add new features (or build new games)  
- Plug and play with Wolf3D's original game data files  

---

## Getting Started

### Game Files

Included in this repo are the original `.WL1` files from the shareware release of Wolf3D. JCWolf is configured by default to use these.

If you have the full game, place all original `.WL6` game files in the same directory where you compile your executable. Then remove the following line in `main.c`:

```c
#define SHAREWARE
```

### Build With GCC and Run

```bash
gcc main.c -o jcwolf.exe -Iinclude -Llib -lraylib -lgdi32 -lwinmm
./jcwolf.exe levelnum
```

_Replace `levelnum` with a desired level number, or leave blank for level 0._

**Now get psyched.**

<img width="626" height="464" alt="image" src="https://github.com/user-attachments/assets/6cd0b9aa-f137-4e54-b80b-c702abe849db" />

---

## Planned Language Ports

- [x] C (base)  
- [ ] Python  
- [ ] Java  
- [ ] JavaScript  
- [ ] Go  

---

## References and Thanks

- [Shikadi Docs](https://moddingwiki.shikadi.net/wiki/GameMaps_Format)  
- [XWOLF Docs](https://devinsmith.net/backups/xwolf/docs.html)  
- [Wolf3D Original Source Code](https://github.com/id-Software/wolf3d)  
