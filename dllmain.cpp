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
#include <string>
#include <time.h>

#include "social_plugin_api.h"


const discord::ClientId DISCORD_CLIENT_ID = 603602960886530062;
const char *MAIN_ICON_ASSET_NAME = "openttd_512";

extern OpenTTD_SocialPluginCallbacks _callbacks;
discord::Core *_discord = nullptr;

discord::Activity _activity{};
ULONGLONG _activity_last_update = 0;
bool _activity_needs_update = false;
bool _activity_in_game = false;

struct OpenJoinRequest {
	ULONGLONG time_opened;
	bool valid;
	discord::UserId user_id;
};
OpenJoinRequest _open_join_requests[15]{};

void Callback_IgnoreResult(discord::Result) { }


void Plugin_shutdown()
{
	delete _discord;
	_discord = nullptr;
}

void Plugin_event_loop()
{
	if (!_discord) return;

	for (auto &ojr : _open_join_requests) {
		if (!ojr.valid) continue;
		auto age = GetTickCount64() - ojr.time_opened;
		if (age > 10 * 60 * 1000) {
			ojr.valid = false;
			_callbacks.cancel_join_request(&ojr);
		}
	}

	if (_activity_needs_update && GetTickCount64() - _activity_last_update > 10000) {
		if (_activity_in_game) {
			_discord->ActivityManager().UpdateActivity(_activity, Callback_IgnoreResult);
		} else {
			_discord->ActivityManager().ClearActivity(Callback_IgnoreResult);
		}
		_activity_last_update = GetTickCount64();
		_activity_needs_update = false;
	}

	if (_discord->RunCallbacks() != discord::Result::Ok) Plugin_shutdown();
}

void Plugin_enter_singleplayer()
{
	if (!_discord) return;

	_activity.GetAssets().SetLargeImage(MAIN_ICON_ASSET_NAME);
	_activity.SetDetails("");
	_activity.SetType(discord::ActivityType::Playing);
	_activity.GetSecrets().SetJoin("");
	_activity.GetTimestamps().SetStart(time(nullptr));
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
	_activity.GetTimestamps().SetStart(time(nullptr));
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

	_activity.GetTimestamps().SetStart(0);
	_activity_needs_update = true;
	_activity_in_game = false;
}

void Plugin_respond_join_request(void *join_request_cookie, OpenTTD_SocialPluginApi_JoinRequestResponse response)
{
	if (!_discord) return;

	for (auto &ojr : _open_join_requests) {
		if (&ojr == join_request_cookie && ojr.valid) {
			discord::ActivityJoinRequestReply discord_response = discord::ActivityJoinRequestReply::Ignore;
			switch (response) {
				case OTTD_JRR_IGNORE: discord_response = discord::ActivityJoinRequestReply::Ignore; break;
				case OTTD_JRR_ACCEPT: discord_response = discord::ActivityJoinRequestReply::Yes; break;
				case OTTD_JRR_REJECT: discord_response = discord::ActivityJoinRequestReply::No; break;
			}

			_discord->ActivityManager().SendRequestReply(ojr.user_id, discord_response, Callback_IgnoreResult);
			ojr.valid = false;
			break;
		}
	}
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


void Callback_OnActivityJoinRequest(const discord::User &user)
{
	/* Find an unused join request slot */
	void *cookie = nullptr;
	for (auto &ojr : _open_join_requests) {
		if (!ojr.valid) {
			cookie = &ojr;
			ojr.time_opened = GetTickCount64();
			ojr.user_id = user.GetId();
			break;
		}
	}
	if (!cookie) {
		/* No slot found: Inform Discord we are ignoring this request */
		_discord->ActivityManager().SendRequestReply(user.GetId(), discord::ActivityJoinRequestReply::Ignore, Callback_IgnoreResult);
		return;
	}

	_callbacks.handle_join_request(cookie, user.GetUsername());
}


int SocialInit(int api_version, struct OpenTTD_SocialPluginApi *api, const struct OpenTTD_SocialPluginCallbacks *callbacks)
{
	if (api_version != OTTD_SOCIAL_PLUGIN_API_VERSION) return 0;
	if (_discord) return 0;

	memcpy_s(api, sizeof(*api), &_api, sizeof(_api));
	memcpy_s(&_callbacks, sizeof(_callbacks), callbacks, sizeof(*callbacks));

	auto res = discord::Core::Create(DISCORD_CLIENT_ID, DiscordCreateFlags_NoRequireDiscord, &_discord);
	if (res == discord::Result::Ok) {
		_activity.GetAssets().SetLargeImage(MAIN_ICON_ASSET_NAME);
		_activity.SetType(discord::ActivityType::Playing);
		_discord->ActivityManager().UpdateActivity(_activity, Callback_IgnoreResult);
		_activity_last_update = GetTickCount64();
		_activity_needs_update = false;
		_activity_in_game = false;
		_discord->ActivityManager().ClearActivity(Callback_IgnoreResult);

		_discord->ActivityManager().OnActivityJoinRequest.Connect(Callback_OnActivityJoinRequest);
		_discord->ActivityManager().OnActivityJoin.Connect(_callbacks.join_requested_game);

		_discord->ActivityManager().RegisterCommand(callbacks->launch_command);
		_callbacks.launch_command = nullptr; // not our pointer to own

		return 1;
	} else {
		// TODO: maybe some logging
		return 0;
	}
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	return TRUE;
}
