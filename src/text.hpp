#include "SDL3/SDL_assert.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_render.h"
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <functional>
#include <string>
#include <unordered_map>
#include <chrono>
#include <vector>

inline TTF_Font *font = NULL;

class TextWrapper
{
private:
	using CacheType = std::unordered_map<std::string, std::pair<SDL_Texture*,
			  std::chrono::time_point<std::chrono::system_clock>>>;
	inline static CacheType cache;
	const std::string text;
	const SDL_Color colour;

public:
	inline static SDL_Renderer* renderer;

	TextWrapper(std::string text, SDL_Color colour) : text(text), colour(colour)
	{}

	void GenerateTexture() const
	{
		SDL_assert_always(SDL_IsMainThread());
		// If string already rendered
		if (TextWrapper::cache.find(text) != TextWrapper::cache.end())
			return;
		// Otherwise
		SDL_Texture* texture;
		auto ts = TTF_RenderText_Blended(
			font, text.c_str(), text.size(), colour);
		if (ts) {
			texture = SDL_CreateTextureFromSurface(renderer,ts);
			SDL_DestroySurface(ts);
			if (!texture)
				throw;
		}
		TextWrapper::cache.insert(
			{text,
			 {texture, std::chrono::system_clock::now()}});
	}

	SDL_Texture* GetTexture() const
	{
		if (TextWrapper::cache.find(text) == TextWrapper::cache.end())
			GenerateTexture();
		auto& pair = TextWrapper::cache.at(text);
		pair.second = std::chrono::system_clock::now();
		return pair.first;
	}

	// Periodic cleanup of unused textures
	static void Cleanup()
	{
		SDL_assert_always(SDL_IsMainThread());
		CacheType::iterator it = cache.begin();
		while (it != cache.end()) {
			if (std::chrono::system_clock::now() - it->second.second
			    > std::chrono::seconds(3)) {
				SDL_DestroyTexture(it->second.first);
				it = cache.erase(it);
			} else {
				++it;
			}
		}
	}

	static void ClearCache()
	{
		for (auto& [_,texture] :
			     TextWrapper::cache) {
			SDL_DestroyTexture(texture.first);
		}
		TextWrapper::cache.clear();
	}
};
