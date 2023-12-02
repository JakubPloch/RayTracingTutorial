#pragma once

#include <glm/glm.hpp>
#include <vector>

struct Material
{
	glm::vec3 Albedo{ 1.0f };
	float Roughness = 1.0f;
	float Metallic = 0.0f;
};

struct Sphere
{
	glm::vec3 Position{ 0.0f };
	float Radius = 0.5f;
	Material Mat;
};

struct Scene
{
	std::vector<Sphere> Spheres;
	glm::vec3 LightSourceCoords;
	glm::vec3 BackgroundColor;
};