#pragma once

namespace Dog
{
    struct BoneInfo
    {
        int id;           // index into finalBoneMatrices
        VQS vqsOffset;

        BoneInfo() : id(0), vqsOffset() {}
        BoneInfo(int ID, VQS vqs) : id(ID), vqsOffset(vqs) {}
    };

    struct KeyPosition
    {
        glm::vec3 position;
        float time;

        KeyPosition() : position(0.0f), time(0.0f) {}
        KeyPosition(const glm::vec3& pos, float timeStamp) : position(pos), time(timeStamp) {}
    };

    struct KeyRotation
    {
        glm::quat orientation;
        float time;

        KeyRotation() : orientation(1.0f, 0.0f, 0.0f, 0.0f), time(0.0f) {}
        KeyRotation(const glm::quat& ori, float timeStamp) : orientation(ori), time(timeStamp) {}
    };

    struct KeyScale
    {
        glm::vec3 scale;
        float time;

        KeyScale() : scale(1.0f), time(0.0f) {}
        KeyScale(const glm::vec3& scale, float timeStamp) : scale(scale), time(timeStamp) {}
    };

  class Bone
  {
  public:
    // Reads bone keyframe data from the aiNodeAnim
    Bone();
    Bone(int ID);
    Bone(int ID, const aiNodeAnim* channel, const std::string& debugName = "");

    void Update(float animationTime);

    inline const VQS& GetLocalTransform() const noexcept { return mLocalTransform; }

    int GetBoneID() const { return mID; }

    const std::vector<KeyPosition>& GetPositionKeys() const { return mPositions; }
    const std::vector<KeyRotation>& GetRotationKeys() const { return mRotations; }
    const std::vector<KeyScale>& GetScalingKeys() const { return mScales; }

  private:
    friend class Animator;

    float GetScaleFactor(float lastTime, float nextTime, float animationTime) const;
    void InterpolatePosition(float animationTime);
    void InterpolateRotation(float animationTime);
    void InterpolateScaling(float animationTime);

  private:
    std::vector<KeyPosition> mPositions;
    std::vector<KeyRotation> mRotations;
    std::vector<KeyScale> mScales;
    std::string debugName;

    VQS mLocalTransform;
    int mID;
  };
}
