#pragma once
#include <Windows.h>

class GameTime
{
public:
	GameTime();

	float TotalTime()const;//游戏运行的总时间
	float DeltaTime()const;
	bool IsStopped();

	void Reset();//重置计时器
	void Start();//开始计时器
	void Stop();//停止计时器
	void Tick();//计算每帧时间间隔

private:
	double mSecondsPerCount;//计数器每一下多少秒
	double mDeltaTime;//每帧时间

	__int64 mBaseTime;//重置后的基准时间
	__int64 mPauseTime;//暂停的总时间
	__int64 mStopTime;//停止时刻的时间
	__int64 mPrevTime;//上一帧的时间
	__int64 mCurrentTime;//当前帧时间

	bool isStopped;//是否为停止状态
};
