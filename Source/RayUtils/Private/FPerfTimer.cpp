#include "FPerfTimer.h"

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
