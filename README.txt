Discord plugin for OpenTTD social presence API

This plugin is for a (currently) experimental branch of OpenTTD that supports social presence plugins.
The plugin will connect to Discord if the client is locally installed and report game status.

Compiling:
- Only tested with Visual Studio 2019.
- This branch has the (officially deprecated) Discord RPC SDK imported into the source tree.
  It does not have any additional dependencies.

Installation:
- Place the ottd-discord.dll file somewhere, your OpenTTD folder is fine
- Register the plugin in your registry:
  HKEY_CURRENT_USER\Software\OpenTTD
    "SocialPresencePlugin" REG_SZ = <full path to ottd-discord.dll>

Installation procedure might change for OpenTTD, to not use a registry-based discovery method.
