# SFSE Plugin For Various Patches

This was originally made for reactors counts, but has expanded to build multiple plugins to affect similar patches.

- [No Reactor Limit](https://www.nexusmods.com/starfield/mods/1169)
- [No Shield Limit](https://www.nexusmods.com/starfield/mods/1475)
- [No Minimum Landing Gear](https://www.nexusmods.com/starfield/mods/1479)
- [No Reactor Class Requirement](https://www.nexusmods.com/starfield/mods/1482)

# Building

Made with VS 2022 (v143).

- Clone this repo.
- Init submodules. (`git submodule update --init`)
- Clone [Base-Dll-Proxy](https://github.com/Synthlight/Base-Dll-Proxy) into the **same level** as this repo.

i.e. you should have a directory with:
```
\Parent Dir
...\This Repo
...\Base-Dll-Proxy
```

Open in VS and have fun.

Batch build whichever configs you want.