#pragma once

#include <cstdint>
#include <vector>

#include "Camera.h"

struct SDL_Window;
struct SDL_Surface;

namespace dae
{
	class Texture;
	struct Mesh;
	struct Vertex;
	struct Vertex_Out;
	class Timer;
	class Scene;

	class Renderer final
	{
	public:
		Renderer(SDL_Window* pWindow);
		~Renderer();

		Renderer(const Renderer&) = delete;
		Renderer(Renderer&&) noexcept = delete;
		Renderer& operator=(const Renderer&) = delete;
		Renderer& operator=(Renderer&&) noexcept = delete;

		void Update(Timer* pTimer);
		void Render();

		void Render_W3();

		bool SaveBufferToImage() const;

		void VertexTransformationFunction(std::vector<Mesh>& meshes) const;

		bool IsPixelInTriangle(const std::vector<Vertex_Out>& vertices, const Vector2& pixel, std::vector<float>& weights, const int startIdx = 0, const bool strip = false);

		const std::vector<Vertex_Out> CreateTriangle(const Mesh& mesh);

	private:
		SDL_Window* m_pWindow{};

		SDL_Surface* m_pFrontBuffer{ nullptr };
		SDL_Surface* m_pBackBuffer{ nullptr };
		uint32_t* m_pBackBufferPixels{};

		float* m_pDepthBufferPixels{};
		Texture* m_pTexture{ nullptr };

		Camera m_Camera{};

		int m_Width{};
		int m_Height{};
	};
}
