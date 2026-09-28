#include <SDL3_ttf/SDL_ttf.h>
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_oldnames.h"
#include "SDL3/SDL_pixels.h"
#include "SDL3/SDL_render.h"
#include "SDL3/SDL_stdinc.h"
#include "SDL3/SDL_surface.h"
#include "SDL3/SDL_video.h"
#include <algorithm>
#include <cstddef>
#include <ixwebsocket/IXWebSocketMessage.h>
#include <ixwebsocket/IXWebSocketMessageType.h>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXUserAgent.h>
#include <stdexcept>
#include <string>
#include <vector>
#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <forward_list>
#include <unordered_map>
#include <cstring>
#include <memory>
#include <mutex>
#include <format>
#include <chrono>
#include "json.hpp"
#include "text.hpp"

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

static SDL_Texture* renderBuffer;

static SDL_Texture* blueBorder;
static SDL_Texture* greyBorder;

static std::mutex mutex;

static const std::string url = "ws://10.246.12.118:3000/ws";
static ix::WebSocket webSocket;
static std::forward_list<std::string> log;

void logs(std::string msg)
{
	log.push_front(msg);
	SDL_Log("%s", msg.c_str());
}

struct Student {
	const TextWrapper nameTexture;
	const TextWrapper remainingTexture;
	const std::string name;
	const bool status;

	Student(std::string name, int remaining, bool status) :
		name(name),
		nameTexture(TextWrapper(name, {0,0,0,SDL_ALPHA_OPAQUE})),
		remainingTexture([&remaining]{
			auto time {std::chrono::seconds(remaining)};
			return std::format("{:%H t %M min }", time);
		}(), {100,100,100,SDL_ALPHA_OPAQUE}),
		status(status)
	{
	}
};

static std::unordered_map<int, std::unique_ptr<Student>> students;


void UpdateStudent(JsonValueWrapper value)
{
	const auto obj = JsonObjectWrapper(value);
	// Id
	auto idv = obj[0];
	if (idv.name() != "id")
		return;
	const int id = int(JsonValueWrapper(idv.value()));
	// Name
	const auto namev = obj[2];
	if (namev.name() != "name")
		return;
	const std::string name = std::string(JsonValueWrapper(namev.value()));
	// Status
	const auto statusv = obj[3];
	if (statusv.name() != "status")
		return;
	const bool status =
		std::string(JsonValueWrapper(statusv.value())) == "IN"
		? true : false;
	// Remaining time
	const auto remainingv = obj[10];
	if (remainingv.name() != "remaining")
		return;
	const int remaining = int(JsonValueWrapper(remainingv.value()));
	// Add object
	students.insert({id, std::make_unique<Student>(name, remaining, status)});
}

void onMessage(const ix::WebSocketMessagePtr& msg)
{
	switch (msg->type) {
	case ix::WebSocketMessageType::Message: {
		logs("Msg: " + msg->str);
		// TODO: erorr hand
		const JsonWrapper json(msg->str);
		const auto object = json.object();
		const auto event = object[0];
		if (event.name() != "event")
			return;
		const auto payload = object[1];
		if (payload.name() != "payload")
			return;
		const std::string eventName =
			std::string(JsonValueWrapper(event.value()));
		if (eventName == "students:update") {
			// Get students
			const auto values =
				std::vector<JsonValueWrapper>(JsonValueWrapper(payload.value()));
			// Wait until rendered before modifying it
			std::lock_guard<std::mutex> _(mutex);
			students.clear();
			for (auto value : values) {
				UpdateStudent(value);
			}
		} else if (eventName == "student:update") {
			std::lock_guard<std::mutex> _(mutex);
			UpdateStudent(JsonValueWrapper(payload.value()));
		} else if (eventName == "student:new") {
			std::lock_guard<std::mutex> _(mutex);
			UpdateStudent(JsonValueWrapper(payload.value()));
		}
		logs("Parsed succesfully!");
		break;
	}
	case ix::WebSocketMessageType::Open: {
		logs("Connected");
		break;
	}
	case ix::WebSocketMessageType::Error: {
		// Maybe SSL is not configured properly
		logs("Connection error: " + msg->errorInfo.reason);
		break;
	}
	default:
		logs("Who knows!");
		break;
	}
}

SDL_Texture* LoadBMP(std::string file)
{
	auto sur = SDL_LoadBMP(file.c_str());
	auto tex= SDL_CreateTextureFromSurface(renderer, sur);
	SDL_DestroySurface(sur);
	return tex;
}

std::tuple<int, int> GetScreenSize()
{
	auto arr = SDL_GetDisplays(nullptr);
	if (not arr)
		throw;
	const SDL_DisplayMode* dm = SDL_GetCurrentDisplayMode(arr[0]);
	SDL_free(arr);
	return {dm->w, dm->h};
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
	if (!SDL_CreateWindowAndRenderer(
		    "Hello World", 800, 600,
		    SDL_WINDOW_RESIZABLE, &window, &renderer)) {
		logs(SDL_GetError());
		return SDL_APP_FAILURE;
	}
	const auto [width, height] = GetScreenSize();
	SDL_SetWindowSize(window, width, height);
	TextWrapper::renderer = renderer;
	if (!TTF_Init()) {
		logs(SDL_GetError());
		return SDL_APP_FAILURE;
	}
	font = TTF_OpenFont("LiberationSans-Regular.ttf", 35);
	if (!font) {
		logs(SDL_GetError());
		return SDL_APP_FAILURE;
	}
	blueBorder = LoadBMP("blue.bmp");
	greyBorder = LoadBMP("gray.bmp");
	// Connect to a server
	//webSocket.setUrl(url);
	logs("Connecting...");
	// Setup a callback to be fired
	// (in a background thread, watch out for race conditions !)
	// when a message or an event (open, close, error) is received
	webSocket.setOnMessageCallback(onMessage);
	// Start thread
	webSocket.start();
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
	// Send a message to the server (default to TEXT mode)
	//webSocket.send("hello world");
	if (event->type == SDL_EVENT_QUIT) {
		return SDL_APP_SUCCESS;
	} else if (event->type == SDL_EVENT_KEY_DOWN) {
		switch (event->key.key) {
		case SDLK_D: {
			students.erase(students.begin());
			break;
		}
		default: {
			students.insert(
				{event->key.raw, std::make_unique<Student>
				 ("Jääskän poika", 2, true)});
			break;
		}
		}
	} else if (event->type == SDL_EVENT_WINDOW_RESIZED) {
		int w, h;
		SDL_GetWindowSize(SDL_GetWindowFromEvent(event), &w, &h);
		if (renderBuffer)
			SDL_DestroyTexture(renderBuffer);
		renderBuffer =
			SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888,
					  SDL_TEXTUREACCESS_TARGET, w, h);
	}
	return SDL_APP_CONTINUE;
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
	if (not renderBuffer)
		return SDL_APP_CONTINUE;
	// Don't modify students while rendering it
	if (mutex.try_lock()) {
		SDL_SetRenderTarget(renderer, renderBuffer);
		// Background
		SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
		SDL_RenderClear(renderer);
		// Debug log
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		{
			int i = 0;
			for (auto const&  msg : log) {
				SDL_RenderDebugText(renderer, 0, i*10, msg.c_str());
				i++;
			}
		}
		// Students
		int screenWidth, screenHeight;
		SDL_GetCurrentRenderOutputSize(renderer, &screenWidth, &screenHeight);

		const auto get = [&](const auto &self, int perRow) {
			const float width = screenWidth / (perRow + 2);
			const float height = width / 2;
			const int nStudents = students.size();
			const float tHeight = SDL_ceil(nStudents / float(perRow)) * height * 1.2f;
			const float tWidth = (perRow * width * 1.2f)
				- (width * .2f);
			if (tHeight < screenHeight*0.9 or perRow >= 4)
				return std::tuple{perRow, width, height, tHeight, tWidth};
			else
				return self(self, perRow+1);
		};
		const auto [perRow, width, height, tHeight, tWidth] = get(get,3);
		// moi
		const float x = SDL_fmod(SDL_GetTicks() / 1000., 8.f);
		const float f = ((x < 2) ? 0 :
				 (x < 4) ? SDL_sin((x-2)*(SDL_PI_F / 4)) :
				 (x < 6) ? 1 :
				 (x < 8) ? 1-SDL_sin((x+2)*(SDL_PI_F / 4)) :
				 0) - 0.5;
		const float scroll = f * SDL_max((tHeight - screenHeight) * 1.1,0);

		int i = 0;
		for (auto const& [id, data] :
			     students) {
			const float startX = (screenWidth - tWidth) / 2;
			const SDL_FRect borderRect = {
				startX + ((i%perRow) * width * 1.2f),
				((screenHeight - tHeight) / 2) +
				(static_cast<float>(SDL_floor(i/float(perRow)))
				 * height * 1.2f) + scroll,
				width, height
			};
			const auto nameTex = data->nameTexture.GetTexture();
			const auto timeTex = data->remainingTexture.GetTexture();
			// Border
			if (data->status)
				SDL_RenderTexture9Grid(renderer, blueBorder, nullptr,
						       12,12,12,12, 0,
						       &borderRect);
			else
				SDL_RenderTexture9Grid(renderer, greyBorder, nullptr,
						       12,12,12,12, 0,
						       &borderRect);

			// Text
			const float fontScale = screenWidth / 1600.f;
			const float timeScale = fontScale * 0.8f;
			const float textHeight = (nameTex->h * fontScale) +
				(timeTex->h * timeScale);
			// Name
			const SDL_FRect nameRect = {
				borderRect.x
				+ ((borderRect.w - (nameTex->w*fontScale)) / 2),
				borderRect.y
				+ ((borderRect.h - textHeight) / 2),
				static_cast<float>(nameTex->w*fontScale),
				static_cast<float>(nameTex->h*fontScale)
			};
			SDL_RenderTexture(renderer, nameTex,
					  nullptr, &nameRect);
			// Time
			const SDL_FRect timeRect = {
				nameRect.x,
				nameRect.y + nameRect.h,
				static_cast<float>(timeTex->w*timeScale),
				static_cast<float>(timeTex->h*timeScale)
			};
			SDL_RenderTexture(renderer, timeTex,
					  nullptr, &timeRect);
			i++;
		}
		mutex.unlock();
		SDL_SetRenderTarget(renderer, nullptr);
	}
	// Present
	SDL_RenderTexture(renderer, renderBuffer, nullptr, nullptr);
	SDL_RenderPresent(renderer);
	return SDL_APP_CONTINUE;
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
	SDL_DestroyTexture(blueBorder);
	TextWrapper::CleanCache();
	if (font)
		TTF_CloseFont(font);
	TTF_Quit();
	webSocket.close();
	webSocket.stop();
}
