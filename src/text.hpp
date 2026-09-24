#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <string>
#include <unordered_map>

inline TTF_Font *font = NULL;

class TextWrapper
{
	inline static std::unordered_map<std::string, SDL_Texture*> cache;
	const std::string text;
	const SDL_Color colour;

public:
	inline static SDL_Renderer* renderer;

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
					throw;
			}
		}, (void*)&data, true);
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
