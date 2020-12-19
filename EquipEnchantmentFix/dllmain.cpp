#include "pch.h"

static bool Initialize(const SKSEInterface* a_skse)
{
    if (!IAL::IsLoaded()) {
        gLog.FatalError("Could not load the address library");
        return false;
    }

    if (IAL::HasBadQuery()) {
        gLog.FatalError("One or more addresses could not be retrieved from the database");
        return false;
    }

    if (!SKSE::Initialize(a_skse)) {
        return false;
    }

    return EEF::Initialize();
}

extern "C"
{
    bool SKSEPlugin_Query(const SKSEInterface* a_skse, PluginInfo* a_info)
    {
        return SKSE::Query(a_skse, a_info);
    }

    bool SKSEPlugin_Load(const SKSEInterface* a_skse)
    {
        gLog.Message("Initializing %s version %s (runtime %u.%u.%u.%u)",
            PLUGIN_NAME, PLUGIN_VERSION_VERSTRING,
            GET_EXE_VERSION_MAJOR(a_skse->runtimeVersion),
            GET_EXE_VERSION_MINOR(a_skse->runtimeVersion),
            GET_EXE_VERSION_BUILD(a_skse->runtimeVersion),
            GET_EXE_VERSION_SUB(a_skse->runtimeVersion));

        bool ret = Initialize(a_skse);

        IAL::Release();

        if (ret)
            gLog.Message("Done");

        gLog.Close();

        return ret;
    }
};