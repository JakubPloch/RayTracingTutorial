#include "Benchmark.h"


void Benchmark::ResetAverage()
{
	m_averageRenderTime = 0.0f;
	m_RenderIterations = 0;
	m_TotalRenderTime = 0;
}

void Benchmark::CalculateAverageRenderTime(float lastRenderTime)
{
	m_TotalRenderTime += lastRenderTime;
	m_RenderIterations++;
	m_averageRenderTime = m_TotalRenderTime / m_RenderIterations;
}
