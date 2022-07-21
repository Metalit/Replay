#pragma once

#include "sombrero/shared/FastVector3.hpp"
#include "sombrero/shared/FastQuaternion.hpp"

using Vector3 = Sombrero::FastVector3;
using Quaternion = Sombrero::FastQuaternion;

#include <cmath>

// all credit to @fern [https://github.com/Fernthedev/]

static float ExpoEaseInOut(float t, float b, float c, float d) {
    if (t == 0.0f) return b;
    if (t == d) return b + c;
    if ((t /= d / 2.0f) < 1.0f) return c / 2.0f * (float) std::pow(2.0f, 10.0f * (t - 1.0f)) + b;
    return c / 2.0f * (-(float) std::pow(2.0f, -10.0f * --t) + 2.0f) + b;
}

static float EasedLerp(float from, float to, float time, float deltaTime) {
    return ExpoEaseInOut(time, from, to - from, deltaTime);
}

// This will make it so big movements are actually exponentially bigger while smaller ones are less
static Vector3 EaseLerp(Vector3 value1, Vector3 value2, float time, float deltaTime) {
    return Vector3(EasedLerp(value1.x, value2.x, time, deltaTime), EasedLerp(value1.y, value2.y, time, deltaTime), EasedLerp(value1.z, value2.z, time, deltaTime));
}

static Quaternion Slerp(Quaternion quaternion1, Quaternion quaternion2, float amount) {
    float num = quaternion1.x * quaternion2.x + quaternion1.y * quaternion2.y + quaternion1.z * quaternion2.z + quaternion1.w * quaternion2.w;
    bool flag = false;
    if (num < 0.0f) {
        flag = true;
        num = -num;
    }
    float num2;
    float num3;
    if (num > 0.999999f) {
        num2 = 1.0f - amount;
        num3 = (flag ? (-amount) : amount);
    } else {
        float num4 = std::acos(num);
        float num5 = 1.0f / std::sin(num4);
        num2 = std::sin((1.0f - amount) * num4) * num5;
        num3 = (flag ? (-std::sin(amount * num4) * num5) : (std::sin(amount * num4) * num5));
    }
    Quaternion result;
    result.x = num2 * quaternion1.x + num3 * quaternion2.x;
    result.y = num2 * quaternion1.y + num3 * quaternion2.y;
    result.z = num2 * quaternion1.z + num3 * quaternion2.z;
    result.w = num2 * quaternion1.w + num3 * quaternion2.w;
    return result;
}

// Changes the sign of the quaternion components. This is not the same as the inverse.
static inline Quaternion InverseSignQuaternion(Quaternion q) {
    return {-q.x, -q.y, -q.z, -q.w};
}

// math from https://stackoverflow.com/a/20249699
struct QuaternionAverage {
    public:
    QuaternionAverage(Quaternion baseRot) : baseRotation(baseRot) {}
    
    void AddRotation(Quaternion& rot) {
        //Before we add the new rotation to the average (mean), we have to check whether the quaternion has to be inverted. Because
        //q and -q are the same rotation, but cannot be averaged, we have to make sure they are all the same.
        if(Quaternion::Dot(rot, baseRotation) < 0) {
            rot = InverseSignQuaternion(rot);
        }
        
        num++;
        cumulative.w += rot.w;
        cumulative.x += rot.x;
        cumulative.y += rot.y;
        cumulative.z += rot.z;
    }

    Quaternion GetAverage() {
        // average values
        float avgMult = 1 / (float) num;
        float w = cumulative.w * avgMult;
        float x = cumulative.x * avgMult;
        float y = cumulative.y * avgMult;
        float z = cumulative.z * avgMult;

        // normalize
        float lengthD = 1 / (w*w + x*x + y*y + z*z);
        w *= lengthD;
        x *= lengthD;
        y *= lengthD;
        z *= lengthD;

        return Sombrero::QuaternionMultiply({x, y, z, w}, UnityEngine::Quaternion::Inverse(baseRotation));
    }

    private:
    Quaternion cumulative = {0, 0, 0, 0};
    int num = 0;
    const Quaternion baseRotation;
};
