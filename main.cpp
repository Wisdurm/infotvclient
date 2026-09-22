#include "SDL3/SDL_events.h"
#include "SDL3/SDL_video.h"
#include <ixwebsocket/IXWebSocketMessage.h>
#include <ixwebsocket/IXWebSocketMessageType.h>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXUserAgent.h>
#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "json.h"
#include <forward_list>
#include <unordered_map>
#include <functional>
#include <iostream>
#include <cstring>

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

static const std::string url = "ws://10.246.12.118:3000/ws";
static ix::WebSocket webSocket;
static std::forward_list<std::string> log;

using Work = int;

static std::unordered_map<std::string, Work> students;

void logs(std::string msg)
{
	log.push_front(msg);
	SDL_Log("%s", msg.c_str());
}

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
		auto* students = json_value_as_array(
			payload->value);
		auto* it = students->start;
		while (it != NULL) {
			auto* obj = json_value_as_object(
				it->value);
			auto* name = obj->start->next->next;
			if (std::strcmp(name->name->string, "name") != 0) {
				return;
			}
			const std::string str = json_value_as_string(
				name->value)->string;
			logs("NAME!!: " + str);
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
		SDL_Log("Couldn't create window and renderer: %s", SDL_GetError());
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
	students.emplace("Jaakko", 12);
	students.emplace("Jorma", 1);
	students.emplace("Asko", 12);
	students.emplace("Jerma", 1);
	students.emplace("Veeti", 12);
	students.emplace("Jouko", 1);

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
			std::string n = students.begin()->first;
			n += static_cast<char>(event->key.raw);
			students.emplace(n, 12);
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
		for (auto const& [name, work] :
			     students) {
			const SDL_FRect rect = {
				(width*0.8f) + ((i%3) * width * 1.2f),
				((screenHeight - tHeight) / 2) +
				(static_cast<float>(SDL_floor(i/3.f))
				 * height * 1.2f),
				width, height
			};
			SDL_RenderRect(renderer, &rect);
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
	webSocket.close();
	webSocket.stop();
}
