#include <functional>

#include "ctfOverseerAPI.h"
#include "bzfsAPI.h"

class SamplePlugin : public bz_Plugin
{
public:
    virtual const char* Name();
    virtual void Init(const char* config);
    virtual void Cleanup();
    virtual void Event(bz_EventData* eventData) {};

private:
    int onCaptureEventListenerID;
    void OnCaptureEventCallback(int playerID, bool isUnfair, bool wasDisallowed, bool isSelfCap);

    const char* getCTFOverseerPlugin();
    void registerCTFOverseerCallback();
    void removeCTFOverseerCallback();
};

BZ_PLUGIN(SamplePlugin)

const char* SamplePlugin::Name()
{
    return "ctfOverseer Sample Callback Plugin";
}

void SamplePlugin::Init(const char* config)
{
    registerCTFOverseerCallback();
}

void SamplePlugin::Cleanup()
{
    Flush();

    removeCTFOverseerCallback();
}

const char* SamplePlugin::getCTFOverseerPlugin()
{
    const char* pluginName = bz_getclipFieldString("allejo/ctfOverseer");

    if (pluginName == NULL)
    {
        bz_debugMessage(0, "ERROR! The plug-in for allejo/ctfOverseer could not be found.");
    }
    else if (!bz_pluginExists(pluginName))
    {
        bz_debugMessage(0, "ERROR! The required CTFOverseer plug-in is not loaded. Be sure it is loaded before this plug-in.");
    }

    return pluginName;
}

void SamplePlugin::registerCTFOverseerCallback()
{
    using namespace std::placeholders;

    const char* ctfOverseer = getCTFOverseerPlugin();

    if (ctfOverseer == NULL)
    {
        return;
    }

    OnCaptureEventCallbackV1 callback = std::bind(&SamplePlugin::OnCaptureEventCallback, this, _1, _2, _3, _4);
    onCaptureEventListenerID = bz_callPluginGenericCallback(ctfOverseer, "listenOnCaptureV1", static_cast<void*>(&callback));

    bz_debugMessagef(0, "Event Listener registered with ID: %d", onCaptureEventListenerID);
}

void SamplePlugin::removeCTFOverseerCallback()
{
    const char* ctfOverseer = getCTFOverseerPlugin();

    if (ctfOverseer == NULL)
    {
        return;
    }

    bz_callPluginGenericCallback(ctfOverseer, "removeOnCapture", static_cast<int*>(&onCaptureEventListenerID));
}

void SamplePlugin::OnCaptureEventCallback(int playerID, bool isUnfair, bool wasDisallowed, bool isSelfCap)
{
    bz_debugMessagef(0, "Player Capping: %d", playerID);
    bz_debugMessagef(0, "Was Unfair?: %s", isUnfair ? "true" : "false");
    bz_debugMessagef(0, "Was Disallowed: %s", wasDisallowed ? "true" : "false");
    bz_debugMessagef(0, "Was Self-cap: %s", isSelfCap ? "true" : "false");
}
