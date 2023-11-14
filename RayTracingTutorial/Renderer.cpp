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

void Renderer::Render(const Scene& scene, const Camera& camera, glm::vec3 lightSourceCoords)
{
	Ray ray;
	ray.Origin = camera.GetPosition();

	for (uint32_t y = 0; y < m_FinalImage->GetHeight(); y++)
	{
		for (uint32_t x = 0; x < m_FinalImage->GetWidth(); x++)
		{
			ray.Direction = camera.GetRayDirections()[x + y * m_FinalImage->GetWidth()];

			glm::vec4 color = TraceRay(scene, ray, lightSourceCoords);
			color = glm::clamp(color, glm::vec4(0.0f), glm::vec4(1.0f));
			m_ImageData[x + y * m_FinalImage->GetWidth()] = Helpers::ConvertToABGR(color);
		}
	}

	m_FinalImage->SetData(m_ImageData);
}

glm::vec4 Renderer::TraceRay(const Scene& scene, const Ray& ray, glm::vec3 lightSourceCoords)
{
	// a = ray origin
	// b = ray direction
	// r = radius
	// t = hit distance

	if (scene.Spheres.size() == 0)
		return glm::vec4(0, 0, 0, 1);

	const Sphere* closestSphere = nullptr;
	float hitDistance = FLT_MAX;
	for (const Sphere& sphere : scene.Spheres)
	{
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
			closestSphere = &sphere;
		}
	}

	if (closestSphere == nullptr)
		return glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

	glm::vec3 tempRayOrigin = ray.Origin - closestSphere->Position;
	glm::vec3 hitPoint = tempRayOrigin + ray.Direction * hitDistance;
	glm::vec3 normal = glm::normalize(hitPoint);
	glm::vec3 lightDirection = glm::normalize(lightSourceCoords);

	float lightIntensity = glm::max(glm::dot(normal, -lightDirection), 0.0f);

	glm::vec3 sphereColor = closestSphere->Albedo;
	sphereColor *= lightIntensity; // normal * 0.5f + 0.5f;
	return glm::vec4(sphereColor, 1.0f);

}

// https://youtu.be/V5g9Mv9wLbY?si=JUxHE_sThS3oWUro&t=807