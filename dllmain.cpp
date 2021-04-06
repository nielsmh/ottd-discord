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

#include <discord_rpc.h>
#include <string>
#include <time.h>

#include "social_plugin_api.h"


const char *DISCORD_CLIENT_ID = "603602960886530062";
const char *MAIN_ICON_ASSET_NAME = "openttd_1024";

extern OpenTTD_SocialPluginCallbacks _callbacks;
bool _inited = false;

DiscordRichPresence _activity{};
ULONGLONG _activity_last_update = 0;
bool _activity_needs_update = false;
bool _activity_in_game = false;

struct OpenJoinRequest {
	ULONGLONG time_opened;
	bool valid;
	const char *user_id;
};
OpenJoinRequest _open_join_requests[15]{};


void Plugin_shutdown()
{
	if (!_inited) return;
	Discord_Shutdown();
	_inited = false;
}

void Plugin_event_loop()
{
	if (!_inited) return;

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
			Discord_UpdatePresence(&_activity);
		} else {
			Discord_ClearPresence();
		}
		_activity_last_update = GetTickCount64();
		_activity_needs_update = false;
	}

	Discord_RunCallbacks();
}

void Plugin_enter_singleplayer()
{
	if (!_inited) return;

	_activity.largeImageKey = MAIN_ICON_ASSET_NAME;
	_activity.details = nullptr;
	_activity.state = nullptr;
	_activity.joinSecret = nullptr;
	_activity.startTimestamp = time(nullptr);
	_activity_needs_update = true;
	_activity_in_game = true;
}

void Plugin_enter_multiplayer(const char *server_name, const char *server_cookie)
{
	if (!_inited) return;

	static char s_server_name[128];
	static char s_server_cookie[128];
	strcpy_s(s_server_name, server_name);
	strcpy_s(s_server_cookie, server_cookie);

	_activity.largeImageKey = MAIN_ICON_ASSET_NAME;
	_activity.details = s_server_name;
	_activity.state = nullptr;
	_activity.joinSecret = s_server_cookie;
	_activity.startTimestamp = time(nullptr);
	_activity_needs_update = true;
	_activity_in_game = true;
}

void Plugin_enter_company(const char *company_name, int company_id)
{
	if (!_inited) return;
	if (!_activity_in_game) return;

	static char s_company_name[128];
	strcpy_s(s_company_name, company_name);

	_activity.state = s_company_name;
	_activity_needs_update = true;
}

void Plugin_enter_spectate()
{
	if (!_inited) return;
	if (!_activity_in_game) return;

	_activity.state = "Spectating";
	_activity_needs_update = true;
}

void Plugin_exit_gameplay()
{
	if (!_inited) return;

	_activity.details = nullptr;
	_activity.state = nullptr;
	_activity.joinSecret = nullptr;
	_activity.startTimestamp = 0;
	_activity_needs_update = true;
	_activity_in_game = true;
}

void Plugin_respond_join_request(void *join_request_cookie, OpenTTD_SocialPluginApi_JoinRequestResponse response)
{
	if (!_inited) return;

	for (auto &ojr : _open_join_requests) {
		if (&ojr == join_request_cookie && ojr.valid) {
			int discord_response = DISCORD_REPLY_IGNORE;
			switch (response) {
				case OTTD_JRR_IGNORE: discord_response = DISCORD_REPLY_IGNORE; break;
				case OTTD_JRR_ACCEPT: discord_response = DISCORD_REPLY_YES; break;
				case OTTD_JRR_REJECT: discord_response = DISCORD_REPLY_NO; break;
			}

			Discord_Respond(ojr.user_id, discord_response);
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
	nullptr, // open browser, not supported by Discord
	nullptr, // get preferred player name, requires authentication
};
OpenTTD_SocialPluginCallbacks _callbacks{};


void Callback_OnActivityJoinGame(const char *joinSecret)
{
	_callbacks.join_requested_game(joinSecret);
}

void Callback_OnActivityJoinRequest(const DiscordUser *request)
{
	/* Find an unused join request slot */
	void *cookie = nullptr;
	for (auto &ojr : _open_join_requests) {
		if (!ojr.valid) {
			cookie = &ojr;
			ojr.time_opened = GetTickCount64();
			ojr.user_id = request->userId;
			break;
		}
	}
	if (!cookie) {
		/* No slot found: Inform Discord we are ignoring this request */
		Discord_Respond(request->userId, DISCORD_REPLY_IGNORE);
		return;
	}

	_callbacks.handle_join_request(cookie, request->username);
}


DiscordEventHandlers _discord_event_handlers = {
	nullptr, // ready
	nullptr, // disconnected
	nullptr, // errored
	nullptr, // joinGame
	nullptr, // spectateGame
	Callback_OnActivityJoinRequest, // joinRequest
};

int SocialInit(int api_version, struct OpenTTD_SocialPluginApi *api, const struct OpenTTD_SocialPluginCallbacks *callbacks)
{
	if (api_version != OTTD_SOCIAL_PLUGIN_API_VERSION) return 0;
	if (_inited) return 2;

	memset(&_activity, 0, sizeof(_activity));
	_activity.largeImageKey = MAIN_ICON_ASSET_NAME;

	memcpy_s(api, sizeof(*api), &_api, sizeof(_api));
	memcpy_s(&_callbacks, sizeof(_callbacks), callbacks, sizeof(*callbacks));

	Discord_Initialize(DISCORD_CLIENT_ID, &_discord_event_handlers, 1, nullptr);
	_inited = true;

	return 1;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	return TRUE;
}
