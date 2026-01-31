/*****************************************************************//**
 * \file   Components.cpp
 * \brief  Implements component functionality
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#include <PCH/pch.h>
#include "Components.h"

namespace Radis 
{
	void TransformComponent::SetTranslation(float x, float y, float z)
	{
        Translation = glm::vec3(x, y, z);
        mIsDirty = true;
	}

	void TransformComponent::SetTranslation(const glm::vec3& tr)
	{
		Translation = tr;
        mIsDirty = true;
	}

	void TransformComponent::SetRotation(float x, float y, float z)
	{
		Rotation = glm::vec3(x, y, z);
        mIsDirty = true;
	}
	
	void TransformComponent::SetRotation(const glm::vec3& rot)
	{
		Rotation = rot;
        mIsDirty = true;
	}
	
	void TransformComponent::SetScale(float x, float y, float z)
	{
		Scale = glm::vec3(x, y, z);
        mIsDirty = true;
	}
	
	void TransformComponent::SetScale(const glm::vec3& scale)
	{
		Scale = scale;
        mIsDirty = true;
	}

	void TransformComponent::SetScale(float s)
	{
		Scale = glm::vec3(s, s, s);
        mIsDirty = true;
	}

	glm::mat4 TransformComponent::GetTransform()
	{
		if (!mIsDirty) return mCachedTransform;

		mIsDirty = false;
		mCachedTransform = glm::translate(glm::mat4(1.0f), Translation) * glm::toMat4(glm::quat(Rotation)) * glm::scale(glm::mat4(1.0f), Scale);

		return mCachedTransform;
	}

	glm::mat3 TransformComponent::GetNormalMatrix() const {
		const float c3 = glm::cos(Rotation.z);
		const float s3 = glm::sin(Rotation.z);
		const float c2 = glm::cos(Rotation.x);
		const float s2 = glm::sin(Rotation.x);
		const float c1 = glm::cos(Rotation.y);
		const float s1 = glm::sin(Rotation.y);
		const glm::vec3 invScale = 1.0f / Scale;

		return glm::mat3{
			{
				invScale.x * (c1 * c3 + s1 * s2 * s3),
				invScale.x * (c2 * s3),
				invScale.x * (c1 * s2 * s3 - c3 * s1),
			},
			{
				invScale.y * (c3 * s1 * s2 - c1 * s3),
				invScale.y * (c2 * c3),
				invScale.y * (c1 * c3 * s2 + s1 * s3),
			},
			{
				invScale.z * (c2 * s1),
				invScale.z * (-s2),
				invScale.z * (c1 * c2),
			},
		};
	}

} // namespace Radis
