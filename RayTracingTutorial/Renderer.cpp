#include "Walnut/Random.h"
#include "Renderer.h"
#include "Scene.h"
#include <execution>

namespace Helpers
{
	static uint32_t ConvertToABGR(const glm::vec4& color)
	{
		uint8_t r = (uint8_t)(color.r * 255.0f);
		uint8_t g = (uint8_t)(color.g * 255.0f);
		uint8_t b = (uint8_t)(color.b * 255.0f);
		uint8_t a = (uint8_t)(color.a * 255.0f);

		uint32_t result = (a << 24) | (b << 16) | (g << 8) | r;
		return result;
	}

	static uint32_t PCG_Hash(uint32_t input)
	{
		uint32_t state = input * 747796405u + 2891336453u;
		uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
		return (word >> 22u) ^ word;
	}

	static float RandomFloatPcg(uint32_t& seed)
	{
		seed = PCG_Hash(seed);
		return (float)seed / (float)std::numeric_limits<uint32_t>::max();
	}

	static glm::vec3 InUnitSphere(uint32_t& seed)
	{
		return glm::normalize(glm::vec3(
			RandomFloatPcg(seed) * 2.0f - 1.0f,
			RandomFloatPcg(seed) * 2.0f - 1.0f,
			RandomFloatPcg(seed) * 2.0f - 1.0f)
		);
	}
}

void Renderer::OnResize(uint32_t width, uint32_t height)
{
	if (m_FinalImage)
	{
		// No resize necessary
		if (m_FinalImage->GetWidth() == width && m_FinalImage->GetHeight() == height)
			return;

		m_FinalImage->Resize(width, height);
	}
	else
	{
		m_FinalImage = std::make_shared<Walnut::Image>(width, height, Walnut::ImageFormat::RGBA);
	}

	delete[] m_ImageData;
	m_ImageData = new uint32_t[width * height];

	delete[] m_AccumulationData;
	m_AccumulationData = new glm::vec4[width * height];

	m_ImageHorizontalIter.resize(width);
	m_ImageVerticalIter.resize(height);

	for (uint32_t i = 0; i < width; i++)
		m_ImageHorizontalIter[i] = i;

	for (uint32_t i = 0; i < height; i++)
		m_ImageVerticalIter[i] = i;
}


void Renderer::Render(const Scene& scene, const Camera& camera)
{
	m_ActiveScene = &scene;
	m_ActiveCamera = &camera;

	if (m_FrameIndex == 1)
		memset(m_AccumulationData, 0, m_FinalImage->GetWidth() * m_FinalImage->GetHeight() * sizeof(glm::vec4));


	std::for_each(std::execution::par, m_ImageVerticalIter.begin(), m_ImageVerticalIter.end(),
		[this, scene](uint32_t y)
		{
			std::for_each(std::execution::par, m_ImageHorizontalIter.begin(), m_ImageHorizontalIter.end(),
			[this, y, scene](uint32_t x)
				{
					// TODO: implement a more generic PerPixel "shader" approach

					glm::vec4 color;
					color = PerPixel(x, y);

					m_AccumulationData[x + y * m_FinalImage->GetWidth()] += color;

					glm::vec4 accumulatedColor = m_AccumulationData[x + y * m_FinalImage->GetWidth()];
					accumulatedColor /= (float)m_FrameIndex;

					accumulatedColor = glm::clamp(accumulatedColor, glm::vec4(0.0f), glm::vec4(1.0f));
					m_ImageData[x + y * m_FinalImage->GetWidth()] = Helpers::ConvertToABGR(accumulatedColor);
				});
		});

	m_FinalImage->SetData(m_ImageData);

	if (m_Settings.Accumulate)
		m_FrameIndex++;
	else
		m_FrameIndex = 1;
}

glm::vec4 Renderer::PerPixel(uint32_t x, uint32_t y)
{
	Ray ray;
	ray.Origin = m_ActiveCamera->GetPosition();
	ray.Direction = m_ActiveCamera->GetRayDirections()[x + y * m_FinalImage->GetWidth()];

	glm::vec3 light = glm::vec3(0.0f);
	glm::vec3 lightContribution(1.0f);

	uint32_t seed = x + y * m_FinalImage->GetWidth();
	seed *= m_FrameIndex;

	int bounces = 5;
	for (int i = 0; i < bounces; i++)
	{
		seed += i;

		Renderer::HitPayload payload = TraceRay();
		if (payload.HitDistance < 0.0f)
		{
			break;
		}

		const Sphere& sphere = m_ActiveScene->Spheres[payload.objectIndex];
		const Material& material = m_ActiveScene->Materials[sphere.MaterialIndex];

		lightContribution *= material.Albedo;
		light += material.GetEmission();

		ray.Origin = payload.WorldPosition + payload.WorldNormal * 0.0001f;

		ray.Direction = glm::normalize(payload.WorldNormal + material.Roughness * Helpers::InUnitSphere(seed));
	}

	return glm::vec4(light, 1.0f);
}

Renderer::HitPayload Renderer::TraceRay(const Scene& scene, const Ray& ray) {

	if (scene.Models.size() == 0)
		return Miss(ray);

	const Triangle* closestTriangle = nullptr;
	float hitDistance = std::numeric_limits<float>::max();


	for (const Model* model : scene.Models) {
		for (const Triangle* triangle : model->m_triangles)
		{
			glm::vec3 origin = ray.Origin - model->Position;
			float t = (glm::dot(triangle->Normal, triangle->A - origin)) / glm::dot(triangle->Normal, ray.Direction);
			glm::vec3 Q = origin + t * ray.Direction;

			if (
				t > 0.0f &&
				t < hitDistance &&
				glm::dot(glm::cross(triangle->B - triangle->A, Q - triangle->A), triangle->Normal) >= 0 &&
				glm::dot(glm::cross(triangle->C - triangle->B, Q - triangle->B), triangle->Normal) >= 0 &&
				glm::dot(glm::cross(triangle->A - triangle->C, Q - triangle->C), triangle->Normal) >= 0
				)
			{
				hitDistance = t;
				closestTriangle = triangle;
			}
		}
	}

	if (closestTriangle == nullptr)
		return Miss(ray);

	glm::vec3 lightDir = glm::normalize(glm::vec3(-0.8, -0.7, -0.9));
	float lightIntensity = glm::dot(closestTriangle->Normal, -lightDir);

	return ClosestHitModel();
}

Renderer::HitPayload Renderer::ClosestHit(const Ray& ray, float hitDistance, Model* model, Triangle* triangle)
{
	Renderer::HitPayload payload;
	payload.HitDistance = hitDistance;
	payload.Model = model;

	const Model* closestModel = m_ActiveScene->Models[objectIndex];

	glm::vec3 tempRayOrigin = ray.Origin - closestModel->Position;
	payload.WorldPosition = tempRayOrigin + ray.Direction * hitDistance;
	payload.WorldNormal = glm::normalize(payload.WorldPosition);

	payload.WorldPosition += closestModel->Position;

	return payload;
}

Renderer::HitPayload Renderer::Miss(const Ray& ray)
{
	Renderer::HitPayload payload;
	payload.HitDistance = -1.0f;
	return payload;
}