#pragma once

class FPerfTimer
{
	FCriticalSection MapMutex;
	TMap<FString, double> StartTimes;
	
public:
	void StartTimer(FString Key);

	double GetTimeElapsed(FString Key);

	double StopTimer(FString Key);
};
