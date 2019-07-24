Discord plugin for OpenTTD social presence API

This plugin is for a (currently) experimental branch of OpenTTD that supports social presence plugins.
The plugin will connect to Discord if the client is locally installed and report game status.

Compiling:
- Only tested with Visual Studio 2019.
- Discord Game SDK requires a separate download from Discord's developer portal: <https://discordapp.com/developers/docs/>
- Create a folder named "discord", place the .cpp and .h files from the SDK in that,
  and move the x86 and x86_64 folders from the SDK's "lib" folder into it.

Installation:
- Place the ottd-discord.dll file somewhere, your OpenTTD folder is fine
- The discord_game_sdk.dll file must be placed in the same folder as openttd.exe
- Register the plugin in your registry:
  HKEY_CURRENT_USER\Software\OpenTTD
    "SocialPresencePlugin" REG_SZ = <full path to ottd-discord.dll>
