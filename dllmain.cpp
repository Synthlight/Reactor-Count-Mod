#include "pch.h"

#include "AoBSwap.h"
#include "Logger.h"
#include "ScanMemory.h"
#include "Util.h"

#include "lib/SFSE/sfse/PluginAPI.h"
#include "lib/SFSE/sfse_common/sfse_version.h"

std::ofstream out;

std::map<std::string, std::vector<std::string>> TYPE_MAPPING = {
    //{"GravDrive-Count-Mod", std::vector<std::string> {"SB_LIMITBODY_MAX_GRAV_DRIVE"}}, // Winds up with "you need additional grav thrust".
    {"LandingGear-Count-Mod", std::vector<std::string> {"SB_LIMITBODY_MIN_LANDING_GEAR"}},
    {"Reactor-Count-Mod", std::vector<std::string> {"SB_LIMITBODY_MAX_REACTOR"}},
    {"Reactor-Class-Mod", std::vector<std::string> {"SB_ERRORBODY_REACTOR_CLASS"}},
    {"Shield-Count-Mod", std::vector<std::string> {"SB_LIMITBODY_MAX_SHIELD"}},
};

std::map<std::string, std::string> SCAN_MAPPING = {
    //{"GravDrive-Count-Mod", "7E ?? 48 8D 15 ?? ?? ?? ?? 48 8D 4D 50"}, // 7E == `jle`
    {"LandingGear-Count-Mod", "75 ?? 48 8D 15 ?? ?? ?? ?? 48 8D 4D 50"}, // 75 == `jne`
    {"Reactor-Count-Mod", "7E ?? 48 8D 15 ?? ?? ?? ?? 48 8D 4D 50"}, // 7E == `jle`
    {"Reactor-Class-Mod", "75 ?? 48 8D 15 ?? ?? ?? ?? 48 8D 4D 50"}, // 75 == `jne`
    {"Shield-Count-Mod", "7E ?? 48 8D 15 ?? ?? ?? ?? 48 8D 4D 50"}, // 7E == `jle`
};

extern "C" {
    // Copied from `PluginAPI.h`.
    __declspec(dllexport) SFSEPluginVersionData SFSEPlugin_Version = {
        SFSEPluginVersionData::kVersion,

        1,
        TARGET_NAME,
        "LordGregory",

        0,	// not address independent
        0,	// not structure independent
        { CURRENT_RELEASE_RUNTIME, 0 },	// compatible with 1.7.23 and that's it

        0,	// works with any version of the script extender. you probably do not need to put anything here
        0, 0,	// set these reserved fields to 0
    };
};

void PrintNBytes(const BYTE* address, const int length) {
    out << NOW << '\t' << "Bytes: ";
    for (auto i = 0; i < length; i++) {
        if (i > 0) out << ",";
        out << std::uppercase << std::hex << UINT32(*(address + i));
    }
    out << std::endl;
}

void DoInjection() {
    Log(TARGET_NAME << " loading.");

    auto moduleName = GetExeFilename();
    Log("Found module name: " << moduleName);

    Log("Doing AoB scan.");

    auto addressesFound = ScanMemory(moduleName, SCAN_MAPPING[TARGET_NAME]); // 7E == `jle`
    if (addressesFound.empty()) {
        Log("AoB scan returned no results, aborting.");
        return;
    }

    auto newBytes = StringToByteVector("EB"); // EB == `jmp`
    Log("Found " << addressesFound.size() << " match(es).");

    auto& validTypes = TYPE_MAPPING[TARGET_NAME];
    auto patchedCount = 0;

    for (auto address : addressesFound) {
        UINT64 addrBase  = (UINT64) address;
        UINT32 leaOffset = UINT32(*((UINT32*) (address + 5)));// In short, move the ptr 5 bytes, and dereference the 4 bytes (the `lea` offset) as an int.
        UINT64 strBegin  = addrBase + leaOffset + 10; // +9 to offset to the end of the `lea` op, +1 to skip the char count. They're null-terminated anyways.
        auto typeStr     = std::string(reinterpret_cast<char*>(strBegin));

        if (contains(validTypes, typeStr)) {
            Log("Target address: " << std::uppercase << std::hex << addrBase);
            //PrintNBytes(address, 13);
            //Log("LEA offset: " << std::uppercase << std::hex << leaOffset);
            //Log("String addr: " << std::uppercase << std::hex << strBegin);
            //Log("Type string: " << typeStr);

            DoWithProtect((BYTE*) address, 1, [newBytes, address]() {
                memcpy((BYTE*) address, newBytes.data(), newBytes.size());
            });
            Log(typeStr << " patched.");
            patchedCount++;
        }
    }

    Log("Patched " << patchedCount << " match(es).");
}

extern "C" {
    __declspec(dllexport) void SFSEPlugin_Load(const SFSEInterface* sfse) {
        out = SetupLog(GetLogPathAsCurrentDllDotLog());

        auto thread = std::thread([]() {
            DoInjection();
        });
        if (thread.joinable()) thread.detach();

        Log("Scan thread spawned.");
    }
};