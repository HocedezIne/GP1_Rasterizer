#include "Texture.h"
#include "Vector2.h"
#include <SDL_image.h>

namespace dae
{
	Texture::Texture(SDL_Surface* pSurface) :
		m_pSurface{ pSurface },
		m_pSurfacePixels{ (uint32_t*)pSurface->pixels }
	{
	}

	Texture::~Texture()
	{
		if (m_pSurface)
		{
			SDL_FreeSurface(m_pSurface);
			m_pSurface = nullptr;
		}
	}

	Texture* Texture::LoadFromFile(const std::string& path)
	{
		SDL_Surface* surfacePtr{ IMG_Load(path.data()) };
		if (!surfacePtr)
		{
			return nullptr;
		}

		return new Texture{surfacePtr};
	}

	ColorRGB Texture::Sample(const Vector2& uv) const
	{
		//TODO
		//Sample the correct texel for the given uv
		const uint16_t pixelX = uv.x * m_pSurface->w;
		const uint16_t pixelY = uv.y * m_pSurface->h;
		const int pixel{pixelX + (pixelY*m_pSurface->w)};

		uint8_t r{}, g{}, b{};
		SDL_GetRGB(m_pSurfacePixels[pixel], m_pSurface->format, &r, &g, &b);

		return {float(r)/255.f, float(g)/255.f, float(b)/255.f};
	}
}