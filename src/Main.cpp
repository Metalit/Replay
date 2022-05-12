#include "Main.hpp"

static ModInfo modInfo;

Logger& getLogger() {
    static Logger* logger = new Logger(modInfo);
    return *logger;
}

extern "C" void setup(ModInfo& info) {
    info.id = MOD_ID;
    info.version = VERSION;
    modInfo = info;
	
    getLogger().info("Completed setup!");
}

extern "C" void load() {
    il2cpp_functions::Init();

    LOG_INFO("Installing hooks...");
    // Install our hooks (none defined yet)
    LOG_INFO("Installed all hooks!");
}
