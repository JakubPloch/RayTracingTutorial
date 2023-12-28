#include "Walnut/Application.h"
#include "Walnut/EntryPoint.h"

#include "Walnut/Image.h"
#include "Walnut/Random.h"
#include "Walnut/Timer.h"
#include "../Renderer.h"
#include "../Camera.h"
#include "../Benchmark.h"

#include <glm/gtc/type_ptr.hpp>

using namespace Walnut;

class ExampleLayer : public Walnut::Layer
{
public:
	ExampleLayer()
		: m_Camera(45.0f, 0.1f, 100.f)
	{
		m_Scene.BackgroundColor = glm::vec3(0.6f, 0.7f, 0.9f);

		Material& pinkMaterial = m_Scene.Materials.emplace_back();
		pinkMaterial.Albedo = { 1.0f, 0.0f, 1.0f };
		pinkMaterial.Roughness = 0.3f;

		Material& blueMaterial = m_Scene.Materials.emplace_back();
		blueMaterial.Albedo = { 0.2f, 0.3f, 1.0f };
		blueMaterial.Roughness = 0.9f;

		Material& orangeMaterial = m_Scene.Materials.emplace_back();
		orangeMaterial.Albedo = { 0.8f, 0.5f, 0.2f };
		orangeMaterial.Roughness = 0.1f;
		orangeMaterial.EmissionColor = orangeMaterial.Albedo;
		orangeMaterial.EmissionPower = 2.0f;

		{
			Model* model = new Model();
			model->LoadFromOBJ("models/cube.obj");
			model->Position = glm::vec3{ 2.0f, 0.0f, 0.0f };
			model->m_materialIndex = 2;
			m_Scene.Models.push_back(model);
		}

	}
	virtual void OnUpdate(float ts) override
	{
		if (m_Camera.OnUpdate(ts))
		{
			m_Renderer.ResetFrameIndex();
			m_benchmark.ResetAverage();
		}
	}

	virtual void OnUIRender() override
	{
		ImGui::Begin("Settings");
		ImGui::Text("Last render: %.3fms", m_LastRenderTime);
		ImGui::Text("Average render: %.3fms", m_benchmark.GetAverageRenderTime());
		if (ImGui::Button("Render"))
		{
			Render();
		}

		ImGui::Checkbox("Accumulate", &m_Renderer.GetSettings().Accumulate);

		if (ImGui::Button("Reset"))
		{
			m_Renderer.ResetFrameIndex();
			m_benchmark.ResetAverage();
		}

		ImGui::Separator();

		ImGui::End();

		ImGui::Begin("Scene");

		ImGui::Text("Background color:");
		ImGui::ColorEdit3("Background color", glm::value_ptr(m_Scene.BackgroundColor));
		ImGui::Separator();

		for (size_t i = 0; i < m_Scene.Materials.size(); i++)
		{
			ImGui::PushID(i);

			Material& material = m_Scene.Materials[i];
			ImGui::ColorEdit3("Albedo", glm::value_ptr(material.Albedo));
			ImGui::DragFloat("Roughness", &material.Roughness, 0.05f, 0.0f, 1.0f);
			ImGui::DragFloat("Metallic", &material.Metallic, 0.05f, 0.0f, 1.0f);
			ImGui::ColorEdit3("Emission Color", glm::value_ptr(material.EmissionColor));
			ImGui::DragFloat("Emission Power", &material.EmissionPower, 0.05f, 0.0f, FLT_MAX);

			ImGui::Separator();
			ImGui::PopID();
		}

		ImGui::End();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("Viewport");

		m_ViewportWidth = ImGui::GetContentRegionAvail().x;
		m_ViewportHeight = ImGui::GetContentRegionAvail().y;

		auto image = m_Renderer.GetFinalImage();
		if (image)
		{
			ImGui::Image(image->GetDescriptorSet(), { (float)image->GetWidth(), (float)image->GetHeight() },
				ImVec2(0, 1), ImVec2(1, 0));
		}

		ImGui::End();
		ImGui::PopStyleVar();

		Render();
	}

	void Render()
	{
		Timer timer;

		m_Renderer.OnResize(m_ViewportWidth, m_ViewportHeight);
		m_Camera.OnResize(m_ViewportWidth, m_ViewportHeight);
		m_Renderer.Render(m_Scene, m_Camera);

		m_LastRenderTime = timer.ElapsedMillis();

		m_benchmark.CalculateAverageRenderTime(m_LastRenderTime);

	}
private:
	Renderer m_Renderer;
	Benchmark m_benchmark;
	Camera m_Camera;
	Scene m_Scene;
	uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;
	float m_LastRenderTime = 0.0f;

};

Walnut::Application* Walnut::CreateApplication(int argc, char** argv)
{
	Walnut::ApplicationSpecification spec;
	spec.Name = "My Window";

	Walnut::Application* app = new Walnut::Application(spec);
	app->PushLayer<ExampleLayer>();
	app->SetMenubarCallback([app]()
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Exit"))
				{
					app->Close();
				}
				ImGui::EndMenu();
			}
		});
	return app;
}