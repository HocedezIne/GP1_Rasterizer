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
	m_pDiffuseTexture = Texture::LoadFromFile("Resources/vehicle_diffuse.png");
	m_pNormalTexture = Texture::LoadFromFile("Resources/vehicle_normal.png");
	m_pSpecularTexture = Texture::LoadFromFile("Resources/vehicle_specular.png");
	m_pGlossinessTexture = Texture::LoadFromFile("Resources/vehicle_gloss.png");

	m_ObjectMeshes.push_back(Mesh{});
	Utils::ParseOBJ("Resources/vehicle.obj", m_ObjectMeshes[0].vertices, m_ObjectMeshes[0].indices);

	//Initialize Camera
	m_Camera.Initialize((m_Width / static_cast<float>(m_Height)), 45.f, { 0.f,5.f,-64.f });

	// Lights
	m_LightDirection = Vector3{ 0.577f, -0.577f, 0.577f };
	m_Shininess = 25.f;
	m_Ambient = ColorRGB{ 0.03f, 0.03f, 0.03f };
}

Renderer::~Renderer()
{
	delete[] m_pDepthBufferPixels;
	delete m_pDiffuseTexture;
	delete m_pNormalTexture;
	delete m_pSpecularTexture;
	delete m_pGlossinessTexture;
}

void Renderer::Update(Timer* pTimer)
{
	m_Camera.Update(pTimer);

	if (m_doesRotate)
	{
		Matrix rotation{ Matrix::CreateRotationY(pTimer->GetElapsed()) };
		for (Mesh& mesh : m_ObjectMeshes)
		{
			mesh.worldMatrix = rotation * mesh.worldMatrix;
		}
	}
}

int Renderer::CycleShadingMode()
{
	m_CurrentShadingMode = static_cast<ShadingMode>((int(m_CurrentShadingMode) + 1) % 4);
	return int(m_CurrentShadingMode);
}

void Renderer::Render()
{
	//@START
	//Lock BackBuffer
	SDL_LockSurface(m_pBackBuffer);

	Render_W4();

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
		meshes[idx].vertices_out.clear();

		for (int verticeIdx{}; verticeIdx < meshes[idx].vertices.size(); ++verticeIdx)
		{			
			Vector4 transformedPosition{ meshes[idx].vertices[verticeIdx].position, 1.f };
			transformedPosition = worldViewProjection.TransformPoint(transformedPosition);
			const Vector3 transformedNormal{ meshes[idx].worldMatrix.TransformVector(meshes[idx].vertices[verticeIdx].normal).Normalized()};
			const Vector3 transformedTangent{ meshes[idx].worldMatrix.TransformVector(meshes[idx].vertices[verticeIdx].tangent)/*.Normalized()*/};
			const Vector3 viewDirection{ (meshes[idx].worldMatrix.TransformVector(meshes[idx].vertices[verticeIdx].position) - m_Camera.origin).Normalized()};

			// perspective divide
			transformedPosition.x /= transformedPosition.w;
			transformedPosition.y /= transformedPosition.w;
			transformedPosition.z /= transformedPosition.w;

			// NDC to screen space
			transformedPosition.x = ((transformedPosition.x + 1) / 2) * m_Width;
			transformedPosition.y = ((1 - transformedPosition.y) / 2) * m_Height;

			meshes[idx].vertices_out.push_back(Vertex_Out{ transformedPosition, meshes[idx].vertices[verticeIdx].color, meshes[idx].vertices[verticeIdx].uv, transformedNormal, transformedTangent, viewDirection });
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

const Vertex_Out Renderer::InterpolatedVertexAtrributes(const Vertex_Out& v0, const Vertex_Out& v1, const Vertex_Out& v2, const std::vector<float> weights)
{
	const float wDepth{ 1 / ((1 / v0.position.w) * weights[0] +
							 (1 / v1.position.w) * weights[1] +
							 (1 / v2.position.w) * weights[2]) };

	// interpolate all attributes
	const ColorRGB colorInterpolated{ (v0.color / v0.position.w * weights[0] +
									   v1.color / v1.position.w * weights[1] +
									   v2.color / v2.position.w * weights[2]) * wDepth };

	Vector2 uvInterpolated{ (v0.uv / v0.position.w * weights[0] +
						     v1.uv / v1.position.w * weights[1] +
						     v2.uv / v2.position.w * weights[2]) * wDepth };
	uvInterpolated.x = Clamp(uvInterpolated.x, 0.f, 1.f);
	uvInterpolated.y = Clamp(uvInterpolated.y, 0.f, 1.f);

	const Vector3 normalInterpolated{ (v0.normal / v0.position.w * weights[0] +
									   v1.normal / v1.position.w * weights[1] +
									   v2.normal / v2.position.w * weights[2]) * wDepth };

	const Vector3 tangentInterpolated{ (v0.tangent / v0.position.w * weights[0] +
										v1.tangent / v1.position.w * weights[1] +
										v2.tangent / v2.position.w * weights[2]) * wDepth };

	const Vector3 viewDirectionInterpolated{ (v0.viewDirection / v0.position.w * weights[0] +
											  v1.viewDirection / v1.position.w * weights[1] +
											  v2.viewDirection / v2.position.w * weights[2]) * wDepth };

	return Vertex_Out{ {}, colorInterpolated, uvInterpolated, normalInterpolated, tangentInterpolated, viewDirectionInterpolated };
}

bool Renderer::IsOutsideFrustum(const Vertex_Out& v0, const Vertex_Out& v1, const Vertex_Out& v2)
{
	if (v0.position.x < 0 || v0.position.x > m_Width || v0.position.y < 0 || v0.position.y > m_Height)
	{
		return true;
	}
	if (v1.position.x < 0 || v1.position.x > m_Width || v1.position.y < 0 || v1.position.y > m_Height) 
	{
		return true;
	}
	if (v2.position.x < 0 || v2.position.x > m_Width || v2.position.y < 0 || v2.position.y > m_Height)
	{
		return true;
	}

	return false;
}

ColorRGB Renderer::PixelShading(const Vertex_Out& v)
{
	float observedArea{};
	if (m_useNormals)
	{
		// create tangent space transformation matrix
		const Vector3 binormal{ Vector3::Cross(v.normal, v.tangent) };
		Matrix tangentScapeAxis{ v.tangent, binormal, v.normal, {} };

		const ColorRGB normalMapSample{ m_pNormalTexture->Sample(v.uv) };
		Vector3 normal{ 2.f * normalMapSample.r - 1.f, 2.f * normalMapSample.g - 1.f, 2.f * normalMapSample.b - 1.f };
		normal = tangentScapeAxis.TransformVector(normal);

		observedArea = Vector3::Dot(normal, -m_LightDirection);
	}
	else 
	{
		observedArea = Vector3::Dot(v.normal, -m_LightDirection);
	}

	if (observedArea <= 0.f) return {};

	switch (m_CurrentShadingMode)
	{
	case dae::Renderer::ShadingMode::ObservedAreaOnly:
		return {observedArea, observedArea, observedArea};
	case dae::Renderer::ShadingMode::Diffuse:
		return Lambert(7.f, m_pDiffuseTexture->Sample(v.uv)) * observedArea;
	case dae::Renderer::ShadingMode::Specular:
		return Phong(m_pSpecularTexture->Sample(v.uv).r, m_pGlossinessTexture->Sample(v.uv).r * m_Shininess, m_LightDirection, -v.viewDirection, v.normal) * observedArea;
	case dae::Renderer::ShadingMode::Combined:
		return (Lambert(7.f, m_pDiffuseTexture->Sample(v.uv)) + Phong(m_pSpecularTexture->Sample(v.uv).r, m_pGlossinessTexture->Sample(v.uv).r * m_Shininess, m_LightDirection, -v.viewDirection, v.normal) + m_Ambient) * observedArea;
	}

}

ColorRGB Renderer::Lambert(const float refectance, const ColorRGB color)
{
	return { (color * refectance) / PI };
}

ColorRGB Renderer::Phong(const float reflection, const float exponent, const Vector3& l, const Vector3& v, const Vector3& n)
{
	const float dot{ Vector3::Dot(n,l) };
	const Vector3 reflect{ l - (2 * dot * n) };
	const float cosAlpha{ std::max(Vector3::Dot(reflect, v), 0.f) };
	const float specular{ reflection * powf(cosAlpha, exponent) };
	return { specular, specular, specular };
}

bool Renderer::SaveBufferToImage() const
{
	return SDL_SaveBMP(m_pBackBuffer, "Rasterizer_ColorBuffer.bmp");
}

void Renderer::Render_W4()
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
			// check if triangle is inside frustum or cull
			if (IsOutsideFrustum(vertices[triangleIdx], vertices[triangleIdx + 1], vertices[triangleIdx + 2]))
			{
				continue;
			}

			// calc bounding box
			const int boundingBoxMargin{ 1 };
			boundingBoxTopLeft.first      = Clamp(int(std::min({ vertices[triangleIdx].position.x, vertices[triangleIdx + 1].position.x, vertices[triangleIdx + 2].position.x })) - boundingBoxMargin, 0, m_Width - 1);
			boundingBoxTopLeft.second     = Clamp(int(std::min({ vertices[triangleIdx].position.y, vertices[triangleIdx + 1].position.y, vertices[triangleIdx + 2].position.y })) - boundingBoxMargin, 0, m_Height - 1);
			boundingBoxBottomRight.first  = Clamp(int(std::max({ vertices[triangleIdx].position.x, vertices[triangleIdx + 1].position.x, vertices[triangleIdx + 2].position.x })) + boundingBoxMargin, 0, m_Width - 1);
			boundingBoxBottomRight.second = Clamp(int(std::max({ vertices[triangleIdx].position.y, vertices[triangleIdx + 1].position.y, vertices[triangleIdx + 2].position.y })) + boundingBoxMargin, 0, m_Height - 1);

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

						// check if pixel's depth value is smaller then stored one in depth buffer and inside [0,1] range
						const float interpolatedZDepth{ 1 / ((1 / vertices[triangleIdx + 0].position.z) * weights[0] +
															 (1 / vertices[triangleIdx + 1].position.z) * weights[1] +
															 (1 / vertices[triangleIdx + 2].position.z) * weights[2]) };

						if (interpolatedZDepth > 0.f && interpolatedZDepth < 1.f && interpolatedZDepth < m_pDepthBufferPixels[px + (py * m_Width)])
						{
							m_pDepthBufferPixels[px + (py * m_Width)] = interpolatedZDepth;

							if (m_showDepthBuffer)
							{
								float color = Remap(interpolatedZDepth, 0.995f, 1.f);
								finalColor = { color, color, color };
							}
							else finalColor = PixelShading(InterpolatedVertexAtrributes(vertices[triangleIdx + 0], vertices[triangleIdx + 1], vertices[triangleIdx + 2], weights));

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