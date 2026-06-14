#include "pch.h"

#include "AoBSwap.h"
#include "Logger.h"
#include "ScanMemory.h"
#include "Util.h"

#include "lib/SFSE/sfse/PluginAPI.h"
#include "lib/SFSE/sfse_common/sfse_version.h"

#define LOG_VERSION(a) GET_EXE_VERSION_MAJOR(a) << "." << GET_EXE_VERSION_MINOR(a) << "." << GET_EXE_VERSION_BUILD(a) << "." << GET_EXE_VERSION_SUB(a)

/*
For game version 1.15.216.

0F 8? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 48 8D ?? ?? ?? ?? ?? 48 8D         (4k+ results.)
0F 8? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 48 8D ?? ?? ?? ?? ?? 48 8D ?? ?? E8 ?? ?? ?? ?? 90
90 E9
    SB_LIMITBODY_MAX_REACTOR

0F 8? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 48 8D ?? ?? ?? ?? ?? 48 8D      (200+ results.)
90 E9
    SB_LIMITBODY_MAX_SHIELD
    SB_ERRORBODY_NOT_ATTACHED
    SB_ERRORBODY_MODULE_BELOW_LANDINGBAY

0F 8? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 48 8D ?? ?? ?? ?? ?? 48 8D
90 E9
    SB_LIMITBODY_MAX_WEAPONS

83 38 0C 7F 0E ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 48 8D ?? ?? ?? ?? ?? 48 8D
         90 90
    SB_LIMITBODY_EXCESS_POWER_WEAPON

39 81 ?? ?? ?? ?? 0F 8F ?? ?? ?? ?? 48 81 C1 ?? ?? ?? ?? 48 3B CA 75 E8
                  90 90 90 90 90 90
    SB_ERRORBODY_REACTOR_CLASS

0F 8? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 48 8D ?? ?? ?? ?? ?? 48 8D
90 E9
    SB_LIMITBODY_MIN_LANDING_GEAR
    SB_LIMITBODY_MAX_LANDING_BAY
    SB_LIMITBODY_EXCESS_POWER_ENGINE
    SB_LIMITBODY_MAX_GRAV_DRIVE // Winds up with "you need additional grav thrust".
    SB_ERRORBODY_SHIP_TOO_HEAVY_TO_GRAVJUMP
    SB_LIMITBODY_MAX_COCKPIT

0F 8? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 48 8D ?? ?? ?? ?? ?? 48 8D
90 E9
    SB_LIMITBODY_MAX_DOCKER

E8 ?? ?? ?? ?? 84 ?? 74 ?? 48 8D ?? ?? E8 ?? ?? ?? ?? 84 C0 74 ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 76 ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 76 ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 76 ?? 48 8D ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 0F 85
                     90 90                                  90 90                                                    90 90                            90 90                            90 90                                           90 E9
    SB_ERRORBODY_DOCKER_INVALID_POSITION

73 37 48 83 C3 04 49 3B DE 49 BB 89 88 88 88 88 88 88 88
90 90
    SB_ERRORBODY_LANDINGENGINE_NOT_ALIGNED_WITH_LANDINGBAY
*/

struct ChangeInfo {
    std::string newBytes;
    int         newBytesOffset;
};

struct PatchInfo {
    std::string             sbConstString;
    std::string             scanBytes;
    std::vector<ChangeInfo> changes;
    int                     leaStart; // Will not check string const if -1.
};

// Note: Scanner doesn't support half-wildcards like `8?`. Need to replace it with `??`.
const std::map<std::string, std::vector<PatchInfo>> SCAN_INFO = {
    {
        "Allow-Unattached-Modules-Mod",
        {
            {"SB_ERRORBODY_NOT_ATTACHED", "0F ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 48 8D ?? ?? ?? ?? ?? 48 8D", {{"90 E9", 0}}, 16},
        }
    },
    {
        "BayAndDocker-Count-Mod",
        {
            {"SB_LIMITBODY_MAX_LANDING_BAY", "0F ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 48 8D ?? ?? ?? ?? ?? 48 8D", {{"90 E9", 0}}, 17},
            {"SB_LIMITBODY_MAX_DOCKER", "0F ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 48 8D ?? ?? ?? ?? ?? 48 8D", {{"90 E9", 0}}, 18},
        }
    },
    {
        "Build-Below-Bay-Mod",
        {
            {"SB_ERRORBODY_MODULE_BELOW_LANDINGBAY", "0F ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 48 8D ?? ?? ?? ?? ?? 48 8D", {{"90 E9", 0}}, 18},
            {
                "SB_ERRORBODY_DOCKER_INVALID_POSITION",
                "E8 ?? ?? ?? ?? 84 ?? 74 ?? 48 8D ?? ?? E8 ?? ?? ?? ?? 84 C0 74 ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 76 ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 76 ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 76 ?? 48 8D ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 0F 85",
                {
                    {"90 90", 7},
                    {"90 90", 20},
                    {"90 90", 39},
                    {"90 90", 50},
                    {"90 90", 61},
                    {"90 E9", 77},
                },
                -1
            }, // Only one match, don't check the string const.
            {"SB_ERRORBODY_LANDINGENGINE_NOT_ALIGNED_WITH_LANDINGBAY", "73 37 48 83 C3 04 49 3B DE 49 BB 89 88 88 88 88 88 88 88", {{"90 90", 0}}, -1}, // Only one match, don't check the string const.
        }
    },
    {
        "Cockpit-Count-Mod",
        {
            {"SB_LIMITBODY_MAX_COCKPIT", "0F ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 48 8D ?? ?? ?? ?? ?? 48 8D", {{"90 E9", 0}}, 17},
        }
    },
    {
        "Engine-Power-Mod",
        {
            {"SB_LIMITBODY_EXCESS_POWER_ENGINE", "0F ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 48 8D ?? ?? ?? ?? ?? 48 8D", {{"90 E9", 0}}, 17},
        }
    },
    {
        "GravDrive-Weight-Mod",
        {
            {"SB_ERRORBODY_SHIP_TOO_HEAVY_TO_GRAVJUMP", "0F ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 48 8D ?? ?? ?? ?? ?? 48 8D", {{"90 E9", 0}}, 17},
        }
    },
    {
        "LandingGear-Count-Mod",
        {
            {"SB_LIMITBODY_MIN_LANDING_GEAR", "0F ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 48 8D ?? ?? ?? ?? ?? 48 8D", {{"90 E9", 0}}, 17},
        }
    },
    {
        "Reactor-Class-Mod",
        {
            {"SB_ERRORBODY_REACTOR_CLASS", "39 81 ?? ?? ?? ?? 0F 8F ?? ?? ?? ?? 48 81 C1 ?? ?? ?? ?? 48 3B CA 75 E8", {{"90 90 90 90 90 90", 6}}, -1}, // Only one match, don't check the string const.
        }
    },
    {
        "Reactor-Count-Mod",
        {
            {"SB_LIMITBODY_MAX_REACTOR", "0F ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 48 8D ?? ?? ?? ?? ?? 48 8D ?? ?? E8 ?? ?? ?? ?? 90", {{"90 E9", 0}}, 16},
        }
    },
    {
        "Shield-Count-Mod",
        {
            {"SB_LIMITBODY_MAX_SHIELD", "0F ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 48 8D ?? ?? ?? ?? ?? 48 8D", {{"90 E9", 0}}, 16},
        }
    },
    {
        "Weapon-Power-Mod",
        {
            {"SB_LIMITBODY_EXCESS_POWER_WEAPON", "83 38 0C 7F 0E ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 48 8D ?? ?? ?? ?? ?? 48 8D", {{"90 90", 3}}, 30},
            {"SB_LIMITBODY_MAX_WEAPONS", "0F ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 48 8D ?? ?? ?? ?? ?? 48 8D", {{"90 E9", 0}}, 14},
        }
    },
};

inline bool IsAddressValid(const UINT64 address, const UINT64 moduleAddr, const DWORD moduleSize) {
    return address > moduleAddr && address < moduleAddr + moduleSize;
}

void DoInjection() {
    LOG(TARGET_NAME << " loading.");

    constexpr auto targetVersion = CURRENT_RELEASE_RUNTIME;
    const auto     gameVersion   = GetGameVersion();
    LOG("Target version: " << LOG_VERSION(targetVersion));
    LOG("Game version: " << LOG_VERSION(gameVersion));
    if (targetVersion != gameVersion) {
        LOG("WARNING: TARGET VERSION DOES NOT MATCH DETECTED GAME VERSION! Patching may or may not work.");
        LOG("If you're deliberately running this on an older release expect zero support and do not open bug reports about it not working.");
    }

    const auto moduleName   = GetExeFilename();
    const auto moduleHandle = GetModuleHandle(moduleName.c_str());
    const auto moduleAddr   = reinterpret_cast<const UINT64>(moduleHandle);
    MODULEINFO moduleInfo{};
    GetModuleInformation(GetCurrentProcess(), moduleHandle, std::addressof(moduleInfo), sizeof(moduleInfo));
    const auto moduleSize = moduleInfo.SizeOfImage;
    LOG("Found module name: " << moduleName);
    LOG("Module base address: " << std::uppercase << std::hex << moduleAddr);
    LOG("Module size: " << moduleSize);

    auto patchedCount = 0;

    const auto patchInfos = SCAN_INFO.at(TARGET_NAME);
    LOG("Looking for " << patchInfos.size() << " patch targets.");

    for (const auto& patchInfo : patchInfos) {
        LOG("Doing AoB scan for: " << patchInfo.sbConstString);

        auto addressesFound = ScanMemory(moduleName, patchInfo.scanBytes);
        if (addressesFound.empty()) {
            LOG("AoB scan returned no results, aborting.");
            return;
        }

        LOG("Found " << addressesFound.size() << " potential match(es).");

        for (const auto& address : addressesFound) {
            const auto addrBase     = reinterpret_cast<const UINT64>(address);
            const auto moduleOffset = addrBase - moduleAddr;

            if (patchInfo.leaStart > -1) {
                //LOG("Checking address: " << std::uppercase << std::hex << addrBase << " (" << moduleName << " + " << moduleOffset << ")");
                // Find the start of the `lea`.
                const auto leaAddress = address + patchInfo.leaStart;
                const auto leaBase    = reinterpret_cast<const UINT64>(leaAddress);
                //LOG("`lea` found at: +" << std::uppercase << std::hex << leaBase - moduleAddr);
                const auto leaOffset = *reinterpret_cast<const UINT32*>(leaAddress + 3); // In short, move the ptr 3 bytes, and dereference the 4 bytes (the `lea` offset) as an int. // NOLINT(clang-diagnostic-cast-qual)
                //LOG("LEA offset: " << std::uppercase << std::hex << leaOffset);

                const UINT64 strBegin = leaBase + leaOffset + 8; // +7 to offset to the end of the `lea` op, +1 to skip the char count. They're null-terminated anyways.
                //LOG("String addr: " << std::uppercase << std::hex << strBegin << " (" << moduleName << " + " << (strBegin - moduleAddr) << ")");

                if (!IsAddressValid(strBegin, moduleAddr, moduleSize)) continue;

                const auto typeStr = std::string(reinterpret_cast<const char*>(strBegin)); // NOLINT(performance-no-int-to-ptr)
                //LOG("Read LEA string const: " << typeStr);

                if (typeStr == patchInfo.sbConstString) {
                    goto doPatch;
                }
            } else {
            doPatch:
                LOG("Target address: " << std::uppercase << std::hex << addrBase << " (" << moduleName << " + " << moduleOffset << ")");
                LOG("Change count: " << patchInfo.changes.size());

                for (UINT64 i = 0; i < patchInfo.changes.size(); i++) {
                    const auto change = patchInfo.changes[i];

                    const auto writeAddress = address + change.newBytesOffset;
                    const auto newBytes     = StringToByteVector(change.newBytes);

                    DoWithProtect(const_cast<BYTE*>(writeAddress), newBytes.size(), [writeAddress, newBytes] {
                        memcpy(const_cast<BYTE*>(writeAddress), newBytes.data(), newBytes.size());
                    });
                    const auto changeAddr         = reinterpret_cast<const UINT64>(writeAddress);
                    const auto changeModuleOffset = changeAddr - moduleAddr;

                    LOG("Change " << (i + 1) << " patched: " << std::uppercase << std::hex << changeAddr << " (" << moduleName << " + " << changeModuleOffset << ")");
                    LOG("    Wrote: " << BytesToString(newBytes));
                }

                LOG(patchInfo.sbConstString << " patched.");
                patchedCount++;
            }
        }
    }

    LOG("Patched " << patchedCount << " match(es).");
}

BOOL WINAPI DllMain(HINSTANCE hInst, const DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        // Do nothing if not an ASI as SFSE will handle it instead.
        if (!EndsWith(GetFullModulePath(), ".asi")) return TRUE;

        SetupLog(GetLogPathAsCurrentDllDotLog());

        LOG(TARGET_NAME << " initializing.");

        auto thread = std::thread([] {
            DoInjection();
        });
        if (thread.joinable()) thread.detach();

        LOG("Scan thread spawned.");
    }
    return TRUE;
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
    {CURRENT_RELEASE_RUNTIME, 0}, // compatible with 1.13.61 and that's it

    0, // works with any version of the script extender. you probably do not need to put anything here
    0, 0, // set these reserved fields to 0
};

// ReSharper disable once CppInconsistentNaming
// ReSharper disable once CppParameterNeverUsed
__declspec(dllexport) bool SFSEPlugin_Load(const SFSEInterface* sfse) {
    SetupLog(GetLogPathAsCurrentDllDotLog());

    LOG(TARGET_NAME << " initializing.");

    auto thread = std::thread([] {
        DoInjection();
    });
    if (thread.joinable()) thread.detach();

    LOG("Scan thread spawned.");
    return true;
}
};