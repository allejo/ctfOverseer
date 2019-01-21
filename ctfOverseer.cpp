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
const int BUILD = 23;

// Plugin settings
const int RECALC_INTERVAL = 20; /// The number of seconds between a flag drop and point bonus point recalculation
const int SELF_CAP_MULTIPLIER = 5; /// The pentality multiplier for self-caps; this number times the current team size
const int MESSAGE_SPAM_INTERVAL = 5; /// The number of seconds between a message should be sent to prevent spamming players
const int VERBOSE_DEBUG_LEVEL = 4; /// The debug level that verbose messages will be written out at

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

class CTFOverseer : public bz_Plugin, public bz_CustomSlashCommandHandler
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

    bool isFairCapture(bz_eTeamType capping, bz_eTeamType capped);
    int calcCapturePoints(bz_eTeamType capping, bz_eTeamType capped);

    Configuration settings;

    std::map<bz_eTeamType, double> lastCapTime; /// The server time when a team was last capped on
    std::map<bz_eTeamType, int> capBonus; /// The number of points a capture will be worth against this team
    std::map<int, double> lastFlagDrop; /// The server time a team flag was last dropped
    std::map<int, double> lastFlagWarnMsg; /// The server time a warning was sent to a player trying to grab a team flag

    const char* bzdb_delayTeamFlagGrab = "_delayTeamFlagGrab";
    const char* bzdb_disallowSelfCap = "_disallowSelfCap";
    const char* bzdb_warnUnfairTeams = "_warnUnfairTeams";
    const char* configFile;
};

BZ_PLUGIN(CTFOverseer)

const char* CTFOverseer::Name()
{
    static const char* pluginBuild;

    if (!pluginBuild)
    {
        pluginBuild = bz_format("%s %d.%d.%d (%d)", PLUGIN_NAME.c_str(), MAJOR, MINOR, REV, BUILD);
    }

    return pluginBuild;
}

void CTFOverseer::Init(const char* config)
{
    configFile = config;

    loadConfigurationFile();

    // Namespace our clip fields to avoid plug-in conflicts
    bz_setclipFieldString("allejo/ctfOverseer", Name());

    Register(bz_eAllowCTFCaptureEvent);
    Register(bz_eAllowFlagGrab);
    Register(bz_eCaptureEvent);
    Register(bz_eFlagGrabbedEvent);
    Register(bz_eFlagDroppedEvent);

    bz_registerCustomBZDBInt(bzdb_delayTeamFlagGrab, 20);
    bz_registerCustomBZDBBool(bzdb_disallowSelfCap, true);
    bz_registerCustomBZDBBool(bzdb_warnUnfairTeams, true);

    bz_registerCustomSlashCommand("reload", this);
}

void CTFOverseer::Cleanup()
{
    Flush();

    bz_removeCustomBZDBVariable(bzdb_delayTeamFlagGrab);
    bz_removeCustomBZDBVariable(bzdb_disallowSelfCap);
    bz_removeCustomBZDBVariable(bzdb_warnUnfairTeams);

    bz_removeCustomSlashCommand("reload");
}

int CTFOverseer::GeneralCallback(const char* name, void* data)
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

void CTFOverseer::Event(bz_EventData* eventData)
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

            // If the team hasn't been capped against yet, don't disallow anything
            if (!lastCapTime.count(team))
            {
                return;
            }

            int teamFlagGrabDelay = bz_getBZDBInt(bzdb_delayTeamFlagGrab);
            double safeGrabTime = lastCapTime[team] + teamFlagGrabDelay;

            // If the `_delayTeamFlagGrab` variable is set to a negative number, allow immediate flag grabs after capture
            if (teamFlagGrabDelay < 0)
            {
                return;
            }

            int playerID = data->playerID;

            // Don't allow flag grabs on team flags that were captured less than `_delayTeamFlagGrab` seconds ago
            if (bz_getCurrentTime() < safeGrabTime && bz_getPlayerTeam(data->playerID) != team)
            {
                data->allow = false;

                double safeMsgTime = lastFlagWarnMsg[playerID] + MESSAGE_SPAM_INTERVAL;

                // Don't spam our users if they continue trying to grab it
                if (bz_getCurrentTime() > safeMsgTime)
                {
                    bz_sendTextMessagef(BZ_SERVER, playerID, "Team flags cannot be grabbed for %d seconds after they were last capped.", teamFlagGrabDelay);
                    bz_sendTextMessagef(BZ_SERVER, playerID, "You cannot grab the %s team flag for another ~%.0f seconds", bzu_GetTeamName(team), (safeGrabTime - bz_getCurrentTime()));

                    lastFlagWarnMsg[playerID] = bz_getCurrentTime();
                }
            }
        }
        break;

        case bz_eCaptureEvent:
        {
            bz_CTFCaptureEventData_V1 *data = (bz_CTFCaptureEventData_V1*)eventData;

            lastCapTime[data->teamCapped] = bz_getCurrentTime();

            StringDict placeholders;
            placeholders["{capper}"] = bz_getPlayerCallsign(data->playerCapping);
            placeholders["{teamCapping}"] = bzu_GetTeamName(data->teamCapping);
            placeholders["{teamCapped}"] = bzu_GetTeamName(data->teamCapped);

            // A self-capture
            if (data->teamCapped == bz_getPlayerTeam(data->playerCapping))
            {
                int penalty = SELF_CAP_MULTIPLIER * bz_getTeamCount(data->teamCapped);

                bz_incrementPlayerLosses(data->playerCapping, penalty);

                placeholders["{points}"] = std::to_string(-1 * penalty).c_str();
                placeholders["{pointsAbs}"] = std::to_string(penalty).c_str();

                safeSendMessage(settings.SelfCapturePublicMessage, BZ_ALLUSERS, placeholders);
                safeSendMessage(settings.SelfCapturePrivateMessage, data->playerCapping, placeholders);

                return;
            }

            bool isFair = isFairCapture(data->teamCapping, data->teamCapped);
            int bonusPoints = abs(capBonus[data->teamCapped]);

            if (isFair)
            {
                bz_incrementPlayerWins(data->playerCapping, bonusPoints);

                placeholders["{points}"] = placeholders["{pointsAbs}"] = std::to_string(bonusPoints).c_str();

                safeSendMessage(settings.FairCapturePublicMessage, BZ_ALLUSERS, placeholders);
                safeSendMessage(settings.FairCapturePrivateMessage, data->playerCapping, placeholders);
            }
            else
            {
                bz_incrementPlayerLosses(data->playerCapping, bonusPoints);

                placeholders["{points}"] = std::to_string(-1 * bonusPoints).c_str();
                placeholders["{pointsAbs}"] = std::to_string(bonusPoints).c_str();

                safeSendMessage(settings.UnfairCapturePublicMessage, BZ_ALLUSERS, placeholders);
                safeSendMessage(settings.UnfairCapturePrivateMessage, data->playerCapping, placeholders);
            }
        }
        break;

        case bz_eFlagGrabbedEvent:
        {
            bz_FlagGrabbedEventData_V1 *data = (bz_FlagGrabbedEventData_V1*)eventData;

            bz_eTeamType flagTeam = bzu_getTeamFromFlag(data->flagType);
            bz_eTeamType grabTeam = bz_getPlayerTeam(data->playerID);

            if (eRedTeam <= grabTeam && grabTeam <= ePurpleTeam && flagTeam != grabTeam)
            {
                // Only recalculate the capture bonus if it's been X seconds since the flag was last dropped.
                // This is to prevent players from dropping the flag right before capture and triggering a
                // recalculation.
                bool shouldRecalc = lastFlagDrop[data->flagID] + RECALC_INTERVAL < bz_getCurrentTime();

                // shouldRecalc will be false if the team flag has never been dropped before (the server
                // just started), so allow a calculation on this occassion.
                if (!lastFlagDrop.count(data->flagID) && !shouldRecalc)
                {
                    return;
                }

                int flagTeamSize = bz_getTeamCount(flagTeam);
                int grabTeamSize = bz_getTeamCount(grabTeam);

                int capValue = calcCapturePoints(grabTeam, flagTeam);

                capBonus[flagTeam] = capValue;

                bool sendWarning = bz_getBZDBBool(bzdb_warnUnfairTeams);

                if (sendWarning && capValue < 0 && flagTeamSize > 0)
                {
                    bz_sendTextMessagef(BZ_SERVER, data->playerID, "%d vs %d? Don't be a bad sport.", grabTeamSize, flagTeamSize);
                }
            }
        }
        break;

        case bz_eFlagDroppedEvent:
        {
            bz_FlagDroppedEventData_V1 *data = (bz_FlagDroppedEventData_V1*)eventData;

            bz_eTeamType flagTeam = bzu_getTeamFromFlag(data->flagType);
            bz_eTeamType grabTeam = bz_getPlayerTeam(data->playerID);

            // Only record the time of when an enemy drops the flag
            if (eRedTeam <= flagTeam && flagTeam <= ePurpleTeam && flagTeam != grabTeam)
            {
                lastFlagDrop[data->flagID] = bz_getCurrentTime();
            }
        }
        break;

        default:
            break;
    }
}

bool CTFOverseer::SlashCommand(int playerID, bz_ApiString command, bz_ApiString /*message*/, bz_APIStringList* params)
{
    if (command == "reload" && bz_hasPerm(playerID, "setAll"))
    {
        if (params->size() == 1 && params->get(0) == "ctfoverseer")
        {
            loadConfigurationFile();
            bz_sendTextMessage(BZ_SERVER, playerID, "CTF Overseer reloaded");

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

void CTFOverseer::loadConfigurationFile()
{
    const char* section = "ctfOverseer";
    PluginConfig plgCfg = PluginConfig(configFile);

    if (plgCfg.errors)
    {
        bz_debugMessagef(0, "ERROR :: CTF Overseer :: There was an error reading the configuration file: %s", configFile);
        return;
    }

    settings.SelfCapturePublicMessage = bz_trim(plgCfg.item(section, "self_cap_message_pub").c_str(), "\"");
    settings.SelfCapturePrivateMessage = bz_trim(plgCfg.item(section, "self_cap_message_pm").c_str(), "\"");
    settings.FairCapturePublicMessage = bz_trim(plgCfg.item(section, "fair_cap_message_pub").c_str(), "\"");
    settings.FairCapturePrivateMessage = bz_trim(plgCfg.item(section, "fair_cap_message_pm").c_str(), "\"");
    settings.UnfairCapturePublicMessage = bz_trim(plgCfg.item(section, "unfair_cap_message_pub").c_str(), "\"");
    settings.UnfairCapturePrivateMessage = bz_trim(plgCfg.item(section, "unfair_cap_message_pm").c_str(), "\"");

    bz_debugMessagef(VERBOSE_DEBUG_LEVEL, "DEBUG :: CTF Overseer :: Loaded configuration...");
    bz_debugMessagef(VERBOSE_DEBUG_LEVEL, "DEBUG :: CTF Overseer ::   Self cap public message: %s", settings.SelfCapturePublicMessage.c_str());
    bz_debugMessagef(VERBOSE_DEBUG_LEVEL, "DEBUG :: CTF Overseer ::   Self cap private message: %s", settings.SelfCapturePrivateMessage.c_str());
    bz_debugMessagef(VERBOSE_DEBUG_LEVEL, "DEBUG :: CTF Overseer ::   Fair cap public message: %s", settings.FairCapturePublicMessage.c_str());
    bz_debugMessagef(VERBOSE_DEBUG_LEVEL, "DEBUG :: CTF Overseer ::   Fair cap private message: %s", settings.FairCapturePrivateMessage.c_str());
    bz_debugMessagef(VERBOSE_DEBUG_LEVEL, "DEBUG :: CTF Overseer ::   Unfair cap public message: %s", settings.UnfairCapturePublicMessage.c_str());
    bz_debugMessagef(VERBOSE_DEBUG_LEVEL, "DEBUG :: CTF Overseer ::   Unfair cap private message: %s", settings.UnfairCapturePrivateMessage.c_str());
}

void CTFOverseer::safeSendMessage(const std::string rawMsg, int recipient, StringDict placeholders)
{
    if (rawMsg == "")
    {
        return;
    }

    bz_ApiString msg = rawMsg;
    formatString(msg, placeholders);
    bz_sendTextMessage(BZ_SERVER, recipient, msg.c_str());
}

void CTFOverseer::formatString(bz_ApiString &string, StringDict placeholders)
{
    for (auto it : placeholders)
    {
        string.replaceAll(it.first.c_str(), it.second.c_str());
    }
}

bool CTFOverseer::isFairCapture(bz_eTeamType capping, bz_eTeamType capped)
{
    return calcCapturePoints(capping, capped) > 0;
}

int CTFOverseer::calcCapturePoints(bz_eTeamType capping, bz_eTeamType capped)
{
    int losingTeamSize = bz_getTeamCount(capped);
    int cappingTeamSize = bz_getTeamCount(capping);

    bz_debugMessagef(VERBOSE_DEBUG_LEVEL, "DEBUG :: CTF Overseer :: Calculating bonus points for capping %s flag...", bzu_GetTeamName(capped));
    bz_debugMessagef(VERBOSE_DEBUG_LEVEL, "DEBUG :: CTF Overseer ::   losing team   => %d", losingTeamSize);
    bz_debugMessagef(VERBOSE_DEBUG_LEVEL, "DEBUG :: CTF Overseer ::   grabbing team => %d", cappingTeamSize);

    return (3 * losingTeamSize + 8 * (losingTeamSize - cappingTeamSize));
}
