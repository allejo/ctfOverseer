# CTF Overseer

[![GitHub release](https://img.shields.io/github/release/allejo/ctfOverseer.svg)](https://github.com/allejo/ctfOverseer/releases/latest)
![Minimum BZFlag Version](https://img.shields.io/badge/BZFlag-v2.4.18+-blue.svg)
[![License](https://img.shields.io/github/license/allejo/ctfOverseer.svg)](LICENSE.md)

A brief description about what the plugin does should go here

## Requirements

- BZFlag 2.4.18
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
| `_disallowSelfCap` | bool | true | Disallow players from capturing their own flag |
| `_warnUnfairTeams` | bool | true | Send a PM to players to warn them they're grabbing an enemy team flag while teams are unfair |

### Custom Slash Commands

| Command | Permission | Description |
| ------- | ---------- | ----------- |
| `/reload [ctfoverseer]` | setAll | Re-read the configuration file to load in new messages |

## License

[MIT](LICENSE.md)
