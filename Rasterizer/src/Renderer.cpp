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

	m_pDepthBufferPixels = new float[m_Width * m_Height];

	//Initialize Camera
	m_Camera.Initialize(60.f, { .0f,.0f,-10.f });
}

Renderer::~Renderer()
{
	delete[] m_pDepthBufferPixels;
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
	//Render_W1_Part3(); //Barycentric Coordinates
	//Render_W1_Part4(); //Depth buffer
	//Render_W1_Part5(); //Bounding box optimization

	Render_W2();

	//@END
	//Update SDL Surface
	SDL_UnlockSurface(m_pBackBuffer);
	SDL_BlitSurface(m_pBackBuffer, 0, m_pFrontBuffer, 0);
	SDL_UpdateWindowSurface(m_pWindow);
}

void Renderer::VertexTransformationFunction(const std::vector<Vertex>& vertices_in, std::vector<Vertex>& vertices_out) const
{
	for (int i{}; i < vertices_in.size(); ++i)
	{
		vertices_out.push_back({ {m_Camera.viewMatrix.TransformPoint(vertices_in[i].position)}, vertices_in[i].color }); // transform with viewmatrix

		// perspective divide
		vertices_out[i].position.x /= vertices_out[i].position.z;
		vertices_out[i].position.y /= vertices_out[i].position.z;

		// camera setting and screen size
		vertices_out[i].position.x /= (m_Width / static_cast<float>(m_Height)) * m_Camera.fov;
		vertices_out[i].position.y /= m_Camera.fov;

		// NDC to screen space
		vertices_out[i].position.x = ((vertices_out[i].position.x + 1) /2) * m_Width;
		vertices_out[i].position.y = ((1 - vertices_out[i].position.y) / 2) * m_Height;
	}
}

void Renderer::VertexTransformationFunction(const std::vector<Mesh>& meshes_in, std::vector<Mesh>& meshes_out) const
{
	for (int meshIdx{}; meshIdx < meshes_in.size(); ++meshIdx)
	{
		std::vector<Vertex> transformedVertices{};

		for (int verticeIdx{}; verticeIdx < meshes_in[meshIdx].vertices.size(); ++verticeIdx)
		{
			transformedVertices.push_back({ {m_Camera.viewMatrix.TransformPoint(meshes_in[meshIdx].vertices[verticeIdx].position)}, meshes_in[meshIdx].vertices[verticeIdx].color }); // transform with viewmatrix

			// perspective divide
			transformedVertices[verticeIdx].position.x /= transformedVertices[verticeIdx].position.z;
			transformedVertices[verticeIdx].position.y /= transformedVertices[verticeIdx].position.z;

			// camera setting and screen size
			transformedVertices[verticeIdx].position.x /= (m_Width / static_cast<float>(m_Height)) * m_Camera.fov;
			transformedVertices[verticeIdx].position.y /= m_Camera.fov;

			// NDC to screen space
			transformedVertices[verticeIdx].position.x = ((transformedVertices[verticeIdx].position.x + 1) / 2) * m_Width;
			transformedVertices[verticeIdx].position.y = ((1 - transformedVertices[verticeIdx].position.y) / 2) * m_Height;
		}

		meshes_out.push_back(Mesh{ transformedVertices, meshes_in[meshIdx].indices, meshes_in[meshIdx].primitiveTopology});
	}
}

bool Renderer::IsPixelInTriangle(const std::vector<Vertex>& vertices, const Vector2& pixel, std::vector<float>& weights, const int startIdx, bool strip)
{
	const int increment{ (strip && startIdx % 2 == 1) ? 3 : 1 };

	for (int verticeIdx = startIdx; verticeIdx - startIdx < 3; verticeIdx = (verticeIdx + increment)%3)
	{
		const Vector2 edge{ vertices[(verticeIdx - startIdx + 1) % 3 + startIdx].position.x - vertices[verticeIdx].position.x, vertices[(verticeIdx - startIdx + 1) % 3 + startIdx].position.y - vertices[verticeIdx].position.y };
		const Vector2 vertexToPixel{ pixel.x - vertices[verticeIdx].position.x, pixel.y - vertices[verticeIdx].position.y };
		weights[(verticeIdx - startIdx + 2) % 3] = Vector2::Cross(edge, vertexToPixel);
		if (weights[(verticeIdx - startIdx + 2) % 3] < 0.f)
		{
			return false;
		}
	}

	return true;
}

const std::vector<Vertex> Renderer::CreateTriangle(const Mesh& mesh)
{
	std::vector<Vertex> result;

	for (int idx = 0; idx < mesh.indices.size(); idx++)
	{
		result.push_back(mesh.vertices[mesh.indices[idx]]);
	}

	return result;
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
	std::vector<float> weights{ {},{},{} };

	for (int px{}; px < m_Width; ++px)
	{
		for (int py{}; py < m_Height; ++py)
		{
			ColorRGB finalColor{};

			bool earlyReturn{ false };

			for (int idx = 0; idx < 3; idx++)
			{
				const Vector2 edge{ vertices_out[(idx + 1) % 3].position.x - vertices_out[idx].position.x,  vertices_out[(idx + 1) % 3].position.y - vertices_out[idx].position.y };
				const Vector2 vertexToPixel{ px - vertices_out[idx].position.x, py - vertices_out[idx].position.y };
				weights[(idx + 2) % 3] = Vector2::Cross(edge, vertexToPixel);
				if (weights[(idx + 2) % 3] < 0.f)
				{
					earlyReturn = true;
					break;
				}
			}
			if (!earlyReturn)
			{
				const float triangleArea{ weights[0] + weights[1] + weights[2] };
				finalColor = { vertices_out[0].color * (weights[0] / triangleArea) +
							   vertices_out[1].color * (weights[1] / triangleArea) +
							   vertices_out[2].color * (weights[2] / triangleArea) };
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

void Renderer::Render_W1_Part4()
{
	// Define Triangle - vertices in WORLD space
	std::vector<Vertex> vertices_world
	{
		// Triangle 0
		{ { 0.f,2.f,0.f}, {1,0,0}},
		{ { 1.5f,-1.f,0.f}, {1,0,0}},
		{ {-1.5f,-1.f,0.f}, {1,0,0}},

		// Triangle 1
		{ { 0.f,  4.f, 2.f}, {1,0,0}},
		{ { 3.f, -2.f, 2.f}, {0,1,0}},
		{ {-3.f, -2.f, 2.f}, {0,0,1}}
	};

	std::vector<Vertex> vertices_out{};
	vertices_out.reserve(vertices_world.size());

	VertexTransformationFunction(vertices_world, vertices_out);

	std::vector<float> weights;
	for (int i{}; i <3;i++) weights.push_back(float{});

	// fill depth buffer
	std::fill_n(m_pDepthBufferPixels, m_Width * m_Height, FLT_MAX);

	// clear backbuffer
	SDL_FillRect(m_pBackBuffer, &m_pBackBuffer->clip_rect, SDL_MapRGB(m_pBackBuffer->format, 100, 100, 100));

	ColorRGB finalColor{};
	

	for (int triangleIdx{}; triangleIdx < vertices_world.size(); triangleIdx+=3)
	{
		for (int px{}; px < m_Width; ++px)
		{
			for (int py{}; py < m_Height; ++py)
			{
				bool earlyReturn{ false };

				for (int VerticeIdx = triangleIdx; VerticeIdx - triangleIdx < 3; VerticeIdx++)
				{
					const Vector2 edge{ vertices_out[(VerticeIdx - triangleIdx + 1) % 3 + triangleIdx].position.x - vertices_out[VerticeIdx].position.x,  vertices_out[(VerticeIdx - triangleIdx + 1) % 3 + triangleIdx].position.y - vertices_out[VerticeIdx].position.y };
					const Vector2 vertexToPixel{ px - vertices_out[VerticeIdx].position.x, py - vertices_out[VerticeIdx].position.y };
					weights[(VerticeIdx - triangleIdx + 2) % 3] = Vector2::Cross(edge, vertexToPixel);
					if (weights[(VerticeIdx - triangleIdx + 2) % 3] < 0.f)
					{
						earlyReturn = true;
						break;
					}
				}
				if (!earlyReturn)
				{
					//// check if pixel's depth value is smaller then stored one in depth buffer
					const float interpolatedDepth{vertices_out[triangleIdx + 0 ].position.z * weights[0] +
								vertices_out[triangleIdx + 1].position.z * weights[1] + 
								vertices_out[triangleIdx + 2].position.z * weights[2]};
					
					const int pixelIdx{ px + (py * m_Width) };

					const float triangleArea{ weights[0] + weights[1] + weights[2] };
					finalColor = { vertices_out[triangleIdx + 0].color * (weights[0] / triangleArea) +
								   vertices_out[triangleIdx + 1].color * (weights[1] / triangleArea) +
								   vertices_out[triangleIdx + 2].color * (weights[2] / triangleArea) };

					if (interpolatedDepth < m_pDepthBufferPixels[pixelIdx])
					{
						m_pDepthBufferPixels[pixelIdx] = interpolatedDepth;
					
						const float triangleArea{ weights[0] + weights[1] + weights[2] };
						finalColor = { vertices_out[triangleIdx + 0].color * (weights[0] / triangleArea) +
									   vertices_out[triangleIdx + 1].color * (weights[1] / triangleArea) +
									   vertices_out[triangleIdx + 2].color * (weights[2] / triangleArea) };
					
						//Update Color in Buffer
						finalColor.MaxToOne();
					
						m_pBackBufferPixels[pixelIdx] = SDL_MapRGB(m_pBackBuffer->format,
							static_cast<uint8_t>(finalColor.r * 255),
							static_cast<uint8_t>(finalColor.g * 255),
							static_cast<uint8_t>(finalColor.b * 255));
					}
				}
			}
		}
	}
}

void Renderer::Render_W1_Part5()
{
	// Define Triangle - vertices in WORLD space
	std::vector<Vertex> vertices_world
	{
		// Triangle 0
		{ { 0.f,2.f,0.f}, {1,0,0}},
		{ { 1.5f,-1.f,0.f}, {1,0,0}},
		{ {-1.5f,-1.f,0.f}, {1,0,0}},

		// Triangle 1
		{ { 0.f,  4.f, 2.f}, {1,0,0}},
		{ { 3.f, -2.f, 2.f}, {0,1,0}},
		{ {-3.f, -2.f, 2.f}, {0,0,1}}
	};

	std::vector<Vertex> vertices_out{};
	vertices_out.reserve(vertices_world.size());

	VertexTransformationFunction(vertices_world, vertices_out);

	// fill depth buffer
	std::fill_n(m_pDepthBufferPixels, m_Width * m_Height, FLT_MAX);

	// clear backbuffer
	SDL_FillRect(m_pBackBuffer, &m_pBackBuffer->clip_rect, SDL_MapRGB(m_pBackBuffer->format, 100, 100, 100));

	// Setting frequently used variables that are loop safe 
	ColorRGB finalColor{};
	std::vector<float> weights;
	for (int i{}; i < 3; i++) weights.push_back(float{});
	std::pair<int,int> boundingBoxTopLeft{};
	std::pair<int, int> boundingBoxBottomRight{};


	for (int triangleIdx{}; triangleIdx < vertices_world.size(); triangleIdx += 3)
	{
		// calc bounding box
		boundingBoxTopLeft.first = std::max(0, std::min(int(std::min(vertices_out[triangleIdx].position.x, std::min(vertices_out[triangleIdx + 1].position.x, vertices_out[triangleIdx + 2].position.x))), m_Width - 1));
		boundingBoxTopLeft.second = std::max(0, std::min(int(std::min(vertices_out[triangleIdx].position.y, std::min(vertices_out[triangleIdx + 1].position.y, vertices_out[triangleIdx + 2].position.y))), m_Height - 1));
		boundingBoxBottomRight.first = std::max(0, std::min(int(std::max(vertices_out[triangleIdx].position.x, std::max(vertices_out[triangleIdx + 1].position.x, vertices_out[triangleIdx + 2].position.x))), m_Width - 1));
		boundingBoxBottomRight.second = std::max(0, std::min(int(std::max(vertices_out[triangleIdx].position.y, std::max(vertices_out[triangleIdx + 1].position.y, vertices_out[triangleIdx + 2].position.y))), m_Height - 1));

		for (int px{ boundingBoxTopLeft.first }; px < boundingBoxBottomRight.first; ++px)
		{
			for (int py{ boundingBoxTopLeft.second }; py < boundingBoxBottomRight.second; ++py)
			{
				bool earlyReturn{ false };

				for (int verticeIdx = triangleIdx; verticeIdx - triangleIdx < 3; verticeIdx++)
				{
					const Vector2 edge{ vertices_out[(verticeIdx - triangleIdx + 1) % 3 + triangleIdx].position.x - vertices_out[verticeIdx].position.x,  vertices_out[(verticeIdx - triangleIdx + 1) % 3 + triangleIdx].position.y - vertices_out[verticeIdx].position.y };
					const Vector2 vertexToPixel{ px - vertices_out[verticeIdx].position.x, py - vertices_out[verticeIdx].position.y };
					weights[(verticeIdx - triangleIdx + 2) % 3] = Vector2::Cross(edge, vertexToPixel);
					if (weights[(verticeIdx - triangleIdx + 2) % 3] < 0.f)
					{
						earlyReturn = true;
						break;
					}
				}
				if (!earlyReturn)
				{
					// check if pixel's depth value is smaller then stored one in depth buffer
					const float interpolatedDepth{ vertices_out[triangleIdx + 0].position.z * weights[0] +
								vertices_out[triangleIdx + 1].position.z * weights[1] +
								vertices_out[triangleIdx + 2].position.z * weights[2] };

					const int pixelIdx{ px + (py * m_Width) };

					if (interpolatedDepth < m_pDepthBufferPixels[pixelIdx])
					{
						m_pDepthBufferPixels[pixelIdx] = interpolatedDepth;

						const float triangleArea{ weights[0] + weights[1] + weights[2] };
						finalColor = { vertices_out[triangleIdx + 0].color * (weights[0] / triangleArea) +
									   vertices_out[triangleIdx + 1].color * (weights[1] / triangleArea) +
									   vertices_out[triangleIdx + 2].color * (weights[2] / triangleArea) };

						//Update Color in Buffer
						finalColor.MaxToOne();

						m_pBackBufferPixels[pixelIdx] = SDL_MapRGB(m_pBackBuffer->format,
							static_cast<uint8_t>(finalColor.r * 255),
							static_cast<uint8_t>(finalColor.g * 255),
							static_cast<uint8_t>(finalColor.b * 255));
					}
				}
			}
		}
	}
}

void Renderer::Render_W2()
{
	std::vector<Mesh> meshes_world /*triangle strip*/
	{
		Mesh{
					{
				Vertex{ {-3.f, 3.f,-2.f}, {1.f,0.f,0.f}},
				Vertex{ { 0.f, 3.f,-2.f}, {0.f,1.f,0.f}},
				Vertex{ { 3.f, 3.f,-2.f}, {0.f,0.f,1.f}},
				Vertex{ {-3.f, 0.f,-2.f}, {1.f,1.f,1.f}},
				Vertex{ { 0.f, 0.f,-2.f}, {1.f,0.f,0.f}},
				Vertex{ { 3.f, 0.f,-2.f}, {0.f,1.f,0.f}},
				Vertex{ {-3.f,-3.f,-2.f}, {0.f,0.f,1.f}},
				Vertex{ { 0.f,-3.f,-2.f}, {1.f,1.f,1.f}},
				Vertex{ { 3.f,-3.f,-2.f}, {1.f,0.f,0.f}}
			},
				{
			3,0,4,1,5,2,
			2,6,
			6,3,7,4,8,5
			},
			PrimitiveTopology::TriangleStrip
		}
	};	
	//std::vector<Mesh> meshes_world /*triangle list*/
	//{
	//	Mesh{
	//				{
	//			Vertex{ {-3.f,3.f,-2.f}},
	//			Vertex{ {0.f,3.f,-2.f}},
	//			Vertex{ {3.f,3.f,-2.f}},
	//			Vertex{ {-3.f,0.f,-2.f}},
	//			Vertex{{0.f,0.f,-2.f}},
	//			Vertex{{3.f,0.f,-2.f}},
	//			Vertex{{-3.f,-3.f,-2.f}},
	//			Vertex{{0.f,-3.f,-2.f}},
	//			Vertex{{3.f,-3.f,-2.f}}
	//		},
	//			{
	//		3,0,1,	1,4,3,	4,1,2,
	//		2,5,4,	6,3,4,	4,7,6,
	//		7,4,5,	5,8,7
	//		},
	//		PrimitiveTopology::TriangleList
	//	}
	//};

	std::vector<Mesh> meshes_out{};
	meshes_out.reserve(meshes_world.size());

	VertexTransformationFunction(meshes_world, meshes_out);

	// fill depth buffer
	std::fill_n(m_pDepthBufferPixels, m_Width * m_Height, FLT_MAX);

	// clear backbuffer
	SDL_FillRect(m_pBackBuffer, &m_pBackBuffer->clip_rect, SDL_MapRGB(m_pBackBuffer->format, 100, 100, 100));

	// Setting frequently used variables that are loop safe 
	ColorRGB finalColor{};
	std::vector<float> weights;
	for (int i{}; i < 3; i++) weights.push_back(float{});
	std::pair<int, int> boundingBoxTopLeft{};
	std::pair<int, int> boundingBoxBottomRight{};

	for (const Mesh& mesh : meshes_out)
	{
		const int increment{ (mesh.primitiveTopology == PrimitiveTopology::TriangleList) ? 3 : 1 };
		const auto loopLenght{ (mesh.primitiveTopology == PrimitiveTopology::TriangleList) ? mesh.indices.size() : mesh.indices.size() - 2 };

		for (int triangleIdx{}; triangleIdx < loopLenght; triangleIdx += increment)
		{
			const std::vector<Vertex> vertices{ CreateTriangle(mesh) };

			// calc bounding box
			boundingBoxTopLeft.first = std::max(0, std::min(int(std::min(vertices[triangleIdx].position.x, std::min(vertices[triangleIdx + 1].position.x, vertices[triangleIdx + 2].position.x))), m_Width - 1));
			boundingBoxTopLeft.second = std::max(0, std::min(int(std::min(vertices[triangleIdx].position.y, std::min(vertices[triangleIdx + 1].position.y, vertices[triangleIdx + 2].position.y))), m_Height - 1));
			boundingBoxBottomRight.first = std::max(0, std::min(int(std::max(vertices[triangleIdx].position.x, std::max(vertices[triangleIdx + 1].position.x, vertices[triangleIdx + 2].position.x))), m_Width - 1));
			boundingBoxBottomRight.second = std::max(0, std::min(int(std::max(vertices[triangleIdx].position.y, std::max(vertices[triangleIdx + 1].position.y, vertices[triangleIdx + 2].position.y))), m_Height - 1));

			for (int px{ boundingBoxTopLeft.first }; px < boundingBoxBottomRight.first; ++px)
			{
				for (int py{ boundingBoxTopLeft.second }; py < boundingBoxBottomRight.second; ++py)
				{
					if (IsPixelInTriangle(vertices, Vector2{ float(px), float(py) }, weights, triangleIdx, mesh.primitiveTopology == PrimitiveTopology::TriangleStrip))
					{
						// check if pixel's depth value is smaller then stored one in depth buffer
						const float interpolatedDepth{ vertices[triangleIdx + 0].position.z * weights[0] +
													   vertices[triangleIdx + 1].position.z * weights[1] +
													   vertices[triangleIdx + 2].position.z * weights[2] };

						const int pixelIdx{ px + (py * m_Width) };

						if (interpolatedDepth < m_pDepthBufferPixels[pixelIdx])
						{
							m_pDepthBufferPixels[pixelIdx] = interpolatedDepth;

							const float triangleArea{ weights[0] + weights[1] + weights[2] };
							finalColor = { vertices[triangleIdx + 0].color * (weights[0] / triangleArea) +
										   vertices[triangleIdx + 1].color * (weights[1] / triangleArea) +
										   vertices[triangleIdx + 2].color * (weights[2] / triangleArea) };

							//Update Color in Buffer
							finalColor.MaxToOne();

							m_pBackBufferPixels[pixelIdx] = SDL_MapRGB(m_pBackBuffer->format,
								static_cast<uint8_t>(finalColor.r * 255),
								static_cast<uint8_t>(finalColor.g * 255),
								static_cast<uint8_t>(finalColor.b * 255));
						}
					}
				}
			}
		}
	}
}