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
					if (scene.GlobalDirectionalLight.IsEnabled)
					{
						color = PerPixelEvenlyLit(x, y, scene.GlobalDirectionalLight);
					}
					else
					{
						color = PerPixel(x, y);
					}

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

glm::vec4 Renderer::PerPixelEvenlyLit(uint32_t x, uint32_t y, DirectionalLight directionalLight)
{
	Ray ray;
	ray.Origin = m_ActiveCamera->GetPosition();
	ray.Direction = m_ActiveCamera->GetRayDirections()[x + y * m_FinalImage->GetWidth()];

	glm::vec3 light = glm::vec3(0.0f);
	glm::vec3 lightContribution(1.0f);

	int bounces = 5;
	for (int i = 0; i < bounces; i++)
	{
		Renderer::HitPayload payload = TraceRay(ray);
		if (payload.HitDistance < 0.0f)
		{
			break;
		}

		const Sphere& sphere = m_ActiveScene->Spheres[payload.objectIndex];
		const Material& material = m_ActiveScene->Materials[sphere.MaterialIndex];
		glm::vec3 sphereColor = material.Albedo;

		// Not physically accurate - evenly lights each object
		light += m_ActiveScene->BackgroundColor * lightContribution;
		glm::vec3 lightDirection = glm::normalize(directionalLight.LightSourceCoords);
		float lightIntensity = glm::max(glm::dot(payload.WorldNormal, -lightDirection), 0.0f);
		sphereColor *= lightIntensity; // normal * 0.5f + 0.5f;

		lightContribution *= material.Albedo;
		light += material.GetEmission();

		ray.Origin = payload.WorldPosition + payload.WorldNormal * 0.0001f;
		ray.Direction = glm::normalize(payload.WorldNormal + material.Roughness * Walnut::Random::InUnitSphere());
	}

	return glm::vec4(light, 1.0f);
}

glm::vec4 Renderer::PerPixel(uint32_t x, uint32_t y)
{
	Ray ray;
	ray.Origin = m_ActiveCamera->GetPosition();
	ray.Direction = m_ActiveCamera->GetRayDirections()[x + y * m_FinalImage->GetWidth()];

	glm::vec3 light = glm::vec3(0.0f);
	glm::vec3 lightContribution(1.0f);

	int bounces = 5;
	for (int i = 0; i < bounces; i++)
	{
		Renderer::HitPayload payload = TraceRay(ray);
		if (payload.HitDistance < 0.0f)
		{
			break;
		}

		const Sphere& sphere = m_ActiveScene->Spheres[payload.objectIndex];
		const Material& material = m_ActiveScene->Materials[sphere.MaterialIndex];
		glm::vec3 sphereColor = material.Albedo;

		lightContribution *= material.Albedo;
		light += material.GetEmission();

		ray.Origin = payload.WorldPosition + payload.WorldNormal * 0.0001f;
		ray.Direction = glm::normalize(payload.WorldNormal + material.Roughness * Walnut::Random::InUnitSphere());
	}

	return glm::vec4(light, 1.0f);
}

Renderer::HitPayload Renderer::TraceRay(const Ray& ray)
{
	// a = ray origin
	// b = ray direction
	// r = radius
	// t = hit distance

	int closestSphere = -1;
	float hitDistance = FLT_MAX;

	for (size_t i = 0; i < m_ActiveScene->Spheres.size(); i++)
	{
		const Sphere& sphere = m_ActiveScene->Spheres[i];
		glm::vec3 tempRayOrigin = ray.Origin - sphere.Position;

		float a = glm::dot(ray.Direction, ray.Direction);
		float b = 2.0f * glm::dot(tempRayOrigin, ray.Direction);
		float c = glm::dot(tempRayOrigin, tempRayOrigin) - sphere.Radius * sphere.Radius;

		float discriminant = b * b - 4.0f * a * c;

		if (discriminant <= 0.0f)
			continue;

		float closerHitDistance = (-b - glm::sqrt(discriminant)) / (2.0f * a);

		if (closerHitDistance > 0 && closerHitDistance < hitDistance)
		{
			hitDistance = closerHitDistance;
			closestSphere = (int)i;
		}
	}

	if (closestSphere < 0)
		return Miss(ray);

	return ClosestHit(ray, hitDistance, closestSphere);

}


Renderer::HitPayload Renderer::ClosestHit(const Ray& ray, float hitDistance, int objectIndex)
{
	Renderer::HitPayload payload;
	payload.HitDistance = hitDistance;
	payload.objectIndex = objectIndex;

	const Sphere& closestSphere = m_ActiveScene->Spheres[objectIndex];

	glm::vec3 tempRayOrigin = ray.Origin - closestSphere.Position;
	payload.WorldPosition = tempRayOrigin + ray.Direction * hitDistance;
	payload.WorldNormal = glm::normalize(payload.WorldPosition);

	payload.WorldPosition += closestSphere.Position;

	return payload;
}

Renderer::HitPayload Renderer::Miss(const Ray& ray)
{
	Renderer::HitPayload payload;
	payload.HitDistance = -1.0f;
	return payload;
}