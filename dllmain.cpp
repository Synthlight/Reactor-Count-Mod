#include "pch.h"

#include "AoBSwap.h"
#include "Logger.h"
#include "ScanMemory.h"
#include "Util.h"

#include "lib/SFSE/sfse/PluginAPI.h"
#include "lib/SFSE/sfse_common/sfse_version.h"

std::ofstream out;

extern "C" {
    // Copied from `PluginAPI.h`.
    __declspec(dllexport) SFSEPluginVersionData SFSEPlugin_Version = {
        SFSEPluginVersionData::kVersion,

        1,
        "Reactor-Count-Mod",
        "LordGregory",

        0,	// not address independent
        0,	// not structure independent
        { CURRENT_RELEASE_RUNTIME, 0 },	// compatible with 1.7.23 and that's it

        0,	// works with any version of the script extender. you probably do not need to put anything here
        0, 0,	// set these reserved fields to 0
    };
};

void DoInjection() {
    out = SetupLog(GetLogPathAsCurrentDllDotLog());

    Log("Reactor-Count-Mod loading.");

    auto moduleName = GetExeFilename();
    Log("Found module name: " << moduleName);

    Log("Doing AoB scan.");

    // There should only be one match.
    auto addressesFound = ScanMemory(moduleName, "7E 58 48 8D 15 ?? ?? ?? ?? 48 8D 4D 50");
    if (addressesFound.empty()) {
        Log("AoB scan returned no results, aborting.");
        return;
    } else if (addressesFound.size() > 1) {
        Log("AoB scan returned more than one result, aborting.");
        return;
    }

    auto addr = addressesFound[0];

    Log("Found at: " << addressesFound.data());

    auto newBytes = StringToByteVector("EB");

    DoWithProtect((BYTE*) addr, 1, [newBytes, addr]() {
        memcpy((BYTE*) addr, newBytes.data(), newBytes.size());
    });

    Log("Max reactor count patched.");
}

extern "C" {
    __declspec(dllexport) void SFSEPlugin_Load(const SFSEInterface* sfse) {
// Debug is slow so thread it. Release is done in seconds.
// Release also fails/crashes with threading so...
#ifdef _DEBUG
        auto thread = std::thread([]() {
            DoInjection();
        });
        thread.detach();
#else
        DoInjection();
#endif
    }
};