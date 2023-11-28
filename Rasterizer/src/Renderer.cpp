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
	m_pTexture = Texture::LoadFromFile("Resources/uv_grid_2.png");

	//Initialize Camera
	m_Camera.Initialize(60.f, { .0f,.0f,-10.f });
}

Renderer::~Renderer()
{
	delete[] m_pDepthBufferPixels;
	delete m_pTexture;
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
		vertices_out.push_back({ {m_Camera.viewMatrix.TransformPoint(vertices_in[i].position)}, vertices_in[i].color, vertices_in[i].uv}); // transform with viewmatrix

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
			transformedVertices.push_back({ {m_Camera.viewMatrix.TransformPoint(meshes_in[meshIdx].vertices[verticeIdx].position)}, 
											meshes_in[meshIdx].vertices[verticeIdx].color, 
											meshes_in[meshIdx].vertices[verticeIdx].uv }); // transform with viewmatrix

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
	Vector2 edge{}, vertexToPixel{};

	for (int verticeIdx = startIdx; verticeIdx - startIdx < 3; ++verticeIdx)
	{
		if (strip && startIdx % 2 == 1)
		{
			edge.x = vertices[verticeIdx].position.x - vertices[(verticeIdx - startIdx + 1) % 3 + startIdx].position.x;
			edge.y = vertices[verticeIdx].position.y - vertices[(verticeIdx - startIdx + 1) % 3 + startIdx].position.y;
			vertexToPixel.x = pixel.x - vertices[(verticeIdx - startIdx + 1) % 3 + startIdx].position.x;
			vertexToPixel.y = pixel.y - vertices[(verticeIdx - startIdx + 1) % 3 + startIdx].position.y;
		}
		else
		{
			edge.x = vertices[(verticeIdx - startIdx + 1) % 3 + startIdx].position.x - vertices[verticeIdx].position.x;
			edge.y = vertices[(verticeIdx - startIdx + 1) % 3 + startIdx].position.y - vertices[verticeIdx].position.y;
			vertexToPixel.x = pixel.x - vertices[verticeIdx].position.x;
			vertexToPixel.y = pixel.y - vertices[verticeIdx].position.y;
		}
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

void Renderer::Render_W2()
{

	std::vector<Mesh> meshes_world /*triangle strip*/
	{
		Mesh{
					{
				Vertex{ {-3.f, 3.f,-2.f},{}, {0.f,0.f}},
				Vertex{ { 0.f, 3.f,-2.f},{}, {0.5f,0.f}},
				Vertex{ { 3.f, 3.f,-2.f},{}, {1.f,0.f}},
				Vertex{ {-3.f, 0.f,-2.f},{}, {0.f,0.5f}},
				Vertex{ { 0.f, 0.f,-2.f},{}, {0.5f,0.5f}},
				Vertex{ { 3.f, 0.f,-2.f},{}, {1.f,0.5f}},
				Vertex{ {-3.f,-3.f,-2.f},{}, {0.f, 1.f}},
				Vertex{ { 0.f,-3.f,-2.f},{}, {0.5f,1.f}},
				Vertex{ { 3.f,-3.f,-2.f},{}, {1.f,1.f}}
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
	//			Vertex{ {-3.f, 3.f,-2.f},{}, {0.f,0.f}},
	//			Vertex{ { 0.f, 3.f,-2.f},{}, {0.5f,0.f}},
	//			Vertex{ { 3.f, 3.f,-2.f},{}, {1.f,0.f}},
	//			Vertex{ {-3.f, 0.f,-2.f},{}, {0.f,0.5f}},
	//			Vertex{ { 0.f, 0.f,-2.f},{}, {0.5f,0.5f}},
	//			Vertex{ { 3.f, 0.f,-2.f},{}, {1.f,0.5f}},
	//			Vertex{ {-3.f,-3.f,-2.f},{}, {0.f, 1.f}},
	//			Vertex{ { 0.f,-3.f,-2.f},{}, {0.5f,1.f}},
	//			Vertex{ { 3.f,-3.f,-2.f},{}, {1.f,1.f}}
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
						const float triangleArea{ weights[0] + weights[1] + weights[2] };

						// normalize weights
						weights[0] /= triangleArea;
						weights[1] /= triangleArea;
						weights[2] /= triangleArea;

						// check if pixel's depth value is smaller then stored one in depth buffer
						const float interpolatedDepth{ 1.f / (1.f/vertices[triangleIdx + 0].position.z * weights[0] +
															  1.f/vertices[triangleIdx + 1].position.z * weights[1] +
															  1.f/vertices[triangleIdx + 2].position.z * weights[2]) };

						if (interpolatedDepth < m_pDepthBufferPixels[px + (py * m_Width)])
						{
							m_pDepthBufferPixels[px + (py * m_Width)] = interpolatedDepth;

							const Vector2 uvInterpolated{ (vertices[triangleIdx + 0].uv / vertices[triangleIdx + 0].position.z * weights[0] +
													 vertices[triangleIdx + 1].uv / vertices[triangleIdx + 1].position.z * weights[1] +
													 vertices[triangleIdx + 2].uv / vertices[triangleIdx + 2].position.z * weights[2]) * interpolatedDepth };

							finalColor = m_pTexture->Sample(uvInterpolated);

							//Update Color in Buffer
							finalColor.MaxToOne();

							m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
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