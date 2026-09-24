#include "SDL3/SDL_events.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_oldnames.h"
#include "SDL3/SDL_pixels.h"
#include "SDL3/SDL_render.h"
#include "SDL3/SDL_surface.h"
#include "SDL3/SDL_video.h"
#include <SDL3_ttf/SDL_ttf.h>
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

#include <iostream>

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static TTF_Font *font = NULL;

static SDL_Texture* buffer;

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

class TextWrapper
{
	inline static std::unordered_map<std::string, SDL_Texture*> cache;
	const std::string text;
	const SDL_Color colour;

public:
	TextWrapper(std::string text, SDL_Color colour) : text(text), colour(colour)
	{
		GenerateTexture();
	}

	void GenerateTexture() const
	{
		// If string already rendered
		if (TextWrapper::cache.find(text) != TextWrapper::cache.end())
			return;
		// Otherwise
		SDL_Texture* texture;
		const std::tuple<std::string const*, SDL_Texture**, SDL_Color const*>
			data = std::tuple{&text, &texture, &colour};
		SDL_RunOnMainThread([](void* userdata){
			auto& [msg,
			       texture,
			       colour] =
				*reinterpret_cast<std::tuple<std::string const*,
							     SDL_Texture**,
							     SDL_Color*>*>(userdata);

			auto text = TTF_RenderText_Blended(
				font, msg->c_str(), msg->size(), *colour);
			if (text) {
				*texture = SDL_CreateTextureFromSurface(renderer,text);
				SDL_DestroySurface(text);
				if (!(*texture))
					logs(SDL_GetError());
			}
		}, (void*)&data, true);
		logs("Rendered");
		TextWrapper::cache.insert({text, texture});
	}

	SDL_Texture* GetTexture() const
	{
		if (TextWrapper::cache.find(text) == TextWrapper::cache.end())
			GenerateTexture();
		return TextWrapper::cache.at(text);
	}

	// Periodic cleanup of unused textures
	static void Cleanup()
	{
		// TODO
	}

	static void CleanCache()
	{
		SDL_RunOnMainThread([](void* userdata){
			auto& cache = *reinterpret_cast<
				std::unordered_map<std::string, SDL_Texture*>*
				>(userdata);
			for (auto& [_,texture] :
				     TextWrapper::cache) {
				SDL_DestroyTexture(texture);
			}
		} , &TextWrapper::cache, true);
		TextWrapper::cache.clear();
	}
};

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

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
	auto _ = JsonWrapper("{\"kisu\":2}");
	if (!SDL_CreateWindowAndRenderer(
		    "Hello World", 800, 600,
		    SDL_WINDOW_RESIZABLE, &window, &renderer)) {
		logs(SDL_GetError());
		return SDL_APP_FAILURE;
	}
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
	webSocket.setUrl(url);
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
			students.insert({event->key.raw,
					std::make_unique<Student>
					("Jääskän poika", 2, true)});
			break;
		}
		}
	}
	return SDL_APP_CONTINUE;
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
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
	const float width = screenWidth / 5;
	const float height = width / 2;
	const int nStudents = students.size();
	const float tHeight = SDL_ceil(nStudents / 3.f) * height * 1.2f;
	int i = 0;
	// Don't modify students while rendering it
	if (mutex.try_lock()) {
		for (auto const& [id, data] :
			     students) {
			const SDL_FRect borderRect = {
				(width*0.8f) + ((i%3) * width * 1.2f),
				((screenHeight - tHeight) / 2) +
				(static_cast<float>(SDL_floor(i/3.f))
				 * height * 1.2f),
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
	} else {
		SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
		SDL_RenderClear(renderer);
	}
	// Present
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
