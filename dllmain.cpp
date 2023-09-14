#include "pch.h"

#include "AoBSwap.h"
#include "Logger.h"
#include "ScanMemory.h"
#include "Util.h"

#include "lib/SFSE/sfse/PluginAPI.h"
#include "lib/SFSE/sfse_common/sfse_version.h"

std::ofstream out;

const std::map<std::string, std::vector<std::string>> TYPE_MAPPING = {
    //{"GravDrive-Count-Mod", {"SB_LIMITBODY_MAX_GRAV_DRIVE"}}, // Winds up with "you need additional grav thrust".
    {"LandingGear-Count-Mod", {"SB_LIMITBODY_MIN_LANDING_GEAR"}},
    {"Reactor-Count-Mod", {"SB_LIMITBODY_MAX_REACTOR"}},
    {"Reactor-Class-Mod", {"SB_ERRORBODY_REACTOR_CLASS"}},
    {"Shield-Count-Mod", {"SB_LIMITBODY_MAX_SHIELD"}},
    {"Engine-Power-Mod", {"SB_LIMITBODY_EXCESS_POWER_ENGINE"}},
    {"Weapon-Power-Mod", {"SB_LIMITBODY_EXCESS_POWER_WEAPON", "SB_LIMITBODY_MAX_WEAPONS"}},
};

const std::map<std::string, std::vector<std::string>> SCAN_MAPPING = {
    //{"GravDrive-Count-Mod", {"7E ?? 48 8D 15 ?? ?? ?? ?? 48 8D 4D 50"}}, // 7E == `jle`
    {"LandingGear-Count-Mod", {"75 ?? 48 8D 15 ?? ?? ?? ?? 48 8D 4D 50"}}, // 75 == `jne`
    {"Reactor-Count-Mod", {"7E ?? 48 8D 15 ?? ?? ?? ?? 48 8D 4D 50"}}, // 7E == `jle`
    {"Reactor-Class-Mod", {"75 ?? 48 8D 15 ?? ?? ?? ?? 48 8D 4D 50"}}, // 75 == `jne`
    {"Shield-Count-Mod", {"7E ?? 48 8D 15 ?? ?? ?? ?? 48 8D 4D 50"}}, // 7E == `jle`
    {"Engine-Power-Mod", {"7E ?? 48 8D 15 ?? ?? ?? ?? 48 8D 4D 30"}}, // 7E == `jle`
    {"Weapon-Power-Mod", {"EB ?? 48 8D 15 ?? ?? ?? ?? 48 8D 4D 30", "7E ?? 48 8D 15 ?? ?? ?? ?? 48 8D 4D 50"}}, // EB == `jmp`. This one's unique as the 74 (`je`) 4 ops before (-11 bytes) this to EB (`jmp`).
};

void DoInjection() {
    LOG(TARGET_NAME << " loading.");

    const auto moduleName = GetExeFilename();
    LOG("Found module name: " << moduleName);

    const auto newBytes     = StringToByteVector("EB"); // EB == `jmp`
    auto       patchedCount = 0;

    for (const auto& pattern : SCAN_MAPPING.at(TARGET_NAME)) {
        LOG("Doing AoB scan.");

        auto addressesFound = ScanMemory(moduleName, pattern);
        if (addressesFound.empty()) {
            LOG("AoB scan returned no results, aborting.");
            return;
        }

        LOG("Found " << addressesFound.size() << " match(es).");

        auto validTypes = TYPE_MAPPING.at(TARGET_NAME);

        for (const auto& address : addressesFound) {
            const auto   addrBase  = reinterpret_cast<const UINT64>(address);
            const auto   leaOffset = *reinterpret_cast<const UINT32*>(address + 5); // In short, move the ptr 5 bytes, and dereference the 4 bytes (the `lea` offset) as an int. // NOLINT(clang-diagnostic-cast-qual)
            const UINT64 strBegin  = addrBase + leaOffset + 10; // +9 to offset to the end of the `lea` op, +1 to skip the char count. They're null-terminated anyways.
            const auto   typeStr   = std::string(reinterpret_cast<const char*>(strBegin)); // NOLINT(performance-no-int-to-ptr)

            if (Contains(validTypes, typeStr)) {
                LOG("Target address: " << std::uppercase << std::hex << addrBase);
                //PrintNBytes(address, 13);
                //LOG("LEA offset: " << std::uppercase << std::hex << leaOffset);
                //LOG("String addr: " << std::uppercase << std::hex << strBegin);
                //LOG("Type string: " << typeStr);

                if (std::strcmp(TARGET_NAME, "Weapon-Power-Mod") == 0 && typeStr == "SB_LIMITBODY_EXCESS_POWER_WEAPON") {
                    auto jeAddress = address - 11;

                    if (*jeAddress != 0x74) {
                        LOG("Error finding `JE` for `Weapon-Power-Mod`. Expected `74`, found `" << std::uppercase << std::hex << *jeAddress << "`. Aborting.");
                        continue;
                    }

                    LOG("JE offset (-11) address: " << std::uppercase << std::hex << reinterpret_cast<const UINT64>(jeAddress));

                    DoWithProtect(const_cast<BYTE*>(jeAddress), 1, [newBytes, jeAddress] {
                        memcpy(const_cast<BYTE*>(jeAddress), newBytes.data(), newBytes.size());
                    });
                } else {
                    DoWithProtect(const_cast<BYTE*>(address), 1, [newBytes, address] {
                        memcpy(const_cast<BYTE*>(address), newBytes.data(), newBytes.size());
                    });
                }

                LOG(typeStr << " patched.");
                patchedCount++;
            }
        }
    }

    LOG("Patched " << patchedCount << " match(es).");
}

extern "C" {
// Copied from `PluginAPI.h`.
// ReSharper disable once CppInconsistentNaming
__declspec(dllexport) SFSEPluginVersionData SFSEPlugin_Version = {
    SFSEPluginVersionData::kVersion,

    1,
    TARGET_NAME,
    "LordGregory",

    0, // not address independent
    0, // not structure independent
    {CURRENT_RELEASE_RUNTIME, 0}, // compatible with 1.7.23 and that's it

    0, // works with any version of the script extender. you probably do not need to put anything here
    0, 0, // set these reserved fields to 0
};

// ReSharper disable once CppInconsistentNaming
__declspec(dllexport) void SFSEPlugin_Load(const SFSEInterface* sfse) {
    out = SetupLog(GetLogPathAsCurrentDllDotLog());

    auto thread = std::thread([] {
        DoInjection();
    });
    if (thread.joinable()) thread.detach();

    LOG("Scan thread spawned."); // Don't remove. Plugin fails to load without it for some mysterious reason.
}
};