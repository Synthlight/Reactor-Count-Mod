# SFSE Plugin For Various Patches

This was originally made for reactors counts, but has expanded to build multiple plugins to affect similar patches.

- [No Build Below Bay Error](https://www.nexusmods.com/starfield/mods/7078)
- [No Cockpit Limit](https://www.nexusmods.com/starfield/mods/7079)
- [No Engine Power Limits](https://www.nexusmods.com/starfield/mods/1511)
- [No Grav Drive Weight Limit](https://www.nexusmods.com/starfield/mods/2599)
- [No Landing Bay or Docker Limits](https://www.nexusmods.com/starfield/mods/6150)
- [No Minimum Landing Gear](https://www.nexusmods.com/starfield/mods/1479)
- [No Reactor Class Requirement](https://www.nexusmods.com/starfield/mods/1482)
- [No Reactor Limit](https://www.nexusmods.com/starfield/mods/1169)
- [No Shield Limit](https://www.nexusmods.com/starfield/mods/1475)
- [No Unattached Module Error](https://www.nexusmods.com/starfield/mods/7095)
- [No Weapon Power or Count Limits](https://www.nexusmods.com/starfield/mods/1510)

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