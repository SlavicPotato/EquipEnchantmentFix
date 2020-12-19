#pragma once

namespace SKSE
{
    extern PluginHandle g_pluginHandle;

    extern SKSEMessagingInterface* g_messaging;
    extern SKSETaskInterface* g_taskInterface;

    extern bool Query(const SKSEInterface* skse, PluginInfo* info);
    extern bool Initialize(const SKSEInterface* skse);
}