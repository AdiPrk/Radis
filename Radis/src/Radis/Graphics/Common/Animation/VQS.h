#pragma once

namespace Dog
{
    struct VQS 
    {
        glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };

        glm::vec3 translation{ 0.0f };
        float __padding1{ 0 }; // 16-byte alignment padding
        
        glm::vec3 scale{ 1.0f };
        float __padding2{ 0 }; // 16-byte alignment padding

        // Default ctor
        VQS() = default;

        // Ctor from mat4
        VQS(const glm::mat4& matrix) {
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::decompose(matrix, this->scale, this->rotation, this->translation, skew, perspective);
        }

        // Overload the assignment operator (=) to decompose a glm::mat4
        VQS& operator=(const glm::mat4& matrix) {
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::decompose(matrix, this->scale, this->rotation, this->translation, skew, perspective);
            return *this;
        }

        // Overload the multiplication operator (*) to compose with another VQS
        VQS operator*(const VQS& local) const {
            VQS result;
            result.scale = this->scale * local.scale;
            result.rotation = this->rotation * local.rotation;

            glm::vec3 scaledLocalTrans = this->scale * local.translation;
            glm::vec3 rotatedLocalTrans = this->rotation * scaledLocalTrans;
            result.translation = this->translation + rotatedLocalTrans;

            return result;
        }
    };
}