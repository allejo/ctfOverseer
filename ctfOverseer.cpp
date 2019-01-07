/*
 * Copyright (C) 2019 Vladimir "allejo" Jimenez
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <map>

#include "bzfsAPI.h"
#include "plugin_utils.h"

// Define plugin name
const std::string PLUGIN_NAME = "CTF Overseer";

// Define plugin version numbering
const int MAJOR = 1;
const int MINOR = 0;
const int REV = 0;
const int BUILD = 2;

typedef std::map<std::string, std::string> StringDict;
typedef std::pair<bz_eTeamType, bz_eTeamType> TeamPair;

struct Configuration
{
    std::string SelfCapturePublicMessage;
    std::string SelfCapturePrivateMessage;
    
    std::string FairCapturePublicMessage;
    std::string FairCapturePrivateMessage;

    std::string UnfairCapturePublicMessage;
    std::string UnfairCapturePrivateMessage;
};

class RelativeCaptureBonus : public bz_Plugin, public bz_CustomSlashCommandHandler
{
public:
    const char* Name();
    void Init(const char* config);
    void Cleanup();
    void Event(bz_EventData* eventData);
    int GeneralCallback(const char* name, void* data);
    bool SlashCommand(int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList *params);

private:
    void loadConfigurationFile();
    void safeSendMessage(const std::string msg, int recipient, StringDict placeholders);
    void formatString(bz_ApiString &string, StringDict placeholders);
    
    bool calcCapturePoints(bz_eTeamType capping, bz_eTeamType capped);
    bool isFairCapture(bz_eTeamType capping, bz_eTeamType capped);
    
    Configuration settings;
    
    std::map<bz_eTeamType, double> lastCapTime; /// The server time when a team was last capped on
    std::map<int, double> lastFlagWarnMsg; /// The server time a warning was sent to a player trying to grab a team flag
    std::map<int, int> captureBonus; /// The number of players a team had when its flag was grabbed

    const char* bzdb_delayTeamFlagGrab = "_delayTeamFlagGrab";
    const char* bzdb_disallowUnfairCap = "_disallowUnfairCap";
    const char* bzdb_disallowSelfCap = "_disallowSelfCap";
    const char* bzdb_warnUnfairTeams = "_warnUnfairTeams";
    const char* configFile;
};

BZ_PLUGIN(RelativeCaptureBonus)

const char* RelativeCaptureBonus::Name()
{
    static const char* pluginBuild;
    
    if (!pluginBuild)
    {
        pluginBuild = bz_format("%s %d.%d.%d (%d)", PLUGIN_NAME.c_str(), MAJOR, MINOR, REV, BUILD);
    }
    
    return pluginBuild;
}

void RelativeCaptureBonus::Init(const char* config)
{
    configFile = config;

    loadConfigurationFile();

    // Namespace our clip fields to avoid plug-in conflicts
    bz_setclipFieldString("allejo/ctfOverseer", Name());
    
    Register(bz_eAllowCTFCaptureEvent);
    Register(bz_eAllowFlagGrab);
    Register(bz_eCaptureEvent);
    Register(bz_eFlagGrabbedEvent);

    bz_registerCustomBZDBInt(bzdb_delayTeamFlagGrab, 20);
    bz_registerCustomBZDBBool(bzdb_disallowUnfairCap, true);
    bz_registerCustomBZDBBool(bzdb_disallowSelfCap, true);
    bz_registerCustomBZDBBool(bzdb_warnUnfairTeams, true);

    bz_registerCustomSlashCommand("reload", this);
}

void RelativeCaptureBonus::Cleanup()
{
    Flush();

    bz_removeCustomBZDBVariable(bzdb_delayTeamFlagGrab);
    bz_removeCustomBZDBVariable(bzdb_disallowUnfairCap);
    bz_removeCustomBZDBVariable(bzdb_disallowSelfCap);
    bz_removeCustomBZDBVariable(bzdb_warnUnfairTeams);
    
    bz_removeCustomSlashCommand("reload");
}

int RelativeCaptureBonus::GeneralCallback(const char *name, void *data)
{
    if (!name)
    {
        return -9999;
    }

    std::string callback = name;

    if (callback == "calcBonusPoints")
    {
        TeamPair* pair = static_cast<TeamPair*>(data);

        return calcCapturePoints(pair->first, pair->second);
    }
    else if (callback == "isFairCapture")
    {
        TeamPair* pair = static_cast<TeamPair*>(data);

        return (int)isFairCapture(pair->first, pair->second);
    }

    return -9999;
}

void RelativeCaptureBonus::Event(bz_EventData* eventData)
{
    switch (eventData->eventType)
    {
        case bz_eAllowCTFCaptureEvent:
        {
            bz_AllowCTFCaptureEventData_V1 *data = (bz_AllowCTFCaptureEventData_V1*)eventData;

            bool areSelfCapsDisabled = bz_getBZDBBool(bzdb_disallowSelfCap);
            bool isSelfCap = bz_getPlayerTeam(data->playerCapping) == data->teamCapped;

            if (areSelfCapsDisabled && isSelfCap)
            {
                data->allow = false;
            }
        }
        break;
            
        case bz_eAllowFlagGrab:
        {
            bz_AllowFlagGrabData_V1* data = (bz_AllowFlagGrabData_V1*)eventData;
            
            bz_eTeamType team = bzu_getTeamFromFlag(data->flagType);
            
            // Don't disallow flag grabs of regular flags for any reason
            if (!(eRedTeam <= team && team <= ePurpleTeam))
            {
                return;
            }
            
            double teamFlagGrabDelay = bz_getBZDBInt(bzdb_delayTeamFlagGrab);
            double safeGrabTime = lastCapTime[team] + teamFlagGrabDelay;
            
            // If the `_delayTeamFlagGrab` variable is set to a nevative number, allow immediate flag grabs after capture
            if (teamFlagGrabDelay < 0)
            {
                return;
            }
            
            int playerID = data->playerID;
            
            // Don't allow flag grabs on team flags that were captured less than `_delayTeamFlagGrab` seconds ago
            if (bz_getCurrentTime() < safeGrabTime)
            {
                data->allow = false;
                
                double safeMsgTime = lastFlagWarnMsg[playerID] + 5;
                
                if (bz_getCurrentTime() > safeMsgTime)
                {
                    bz_sendTextMessagef(BZ_SERVER, playerID, "Team flags cannot be grabbed for %d seconds after they were last capped.", teamFlagGrabDelay);
                    bz_sendTextMessagef(BZ_SERVER, playerID, "You cannot grab the %s team flag for another ~%d seconds", bzu_GetTeamName(team), (safeGrabTime - bz_getCurrentTime()));
                    
                    lastFlagWarnMsg[playerID] = bz_getCurrentTime();
                }
            }
        }

        case bz_eCaptureEvent:
        {
            bz_CTFCaptureEventData_V1 *data = (bz_CTFCaptureEventData_V1*)eventData;

            lastCapTime[data->teamCapped] = bz_getCurrentTime();
            
            StringDict placeholders;
            placeholders["{capper}"] = bz_getPlayerCallsign(data->playerCapping);
            placeholders["{teamCapping}"] = bzu_GetTeamName(data->teamCapped);
            placeholders["{teamCapped}"] = bzu_GetTeamName(data->teamCapped);
            
            // A self-capture
            if (data->teamCapped == data->teamCapping)
            {
                int penalty = 5 * bz_getTeamCount(data->teamCapped);
                
                bz_incrementPlayerLosses(data->playerCapping, penalty);
                
                placeholders["{points}"] = std::to_string(-1 * penalty).c_str();
                placeholders["{pointsAbs}"] = std::to_string(penalty).c_str();
                
                safeSendMessage(settings.SelfCapturePublicMessage, BZ_SERVER, placeholders);
                safeSendMessage(settings.SelfCapturePrivateMessage, data->playerCapping, placeholders);

                return;
            }
            
            bool isFair = isFairCapture(data->teamCapping, data->teamCapped);
            int bonusPoints = abs(captureBonus[data->playerCapping]);
            
            if (isFair)
            {
                bz_incrementPlayerWins(data->playerCapping, bonusPoints);
                
                placeholders["{points}"] = placeholders["{pointsAbs}"] = std::to_string(bonusPoints).c_str();
                
                safeSendMessage(settings.FairCapturePublicMessage, BZ_SERVER, placeholders);
                safeSendMessage(settings.FairCapturePrivateMessage, data->playerCapping, placeholders);
            }
            else
            {
                bz_incrementPlayerLosses(data->playerCapping, bonusPoints);
                
                placeholders["{points}"] = std::to_string(-1 * bonusPoints).c_str();
                placeholders["{pointsAbs}"] = std::to_string(bonusPoints).c_str();
                
                safeSendMessage(settings.UnfairCapturePublicMessage, BZ_SERVER, placeholders);
                safeSendMessage(settings.UnfairCapturePrivateMessage, data->playerCapping, placeholders);
            }
        }
        break;

        case bz_eFlagGrabbedEvent:
        {
            bz_FlagGrabbedEventData_V1 *data = (bz_FlagGrabbedEventData_V1*)eventData;

            bz_eTeamType flagTeam = bzu_getTeamFromFlag(data->flagType);
            bz_eTeamType grabTeam = bz_getPlayerTeam(data->playerID);

            if (grabTeam >= eRedTeam && grabTeam <= ePurpleTeam)
            {
                int flagTeamSize = bz_getTeamCount(flagTeam);
                int grabTeamSize = bz_getTeamCount(grabTeam);

                int capValue = calcCapturePoints(grabTeam, flagTeam);

                captureBonus[data->playerID] = capValue;

                if (capValue < 0 && flagTeamSize > 0)
                {
                    bz_sendTextMessagef(BZ_SERVER, data->playerID, "%d vs %d? Don't be a bad sport.", grabTeamSize, flagTeamSize);
                }
            }
        }
        break;

        default:
            break;
    }
}

bool RelativeCaptureBonus::SlashCommand(int playerID, bz_ApiString command, bz_ApiString /*message*/, bz_APIStringList *params)
{
    if (command == "reload" && bz_hasPerm(playerID, "setAll"))
    {
        if (params->size() == 1 && params->get(0) == "relativeCaptureBonus")
        {
            loadConfigurationFile();
            bz_sendTextMessage(BZ_SERVER, playerID, "Relative Capture Bonus database reloaded");
            
            return true;
        }
        
        if (params->size() == 0)
        {
            loadConfigurationFile();
        }
        
        return false;
    }
    
    return false;
}

void RelativeCaptureBonus::loadConfigurationFile()
{
    const char* section = "relativeCaptureBonus";
    PluginConfig plgCfg = PluginConfig(configFile);
    
    if (plgCfg.errors)
    {
        bz_debugMessagef(0, "ERROR :: Relative Capture Bonus :: There was an error reading the configuration file: %s", configFile);
        return;
    }
    
    settings.SelfCapturePublicMessage = bz_trim(plgCfg.item(section, "self_cap_message_pub").c_str(), "\"");
    settings.SelfCapturePrivateMessage = bz_trim(plgCfg.item(section, "self_cap_message_pm").c_str(), "\"");
    settings.FairCapturePublicMessage = bz_trim(plgCfg.item(section, "fair_cap_message_pub").c_str(), "\"");
    settings.FairCapturePrivateMessage = bz_trim(plgCfg.item(section, "fair_cap_message_pm").c_str(), "\"");
    settings.UnfairCapturePublicMessage = bz_trim(plgCfg.item(section, "unfair_cap_message_pub").c_str(), "\"");
    settings.UnfairCapturePrivateMessage = bz_trim(plgCfg.item(section, "unfair_cap_message_pm").c_str(), "\"");
}

void RelativeCaptureBonus::safeSendMessage(const std::string rawMsg, int recipient, StringDict placeholders)
{
    if (rawMsg == "")
    {
        return;
    }
    
    bz_ApiString msg = rawMsg;
    formatString(msg, placeholders);
    bz_sendTextMessage(BZ_SERVER, recipient, msg.c_str());
}

void RelativeCaptureBonus::formatString(bz_ApiString &string, StringDict placeholders)
{
    for (auto it : placeholders)
    {
        string.replaceAll(it.first.c_str(), it.second.c_str());
    }
}

bool RelativeCaptureBonus::calcCapturePoints(bz_eTeamType capping, bz_eTeamType capped)
{
    int losingTeamSize = bz_getTeamCount(capped);
    int cappingTeamSize = bz_getTeamCount(capping);
    
    return (3 * losingTeamSize + 8 * (losingTeamSize - cappingTeamSize));
}

bool RelativeCaptureBonus::isFairCapture(bz_eTeamType capping, bz_eTeamType capped)
{
    return calcCapturePoints(capping, capped) > 0;
}
