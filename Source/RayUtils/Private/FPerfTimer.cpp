#include "FPerfTimer.h"


TMap<FString, double> FPerfTimer::StartTimes = TMap<FString, double>();
FCriticalSection FPerfTimer::MapMutex;

void FPerfTimer::StartTimer(FString Key)
{
	FScopeLock Lock(&MapMutex);
	StartTimes.Emplace(Key, FPlatformTime::Seconds());
}

double FPerfTimer::GetTimeElapsed(FString Key)
{
	FScopeLock Lock(&MapMutex);

	if (double* StartTimePtr = StartTimes.Find(Key))
	{
		return FPlatformTime::Seconds() - *StartTimePtr;
	}
	
	return -1.0;
}

double FPerfTimer::StopTimer(FString Key)
{
	FScopeLock Lock(&MapMutex);
	
	double StartTime = -1.0;
	if (StartTimes.RemoveAndCopyValue(Key, StartTime))
	{
		return FPlatformTime::Seconds() - StartTime;
	}
	
	return -1.0;
}
