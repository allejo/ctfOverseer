# CTF Overseer

[![GitHub release](https://img.shields.io/github/release/allejo/ctfOverseer.svg)](https://github.com/allejo/ctfOverseer/releases/latest)
![Minimum BZFlag Version](https://img.shields.io/badge/BZFlag-v2.4.20+-blue.svg)
[![License](https://img.shields.io/github/license/allejo/ctfOverseer.svg)](LICENSE.md)

The plug-in that implements all of the CTF-related features from Planet MoFo's Apocalypse map. This plug-in has a number of features relating to capture events such as,

- Disable self-captures
- Disable a team flag from being grabbed X seconds after it was capped (prevent "pass-camping")
- Award bonus points to the player who capped the flag based on team counts
- Announce custom messages on capture events

## Requirements

- BZFlag 2.4.20+
- C++11

This plug-in follows [my standard instructions for compiling plug-ins](https://github.com/allejo/docs.allejo.io/wiki/BZFlag-Plug-in-Distribution).

## Usage

### Loading the plug-in

Loading the plugins requires [a configuration file](ctfOverseer.cfg) to define templated messages sent to players on certain events.

```
-loadplugin ctfOverseer,ctfOverseer.cfg
```

### Configuration File

This plug-in makes use of the `ctfOverseer` section. Each configuration value may use quotes and will have them automatically stripped out.

- `self_cap_message_pub` - The message sent to all players on a self-capture
- `self_cap_message_pm` - The message sent to the player who self-captured
- `fair_cap_message_pub` - The message sent to all players on a capture while teams were fair
- `fair_cap_message_pm` - The message sent to the player who captured the flag when teams were fair
- `unfair_cap_message_pub` - The message sent to all players on a capture while teams were _unfair_
- `unfair_cap_message_pm` - The message sent to the player who captured the flag when teams were unfair

The following placeholders are available for use in any of the above settings.

- `{capper}` - The callsign of the player capping
- `{teamCapping}` - The color of the team capping the flag
- `{teamCapped}` - The color of the team whose flag was capped
- `{points}` - The number of bonus points awarded to the capper; this number may be negative when it's a penalty on self or unfair caps
- `{pointsAbs}` - The absolute value of the points awarded

### Custom BZDB Variables

These custom BZDB variables can be configured with `-set` in configuration files and may be changed at any time in-game by using the `/set` command.

```
-set <name> <value>
```

| Name | Type | Default | Description |
| ---- | ---- | ------- | ----------- |
| `_delayTeamFlagGrab` | int | 20 | The number of seconds after a team flag is captured that the team flag is ungrabbable by enemy players |
| `_maxCapBonus` | int | 9999 | The maximum number of points that can be granted per cap |
| `_disallowSelfCap` | bool | true | Disallow players from capturing their own flag |
| `_disallowUnfairCap` | bool | false | Disallow an unfair flag capture and send the team flag to its nearest safety zone |
| `_warnUnfairTeams` | bool | true | Send a PM to players to warn them they're grabbing an enemy team flag while teams are unfair |

### Custom Slash Commands

This plug-in implements custom slash commands only for administrative tasks.

| Command | Permission | Description |
| ------- | ---------- | ----------- |
| `/reload [ctfoverseer]` | setAll | Re-read the configuration file to load in new messages |

### Inter-Plug-in Communication

This plug-in supports using generic callbacks for inter-plug-in communication. Since this plug-in uses semantic versioning in its name, accessing this plugin via a generic callback is not feasible. For this reason, the plug-in registers a clip field under the name of `allejo/ctfOverseer`.

```cpp
std::string ctfOverseer = bz_getclipFieldString("allejo/ctfOverseer");

std::pair<bz_eTeamType, bz_eTeamType> teamPair = std::make_pair(eRedTeam, eGreenTeam);
void* data = teamPair;
int response = bz_callPluginGenericCallback(ctfOverseer.c_str(), "calcBonusPoints", data);
```

| Callback Name | Expected Type | Return Type |
| ------------- | ------------- | ----------- |
| `calcBonusPoints` | `std::pair<bz_eTeamType, bz_eTeamType>` | The amount of points that would be awarded at the given moment for a capture |
| `isFairCapture` | `std::pair<bz_eTeamType, bz_eTeamType>` | A boolean value casted into an int |

#### Notes

- The value of `-9999` will be returned in the case of an error
- The first value of the `std::pair` will be the team who is grabbing the enemy flag, the second value is the team whose flag was grabbed

#### Warning

- Passing an object of the incorrect type will lead to unexpected behavior (and possibly server crashes?)

## License

[MIT](LICENSE.md)
