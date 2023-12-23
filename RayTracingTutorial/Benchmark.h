#pragma once
#include <cstdint>


class Benchmark
{


public:
	void ResetAverage();
	void CalculateAverageRenderTime(float lastRenderTime);
	float GetAverageRenderTime() { return m_averageRenderTime; }

private:
	float m_averageRenderTime = 0.0f;
	uint32_t m_RenderIterations = 0;
	float m_TotalRenderTime = 0;
};