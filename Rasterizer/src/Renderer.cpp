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
	m_pTexture = Texture::LoadFromFile("Resources/tuktuk.png");

	m_ObjectMeshes.push_back(Mesh{});
	Utils::ParseOBJ("Resources/tuktuk.obj", m_ObjectMeshes[0].vertices, m_ObjectMeshes[0].indices);
	m_ObjectMeshes[0].vertices_out.reserve(m_ObjectMeshes[0].vertices.size());

	//Initialize Camera
	m_Camera.Initialize((m_Width / static_cast<float>(m_Height)), 60.f, { .0f,5.f,-30.f });
}

Renderer::~Renderer()
{
	delete[] m_pDepthBufferPixels;
	delete m_pTexture;
}

void Renderer::Update(Timer* pTimer)
{
	m_Camera.Update(pTimer);

	Matrix rotation{ Matrix::CreateRotationY(pTimer->GetElapsed() * PI_DIV_4) };
	for (Mesh& mesh : m_ObjectMeshes)
	{
		mesh.worldMatrix *= rotation;
	}
}

void Renderer::Render()
{
	//@START
	//Lock BackBuffer
	SDL_LockSurface(m_pBackBuffer);

	Render_W3();

	//@END
	//Update SDL Surface
	SDL_UnlockSurface(m_pBackBuffer);
	SDL_BlitSurface(m_pBackBuffer, 0, m_pFrontBuffer, 0);
	SDL_UpdateWindowSurface(m_pWindow);
}

void Renderer::VertexTransformationFunction(std::vector<Mesh>& meshes) const
{
	for (int idx{}; idx < meshes.size(); ++idx)
	{
		Matrix worldViewProjection = meshes[idx].worldMatrix * m_Camera.viewMatrix * m_Camera.projectionMatrix;

		for (int verticeIdx{}; verticeIdx < meshes[idx].vertices.size(); ++verticeIdx)
		{
			Vector4 transformedPosition{ meshes[idx].vertices[verticeIdx].position, 1.f };
			transformedPosition = worldViewProjection.TransformPoint(transformedPosition);

			// perspective divide
			transformedPosition.x /= transformedPosition.w;
			transformedPosition.y /= transformedPosition.w;
			transformedPosition.z /= transformedPosition.w;

			// NDC to screen space
			transformedPosition.x = ((transformedPosition.x + 1) / 2) * m_Width;
			transformedPosition.y = ((1 - transformedPosition.y) / 2) * m_Height;

			meshes[idx].vertices_out[verticeIdx] = Vertex_Out{ transformedPosition, meshes[idx].vertices[verticeIdx].color, meshes[idx].vertices[verticeIdx].uv, meshes[idx].vertices[verticeIdx].normal, meshes[idx].vertices[verticeIdx].tangent };
		}
	}
}

bool Renderer::IsPixelInTriangle(const std::vector<Vertex_Out>& vertices, const Vector2& pixel, std::vector<float>& weights, const int startIdx, bool strip)
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

const std::vector<Vertex_Out> Renderer::CreateOrderedVertices(const Mesh& mesh)
{
	std::vector<Vertex_Out> result;

	for (int idx = 0; idx < mesh.indices.size(); idx++)
	{
		result.push_back(mesh.vertices_out[mesh.indices[idx]]);
	}

	return result;
}

bool Renderer::SaveBufferToImage() const
{
	return SDL_SaveBMP(m_pBackBuffer, "Rasterizer_ColorBuffer.bmp");
}

void Renderer::Render_W3()
{
	VertexTransformationFunction(m_ObjectMeshes);

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

	for (const Mesh& mesh : m_ObjectMeshes)
	{
		const int increment{ (mesh.primitiveTopology == PrimitiveTopology::TriangleList) ? 3 : 1 };
		const auto loopLenght{ (mesh.primitiveTopology == PrimitiveTopology::TriangleList) ? mesh.indices.size() : mesh.indices.size() - 2 };

		const std::vector<Vertex_Out> vertices{ CreateOrderedVertices(mesh) };

		for (int triangleIdx{}; triangleIdx < loopLenght; triangleIdx += increment)
		{
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
						const float interpolatedZDepth{ 1 / ((1 / vertices[triangleIdx + 0].position.z) * weights[0] +
															 (1 / vertices[triangleIdx + 1].position.z) * weights[1] +
															 (1 / vertices[triangleIdx + 2].position.z) * weights[2]) };

						if (0.f > interpolatedZDepth || interpolatedZDepth > 1.f) continue;

						if (interpolatedZDepth < m_pDepthBufferPixels[px + (py * m_Width)])
						{
							m_pDepthBufferPixels[px + (py * m_Width)] = interpolatedZDepth;

							//const float interpolatedWDepth{ 1.f / (1.f / vertices[triangleIdx + 0].position.w * weights[0] +
							//								  1.f / vertices[triangleIdx + 1].position.w * weights[1] +
							//								  1.f / vertices[triangleIdx + 2].position.w * weights[2]) };

							if (m_showDepthBuffer)
							{
								float color = Remap(interpolatedZDepth, 0.995f, 1.f);
								finalColor = { color, color, color };
							}
								
							else
							{
								const float wDepth{ 1 / ((1 / vertices[triangleIdx + 0].position.w) * weights[0] +
														 (1 / vertices[triangleIdx + 1].position.w) * weights[1] +
														 (1 / vertices[triangleIdx + 2].position.w) * weights[2]) };

								Vector2 uvInterpolated{ (vertices[triangleIdx + 0].uv / vertices[triangleIdx + 0].position.w * weights[0] +
													 vertices[triangleIdx + 1].uv / vertices[triangleIdx + 1].position.w * weights[1] +
													 vertices[triangleIdx + 2].uv / vertices[triangleIdx + 2].position.w * weights[2]) * wDepth };

								uvInterpolated.x = Clamp(uvInterpolated.x, 0.f, 1.f);
								uvInterpolated.y = Clamp(uvInterpolated.y, 0.f, 1.f);

								finalColor = m_pTexture->Sample(uvInterpolated);
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
			}
		}
	}
}