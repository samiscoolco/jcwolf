# jcwolf

**jcwolf** is a reimplementation of *Wolfenstein 3D* using the original `.WL6` game files.





Big idea project goals:
- Easy to port to other languages.
- Provide the world with easy exampes of reversing [RLEW](https://moddingwiki.shikadi.net/wiki/Id_Software_RLEW_compression) and [Carmack Compression](https://moddingwiki.shikadi.net/wiki/Carmack_compression).
- Contained in a single file for clarity and porting ease.
- Easy to hack or add new features to (or build new games with.)
- Plug and Play with Wolf3d's original game data files.

---

## Getting Started

**Build with gcc:**

`gcc main.c -o jcwolf.exe -Iinclude -Llib -lraylib -lgdi32 -lwinmm`


**Then:**
- Place all original `.WL6` game files next to your compiled executable.
- Run it and you’re good to go!

![screenshot](https://github.com/user-attachments/assets/d10312bf-18e3-44e9-ac58-9ce9e09bf5a1)

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
- [Wolf3d Original Source Code](https://github.com/id-Software/wolf3d)

---

