#include "Walnut/Random.h"
#include "Renderer.h"
#include "Scene.h"

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
}

void Renderer::Render(const Scene& scene, const Camera& camera)
{
	m_ActiveScene = &scene;
	m_ActiveCamera = &camera;

	for (uint32_t y = 0; y < m_FinalImage->GetHeight(); y++)
	{
		for (uint32_t x = 0; x < m_FinalImage->GetWidth(); x++)
		{
			glm::vec4 color = PerPixel(x, y, scene.lightSourceCoords);
			color = glm::clamp(color, glm::vec4(0.0f), glm::vec4(1.0f));
			m_ImageData[x + y * m_FinalImage->GetWidth()] = Helpers::ConvertToABGR(color);
		}
	}

	m_FinalImage->SetData(m_ImageData);
}

glm::vec4 Renderer::PerPixel(uint32_t x, uint32_t y, glm::vec3 lightSourceCoords)
{
	Ray ray;
	ray.Origin = m_ActiveCamera->GetPosition();
	ray.Direction = m_ActiveCamera->GetRayDirections()[x + y * m_FinalImage->GetWidth()];
	Renderer::HitPayload payload = TraceRay(ray);
	if (payload.HitDistance < 0.0f)
		return glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

	glm::vec3 lightDirection = glm::normalize(lightSourceCoords);
	float lightIntensity = glm::max(glm::dot(payload.WorldNormal, -lightDirection), 0.0f);

	const Sphere& sphere = m_ActiveScene->Spheres[payload.objectIndex];

	glm::vec3 sphereColor = sphere.Albedo;
	sphereColor *= lightIntensity; // normal * 0.5f + 0.5f;
	return glm::vec4(sphereColor, 1.0f);
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

		if (closerHitDistance < hitDistance)
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

// https://youtu.be/bMTyIEXcZjY?si=k9HSIDcm5fYKM1x2&t=1397