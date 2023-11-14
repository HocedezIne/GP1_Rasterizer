//External includes
#include "SDL.h"
#include "SDL_surface.h"

//Project includes
#include "Renderer.h"
#include "Maths.h"
#include "Texture.h"
#include "Utils.h"

using namespace dae;

Renderer::Renderer(SDL_Window* pWindow) :
	m_pWindow(pWindow)
{
	//Initialize
	SDL_GetWindowSize(pWindow, &m_Width, &m_Height);

	//Create Buffers
	m_pFrontBuffer = SDL_GetWindowSurface(pWindow);
	m_pBackBuffer = SDL_CreateRGBSurface(0, m_Width, m_Height, 32, 0, 0, 0, 0);
	m_pBackBufferPixels = (uint32_t*)m_pBackBuffer->pixels;

	//m_pDepthBufferPixels = new float[m_Width * m_Height];

	//Initialize Camera
	m_Camera.Initialize(60.f, { .0f,.0f,-10.f });
}

Renderer::~Renderer()
{
	//delete[] m_pDepthBufferPixels;
}

void Renderer::Update(Timer* pTimer)
{
	m_Camera.Update(pTimer);
}

void Renderer::Render()
{
	//@START
	//Lock BackBuffer
	SDL_LockSurface(m_pBackBuffer);

	//Render_W1_Part1(); //Rasterizer Stage Only
	//Render_W1_Part2(); //Projection Stage only
	Render_W1_Part3(); //Barycentric Coordinates

	//@END
	//Update SDL Surface
	SDL_UnlockSurface(m_pBackBuffer);
	SDL_BlitSurface(m_pBackBuffer, 0, m_pFrontBuffer, 0);
	SDL_UpdateWindowSurface(m_pWindow);
}

void Renderer::VertexTransformationFunction(const std::vector<Vertex>& vertices_in, std::vector<Vertex>& vertices_out) const
{
	for (int i{}; i < /*vertices_in.size()*/3; i++)
	{
		vertices_out.push_back({ {m_Camera.viewMatrix.TransformPoint(vertices_in[i].position)}, vertices_in[i].color }); // transform with viewmatrix

		// perspective divide
		vertices_out[i].position.x /= vertices_out[i].position.z;
		vertices_out[i].position.y /= vertices_out[i].position.z;

		// camera setting and screen size
		vertices_out[i].position.x /= (m_Width / static_cast<float>(m_Height)) * m_Camera.fov;
		vertices_out[i].position.y /= m_Camera.fov;

		// NDC to screen space
		vertices_out[i].position.x = (vertices_out[i].position.x + 1) /2 * m_Width;
		vertices_out[i].position.y = (1 - vertices_out[i].position.y) / 2 * m_Height;
	}
}

bool Renderer::SaveBufferToImage() const
{
	return SDL_SaveBMP(m_pBackBuffer, "Rasterizer_ColorBuffer.bmp");
}

void Renderer::Render_W1_Part1()
{
	// Define Triangle - Vertices in NDC space
	std::vector<Vector3> vertices_ndc = { { 0.f,.5f,1.f } , { .5f,-.5f,1.f } , { -.5f,-.5f,1.f } };

	std::vector<Vector2> screenSpaceCoord{};

	for (int idx{}; idx < vertices_ndc.size(); idx++)
	{
		screenSpaceCoord.push_back({ (vertices_ndc[idx].x + 1) / 2 * m_Width, (1 - vertices_ndc[idx].y) / 2 * m_Height });
	}

	for (int px{}; px < m_Width; ++px)
	{
		for (int py{}; py < m_Height; ++py)
		{
			ColorRGB finalColor{};

			bool earlyReturn{ false };
			for (int idx = 0; idx < 3; idx++)
			{
				Vector2 edge{ screenSpaceCoord[(idx + 1) % 3].x - screenSpaceCoord[idx].x,  screenSpaceCoord[(idx + 1) % 3].y - screenSpaceCoord[idx].y};
				Vector2 vertexToPixel{ px - screenSpaceCoord[idx].x, py - screenSpaceCoord[idx].y };
				float weight{ Vector2::Cross(edge, vertexToPixel) };
				if (weight < 0.f)
				{
					earlyReturn = true;
					break;
				}
			}
			if (!earlyReturn) finalColor = { 1.f,1.f,1.f };

			//Update Color in Buffer
			finalColor.MaxToOne();

			m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
				static_cast<uint8_t>(finalColor.r * 255),
				static_cast<uint8_t>(finalColor.g * 255),
				static_cast<uint8_t>(finalColor.b * 255));
		}
	}
}

void Renderer::Render_W1_Part2()
{
	// Define Triangle - vertices in WORLD space
	std::vector<Vertex> vertices_world
	{
		{ {0.f,2.f,0.f}},
		{ {1.f,0.f,0.f}},
		{ {-1.f, 0.f,0.f}},
	};

	std::vector<Vertex> vertices_out{};

	VertexTransformationFunction(vertices_world, vertices_out);

	for (int px{}; px < m_Width; ++px)
	{
		for (int py{}; py < m_Height; ++py)
		{
			ColorRGB finalColor{};

			bool earlyReturn{ false };
			for (int idx = 0; idx < 3; idx++)
			{
				Vector2 edge{ vertices_out[(idx + 1) % 3].position.x - vertices_out[idx].position.x,  vertices_out[(idx + 1) % 3].position.y - vertices_out[idx].position.y };
				Vector2 vertexToPixel{ px - vertices_out[idx].position.x, py - vertices_out[idx].position.y};
				float weight{ Vector2::Cross(edge, vertexToPixel) };
				if (weight < 0.f) 
				{
					earlyReturn = true;
					break;
				}
			}
			if (!earlyReturn) finalColor = { 1.f,1.f,1.f };

			//Update Color in Buffer
			finalColor.MaxToOne();

			m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
				static_cast<uint8_t>(finalColor.r * 255),
				static_cast<uint8_t>(finalColor.g * 255),
				static_cast<uint8_t>(finalColor.b * 255));
		}
	}
}

void Renderer::Render_W1_Part3()
{
	// Define Triangle - vertices in WORLD space
	std::vector<Vertex> vertices_world
	{
		{ {0.f,4.f,2.f}, {1,0,0}},
		{ {3.f,-2.f,2.f}, {0,1,0}},
		{ {-3.f, -2.f,2.f}, {0,0,1}},
	};

	std::vector<Vertex> vertices_out{};
	vertices_out.reserve(vertices_world.size());

	VertexTransformationFunction(vertices_world, vertices_out);

	for (int px{}; px < m_Width; ++px)
	{
		for (int py{}; py < m_Height; ++py)
		{
			ColorRGB finalColor{};

			bool earlyReturn{ false };
			std::vector<float> weights{};
			weights.reserve(3);

			for (int idx = 0; idx < 3; idx++)
			{
				Vector2 edge{ vertices_out[(idx + 1) % 3].position.x - vertices_out[idx].position.x,  vertices_out[(idx + 1) % 3].position.y - vertices_out[idx].position.y };
				Vector2 vertexToPixel{ px - vertices_out[idx].position.x, py - vertices_out[idx].position.y };
				weights.push_back(Vector2::Cross(edge, vertexToPixel));
				if (weights[idx] < 0.f)
				{
					earlyReturn = true;
					break;
				}
			}
			if (!earlyReturn)
			{
				float triangleArea{ weights[0] + weights[1] + weights[2] };
				finalColor = { vertices_out[0].color * (weights[1] / triangleArea) +
							   vertices_out[1].color * (weights[2] / triangleArea) +
							   vertices_out[2].color * (weights[0] / triangleArea) };
			}

			//Update Color in Buffer
			finalColor.MaxToOne();

			m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
				static_cast<uint8_t>(finalColor.r * 255),
				static_cast<uint8_t>(finalColor.g * 255),
				static_cast<uint8_t>(finalColor.b * 255));
		}
	}
}