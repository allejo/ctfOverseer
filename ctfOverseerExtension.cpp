#include <functional>

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
    void OnCaptureEventCallbackV1(int playerID, bool isUnfair, bool wasDisallowed, bool isSelfCap);
};

BZ_PLUGIN(SamplePlugin)

const char* SamplePlugin::Name()
{
    return "ctfOverseer Sample Callback Plugin";
}

void SamplePlugin::Init(const char* config)
{
    using namespace std::placeholders;

    std::string ctfOverseer = bz_getclipFieldString("allejo/ctfOverseer");
    std::function<void(int, bool, bool, bool)> callback = std::bind(&SamplePlugin::OnCaptureEventCallbackV1, this, _1, _2, _3, _4);
    onCaptureEventListenerID = bz_callPluginGenericCallback(ctfOverseer.c_str(), "listenOnCaptureV1", static_cast<void*>(&callback));

    bz_debugMessagef(0, "Event Listener registered: %d", onCaptureEventListenerID);
}

void SamplePlugin::Cleanup()
{
    Flush();

    std::string ctfOverseer = bz_getclipFieldString("allejo/ctfOverseer");
    bz_callPluginGenericCallback(ctfOverseer.c_str(), "removeOnCapture", static_cast<int*>(&onCaptureEventListenerID));
}

void SamplePlugin::OnCaptureEventCallbackV1(int playerID, bool isUnfair, bool wasDisallowed, bool isSelfCap)
{
    bz_debugMessagef(0, "Player Capping: %d", playerID);
    bz_debugMessagef(0, "Was Unfair?: %s", isUnfair ? "true" : "false");
    bz_debugMessagef(0, "Was Disallowed: %s", wasDisallowed ? "true" : "false");
    bz_debugMessagef(0, "Was Self-cap: %s", isSelfCap ? "true" : "false");
}
