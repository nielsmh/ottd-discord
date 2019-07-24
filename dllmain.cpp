/*
 * Copyright 2019 Niels Martin Hansen
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "discord/discord.h"

#include "social_plugin_api.h"


const discord::ClientId DISCORD_CLIENT_ID = 603602960886530062;
const char *MAIN_ICON_ASSET_NAME = "openttd_512";

discord::Core *_discord = nullptr;

discord::Activity _activity{};
DWORD _activity_last_update = 0;
bool _activity_needs_update = false;
bool _activity_in_game = false;


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

void Plugin_shutdown()
{
	delete _discord;
	_discord = nullptr;
}

void Plugin_event_loop()
{
	if (!_discord) return;

	if (_activity_needs_update && GetTickCount() - _activity_last_update > 10000) {
		if (_activity_in_game) {
			_discord->ActivityManager().UpdateActivity(_activity, [](discord::Result result) {});
		} else {
			_discord->ActivityManager().ClearActivity([](discord::Result result) {});
		}
		_activity_last_update = GetTickCount();
		_activity_needs_update = false;
	}

	if (_discord->RunCallbacks() != discord::Result::Ok) Plugin_shutdown();
}

void Plugin_enter_singleplayer()
{
	if (!_discord) return;

	_activity.GetAssets().SetLargeImage(MAIN_ICON_ASSET_NAME);
	_activity.SetDetails("Singleplayer");
	_activity.SetType(discord::ActivityType::Playing);
	_activity.GetSecrets().SetJoin("");
	_activity_needs_update = true;
	_activity_in_game = true;
}

void Plugin_enter_multiplayer(const char *server_name, const char *server_cookie)
{
	if (!_discord) return;

	_activity.GetAssets().SetLargeImage(MAIN_ICON_ASSET_NAME);
	_activity.SetDetails(server_name);
	_activity.SetType(discord::ActivityType::Playing);
	_activity.GetSecrets().SetJoin(server_cookie);
	_activity_needs_update = true;
	_activity_in_game = true;
}

void Plugin_enter_company(const char *company_name, int company_id)
{
	if (!_discord) return;
	if (!_activity_in_game) return;

	_activity.SetType(discord::ActivityType::Playing);
	_activity.SetState(company_name);
	_activity_needs_update = true;
}

void Plugin_enter_spectate()
{
	if (!_discord) return;
	if (!_activity_in_game) return;

	_activity.SetType(discord::ActivityType::Watching);
	_activity.SetState("Spectating");
	_activity_needs_update = true;
}

void Plugin_exit_gameplay()
{
	if (!_discord) return;

	_activity_needs_update = true;
	_activity_in_game = false;
}

void Plugin_respond_join_request(void *join_request_cookie, OpenTTD_SocialPluginApi_JoinRequestResponse response)
{
	if (!_discord) return;

	// TODO
}


OpenTTD_SocialPluginApi _api{
	Plugin_shutdown,
	Plugin_event_loop,
	Plugin_enter_singleplayer,
	Plugin_enter_multiplayer,
	Plugin_enter_company,
	Plugin_enter_spectate,
	Plugin_exit_gameplay,
	Plugin_respond_join_request,
};
OpenTTD_SocialPluginCallbacks _callbacks{};

int SocialInit(int api_version, struct OpenTTD_SocialPluginApi *api, const struct OpenTTD_SocialPluginCallbacks *callbacks)
{
	if (api_version != OTTD_SOCIAL_PLUGIN_API_VERSION) return 0;
	if (_discord) return 0;

	memcpy_s(api, sizeof(*api), &_api, sizeof(_api));
	memcpy_s(&_callbacks, sizeof(_callbacks), callbacks, sizeof(*callbacks));

	auto res = discord::Core::Create(DISCORD_CLIENT_ID, DiscordCreateFlags_NoRequireDiscord, &_discord);
	if (res == discord::Result::Ok) {
		return 1;
	} else {
		// TODO: maybe some logging
		return 0;
	}
}
