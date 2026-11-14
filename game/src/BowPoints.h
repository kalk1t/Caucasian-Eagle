// Written by art/nyxara/blender/assemble.py (bow_frames). Do not edit: change the pose there and build her again.
// Where her string hand, the bow's grip and the two ends of its string are in each frame of the BowShot clip
// (30 frames a second), in her own frame: x to her right, y up, z ahead, metres from her feet.
#pragma once

namespace CaucasianEagle
{
    struct BowFrame { float Hand[3], Grip[3], Top[3], Bottom[3]; bool Nocked; };
    constexpr int BowFrameCount = 7;
    constexpr float BowFrameRate = 30.f;
    constexpr BowFrame BowFrames[BowFrameCount] = {
        {{0.2498f, 1.3090f, 0.2128f}, {0.2249f, 1.3015f, 0.7726f}, {0.2249f, 1.8057f, 0.6398f}, {0.2249f, 0.9606f, 0.5764f}, true},
        {{0.2904f, 1.3315f, 0.1306f}, {0.2205f, 1.3015f, 0.7725f}, {0.2205f, 1.8057f, 0.6397f}, {0.2205f, 0.9606f, 0.5763f}, false},
        {{0.2878f, 1.2943f, 0.3491f}, {0.2216f, 1.3015f, 0.7726f}, {0.2216f, 1.8057f, 0.6398f}, {0.2216f, 0.9606f, 0.5765f}, false},
        {{0.2291f, 1.2894f, 0.4868f}, {0.2239f, 1.3015f, 0.7727f}, {0.2239f, 1.8057f, 0.6399f}, {0.2239f, 0.9606f, 0.5765f}, true},
        {{0.2499f, 1.3090f, 0.4744f}, {0.2249f, 1.3015f, 0.7726f}, {0.2249f, 1.8057f, 0.6398f}, {0.2249f, 0.9606f, 0.5764f}, true},
        {{0.2499f, 1.3090f, 0.3128f}, {0.2249f, 1.3015f, 0.7726f}, {0.2249f, 1.8057f, 0.6398f}, {0.2249f, 0.9606f, 0.5764f}, true},
        {{0.2498f, 1.3090f, 0.2128f}, {0.2249f, 1.3015f, 0.7726f}, {0.2249f, 1.8057f, 0.6398f}, {0.2249f, 0.9606f, 0.5764f}, true},
    };
}
