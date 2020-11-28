#pragma once

namespace SKSE
{
    extern PluginHandle g_pluginHandle;

    extern SKSEMessagingInterface* g_messaging;

    extern bool Query(const SKSEInterface* skse, PluginInfo* info);
    extern bool Initialize(const SKSEInterface* skse);
}