#pragma once

class RAYUTILS_API FPerfTimer
{
public:
	static FCriticalSection MapMutex;

	static TMap<FString, double> StartTimes;

	static void StartTimer(FString Key);

	static double GetTimeElapsed(FString Key);

	static double StopTimer(FString Key);
};
