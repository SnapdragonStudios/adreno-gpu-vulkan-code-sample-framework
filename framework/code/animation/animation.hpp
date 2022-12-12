//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include "animationData.hpp"
#include <unordered_map>
#include "tcb/span.hpp"

class Skeleton;

/// @brief Animation class
/// @ingroup Animation
/// Contains an animation data (for a single animation on one or more nodes) and provides accessors to the animation data
class Animation
{
    Animation(const Animation&) = delete;
    Animation& operator=(const Animation&) = delete;
public:
    Animation(AnimationData&& d);
    Animation(Animation&& animation) = default;
    ~Animation();

    /// @brief Calculate the animated position for a given animation node. Will lerp value for 'missing' timesteps
    /// @param nodeIdx animation node index (not model nodeId)
    /// @param time to calculate the animation data for
    /// @param startFrameIdx hint of which animation frame may be the 'start' of the time span containing 'time'.  If unknown pass 0.  Updated frame index is returned. Passing in this hint can be a substantial speed-up (otherwise code will potentially search through all animation frames) 
    /// @return Calculated/interpolated position.
    glm::vec3 CalcLocalTranslation(uint32_t animationNodeIdx/*not nodeId*/, float time, uint32_t& startFrameIdx) const;
    glm::quat CalcLocalRotation(uint32_t animationNodeIdx/*not nodeId*/, float time, uint32_t& startFrameIdx) const;
    glm::vec3 CalcLocalScale(uint32_t animationNodeIdx/*not nodeId*/, float time, uint32_t& startFrameIdx) const;

    const auto& GetAnimationData() const { return m_AnimationData; }
    float GetEndTime() const { return m_EndTime; }

protected:
    AnimationData m_AnimationData;
    float m_EndTime = 0.0f;
};

struct AnimationNodeRef
{
    const Animation* animation;
    uint32_t nodeIndex;             // node index within this anim
    operator bool() const { return animation != nullptr; }
};


struct AnimationNodeIterator
{
    uint32_t frameIdx;  // hint at frame index
};
struct AnimationIterator
{
    const Animation& animation;
    float time = 0.0f;
    std::vector<AnimationNodeIterator>  nodeIterators;
};


class AnimationList
{
    AnimationList(const AnimationList&) = delete;
    AnimationList& operator=(const AnimationList&) = delete;
public:
    AnimationList();
    AnimationList(std::vector<AnimationData>&& data);
    AnimationList& operator=(AnimationList&&) noexcept;

    ~AnimationList();

    /// @brief Find animation for a given (gltf) node id.
    /// @return pair with pointer to animation and the node's index within the animation!  nullptr,0 if animation not found
    AnimationNodeRef FindNodeAnimation(int NodeId) const;

    const auto& GetAnimations() const { return m_Animations; }

    //glm::vec3 CalcLocalTranslation(const AnimationNodeRef& ref, float time) const { return ref.animation->CalcLocalTranslation(ref.nodeIndex, time); }
    //glm::quat CalcLocalRotation(const AnimationNodeRef& ref, float time) const { return ref.animation->CalcLocalRotation(ref.nodeIndex, time); }
    //glm::vec3 CalcLocalScale(const AnimationNodeRef& ref, float time) const { return ref.animation->CalcLocalScale(ref.nodeIndex, time); }

    /// @brief Update the given time value, wrapping to the end of the animation.
    /// @param currentTime time to update (add currentTime and wrap if after end of animation timeline)
    /// @param elapsedTime time since last call (generally frame time)
    float StepTime(const AnimationNodeRef& , float currentTime, float elapsedTime) const;

    /// @brief Update the given animation iterator, wrapping to the end of the animation.
    /// @param iter iterator to update
    /// @param elapsedTime time since last call (generally frame time)
    void StepTime(AnimationIterator& iter, float elapsedTime) const;

    /// @brief Create an allocation iterator
    /// @param Animation we want to iterate on
    AnimationIterator MakeIterator(const Animation&);

    /// Recalculate matrix array
    /// @param nodeMatrixs array of matrixes we want to update (indexed by nodeId)
    static void UpdateSkeletonMatrixes( const Skeleton&, AnimationIterator& iterator, tcb::span<glm::mat3x4> nodeMatrixs );

protected:
    std::vector<Animation> m_Animations;
    std::unordered_map<uint32_t, AnimationNodeRef> m_AnimationNodeMap;   // NodeId being animated mapped to Animation (that moves the node) (points into m_Animations) and the index of the node within the animation's data.
};
