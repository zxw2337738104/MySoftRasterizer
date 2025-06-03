#include "GameTime.h"

GameTime::GameTime() : mSecondsPerCount(0.0), mDeltaTime(-1.0), mBaseTime(0),
mPauseTime(0), mStopTime(0), mPrevTime(0), mCurrentTime(0), isStopped(false)
{
	__int64 countsPerSecond;
	QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSecond);
	mSecondsPerCount = 1.0 / (double(countsPerSecond));
}

void GameTime::Tick()
{
	if (isStopped)
	{
		mDeltaTime = 0.0;
		return;
	}
	__int64 currentTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currentTime);//返回计数器的值
	mCurrentTime = currentTime;

	mDeltaTime = (mCurrentTime - mPrevTime) * mSecondsPerCount;

	mPrevTime = mCurrentTime;

	if (mDeltaTime < 0)
	{
		mDeltaTime = 0;
	}
}

float GameTime::DeltaTime()const
{
	return (float)mDeltaTime;
}

void GameTime::Reset()
{
	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
	mBaseTime = currTime;
	mPrevTime = currTime;
	mStopTime = 0;
	isStopped = false;
}

void GameTime::Stop()
{
	if (!isStopped) {
		__int64 currTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
		mStopTime = currTime;
		isStopped = true;
	}
}

void GameTime::Start()
{
	__int64 startTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&startTime);
	if (isStopped)
	{
		mPauseTime = (startTime - mStopTime);
		mPrevTime = startTime;
		mStopTime = 0;
		isStopped = false;
	}
}

float GameTime::TotalTime()const
{
	if (isStopped)
	{
		return (float)((mStopTime - mPauseTime - mBaseTime) * mSecondsPerCount);
	}
	else
	{
		return (float)((mCurrentTime - mPauseTime - mBaseTime) * mSecondsPerCount);
	}
}

bool GameTime::IsStopped()
{
	return isStopped;
}