#include "SDL3/SDL_events.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_pixels.h"
#include "SDL3/SDL_render.h"
#include "SDL3/SDL_surface.h"
#include "SDL3/SDL_video.h"
#include <SDL3_ttf/SDL_ttf.h>
#include <ixwebsocket/IXWebSocketMessage.h>
#include <ixwebsocket/IXWebSocketMessageType.h>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXUserAgent.h>
#include <string>
#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "json.h"
#include <forward_list>
#include <unordered_map>
#include <functional>
#include <iostream>
#include <cstring>
#include <memory>

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static TTF_Font *font = NULL;

static const std::string url = "ws://10.246.12.118:3000/ws";
static ix::WebSocket webSocket;
static std::forward_list<std::string> log;

void logs(std::string msg)
{
	log.push_front(msg);
	SDL_Log("%s", msg.c_str());
}

class Student {
public:
	SDL_Texture* texture;
	std::string name;
	int work;

	Student(std::string name) : name(name), work(0)
	{
		const std::pair<std::string*, SDL_Texture**> data =
			std::pair{&name, &texture};
		SDL_RunOnMainThread([](void* userdata){
			auto& [name, texture] =
				*reinterpret_cast<std::pair<std::string*,
							    SDL_Texture**>*>(userdata);

			auto text = TTF_RenderText_Blended(
				font, name->c_str(), name->size(),
				{255,255,255, SDL_ALPHA_OPAQUE});
			if (text) {
				*texture = SDL_CreateTextureFromSurface(renderer,text);
				SDL_DestroySurface(text);
				if (!(*texture))
					logs(SDL_GetError());
			}
		}, (void*)&data, true);
	}

	~Student()
	{
		if (texture)
			SDL_DestroyTexture(texture);
	}
};

static std::unordered_map<int, std::unique_ptr<Student>> students;

struct JsonWrapper {
	json_value_s* root;

	JsonWrapper(std::string const& str)
	{
		logs("Luotu: " + str);
		root = json_parse(str.c_str(), str.size());
	}

	~JsonWrapper()
	{
		logs("Vapautettu");
		free(root);
	}
};

void onMessage(const ix::WebSocketMessagePtr& msg)
{
	switch (msg->type) {
	case ix::WebSocketMessageType::Message: {
		logs("Msg: " + msg->str);
		const JsonWrapper json(msg->str);
		auto* object = json_value_as_object(json.root);
		if (object == NULL or
		    object->length != 2) {
			return;
		}
		auto* event = object->start;
		if (std::strcmp(event->name->string, "event") != 0) {
			return;
		}
		auto* evalue = json_value_as_string(
			event->value);
		if (std::strcmp(evalue->string, "students:update") != 0) {
			return;
		}
		auto* payload = event->next;
		if (std::strcmp(payload->name->string, "payload") != 0) {
			return;
		}
		// Get students
		auto* values = json_value_as_array(
			payload->value);
		auto* it = values->start;
		while (it != NULL) {
			auto* obj = json_value_as_object(
				it->value);
			// Id
			auto* idv = obj->start;
			if (std::strcmp(idv->name->string, "id") != 0)
				return;
			const int id = std::stoi(
				json_value_as_number(idv->value)->number);
			// Name
			auto* namev = idv->next->next;
			if (std::strcmp(namev->name->string, "name") != 0)
				return;
			const std::string name = json_value_as_string(
				namev->value)->string;
			// Status
			auto* statusv = namev->next;
			if (std::strcmp(statusv->name->string, "status") != 0)
				return;
			const bool status = (json_value_is_true(statusv->value))
				? true : false;
			// Add object
			students.insert({id, std::make_unique<Student>(name)});
			// kikkeli
			it = it->next;
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
	font = TTF_OpenFont("LiberationSans-Regular.ttf", 20);
	if (!font) {
		logs(SDL_GetError());
		return SDL_APP_FAILURE;
	}
	// Connect to a server
	webSocket.setUrl(url);
	logs("Connecting...");
	// Setup a callback to be fired
	// (in a background thread, watch out for race conditions !)
	// when a message or an event (open, close, error) is received
	webSocket.setOnMessageCallback(std::function<void (const ix::WebSocketMessagePtr&)>(onMessage));
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
					std::make_unique<Student>("Jääskän poika")});
			break;
		}
		}
	}
	return SDL_APP_CONTINUE;
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
	// Clear
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);
	// Debug log
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
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
	{
		const float width = screenWidth / 5;
		const float height = width / 2;
		const int nStudents = students.size();
		const float tHeight = SDL_ceil(nStudents / 3.f) * height * 1.2f;
		int i = 0;
		for (auto const& [id, data] :
			     students) {
			const SDL_FRect dstrect = {
				(width*0.8f) + ((i%3) * width * 1.2f),
				((screenHeight - tHeight) / 2) +
				(static_cast<float>(SDL_floor(i/3.f))
				 * height * 1.2f),
				width, height
			};
			SDL_RenderTexture(renderer, data->texture,
					  nullptr, &dstrect);
			i++;
		}
	}
	// Present
	SDL_RenderPresent(renderer);
	return SDL_APP_CONTINUE;
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
	if (font)
		TTF_CloseFont(font);
	TTF_Quit();
	webSocket.close();
	webSocket.stop();
}
