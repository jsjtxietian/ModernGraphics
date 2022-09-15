#pragma once

#include <assert.h>
#include <stdio.h>

class FramesPerSecondCounter
{
public:
    explicit FramesPerSecondCounter(float avgInterval = 0.5f)
        : avgInterval_(avgInterval)
    {
        assert(avgInterval > 0.0f);
    }

    // accepts the time elapsed since the previous call and a Boolean flag that should be set to true if a new
    // frame has been rendered during this iteration. This flag is a convenience feature to
    // handle situations when frame rendering can be skipped in the main loop for various
    // reasons. The time is accumulated until it reaches the value of avgInterval_
    bool tick(float deltaSeconds, bool frameRendered = true)
    {
        if (frameRendered)
            numFrames_++;

        accumulatedTime_ += deltaSeconds;

        // do averaging, update the current FPS value, and print debug info to the console
        if (accumulatedTime_ > avgInterval_)
        {
            currentFPS_ = static_cast<float>(numFrames_ / accumulatedTime_);
            if (printFPS_)
                printf("FPS: %.1f\n", currentFPS_);
            numFrames_ = 0;
            accumulatedTime_ = 0;
            return true;
        }

        return false;
    }

    inline float getFPS() const { return currentFPS_; }

    bool printFPS_ = true;

private:
    // store the duration of a sliding window, the number of
    // frames rendered in the current interval, and the accumulated time of this interval
    const float avgInterval_ = 0.5f;
    unsigned int numFrames_ = 0;
    double accumulatedTime_ = 0;
    float currentFPS_ = 0.0f;
};
