#pragma once

namespace Dog
{
	class Animator;
	class Animation;
	class Model; // <- temporary, animation should not need model ideally.

	class AnimationLibrary
	{
	public:
		AnimationLibrary();
		~AnimationLibrary();

		uint32_t AddAnimation(const std::string& animPath, Model* model);
		Animation* GetAnimation(const std::string& modelPath, const std::string& animPath);
        Animation* GetAnimation(uint32_t index);
        Animator* GetAnimator(uint32_t index);
		uint32_t GetAnimationIndex(const std::string& modelPath, const std::string& animPath);
		const std::string& GetAnimationName(uint32_t index) const;
		const std::vector<VQS>& GetAnimationVQS();
		void UpdateAnimations(float dt);
		void UpdateAnimation(uint32_t index, float dt);

		uint32_t GetAnimationCount() const { return static_cast<uint32_t>(mAnimation.size()); }

		const static uint32_t INVALID_ANIMATION_INDEX;

	private:
		std::string GetKey(const std::string& modelPath, const std::string& animPath);

		friend class Model;

        std::vector<std::unique_ptr<Animation>> mAnimation;
        std::vector<std::unique_ptr<Animator>> mAnimators;

		// ModelName|AnimationName, Animation Index
		std::unordered_map<std::string, uint32_t> mAnimationMap; // Name to index
        std::vector<VQS> mAnimationVQS;
	};
}
