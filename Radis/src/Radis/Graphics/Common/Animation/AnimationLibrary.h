/*****************************************************************//**
 * \file   AnimationLibrary.h
 * \brief  Definition of the AnimationLibrary class for managing animation clips.
 *
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

namespace Radis
{
	class AnimationClip;

	class AnimationLibrary
	{
	public:
		AnimationLibrary();
		~AnimationLibrary();

		// Loads a cooked clip (.da) the first time it's asked for; later calls return the same
		// index. A clip that fails to load isn't tried again and gives INVALID_ANIMATION_INDEX.
		uint32_t AddClip(const std::string& path);
		const AnimationClip* GetClip(uint32_t index) const;
		uint32_t GetClipCount() const { return static_cast<uint32_t>(mClips.size()); }

		const static uint32_t INVALID_ANIMATION_INDEX;

	private:
		std::vector<std::unique_ptr<AnimationClip>> mClips;
		std::unordered_map<std::string, uint32_t> mClipIndices; // by path
	};
}
