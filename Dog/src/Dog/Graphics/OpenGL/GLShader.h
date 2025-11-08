#pragma once

namespace Dog {

    class GLShader
    {
    public:
        bool load(const std::string& path);
        bool load(const std::string& vertPath, const std::string& fragPath);

        // state
        unsigned int ID;
        static int CurrentID;
        // constructor
        GLShader() : ID(UINT32_MAX) {}
        ~GLShader();
        // sets the current GLShader as active
        GLShader& Use();
        // compiles the GLShader from given source code
        bool Compile(const char* vertexSource, const char* fragmentSource);
        void SetUniformsFromCode();
        // utility functions
        void SetFloat(const std::string& name, float value);
        void SetUnsigned(const std::string& name, unsigned int value);
        void SetInteger(const std::string& name, int value);
        void SetVector2f(const std::string& name, float x, float y);
        void SetVector2f(const std::string& name, const glm::vec2& value);
        void SetVector3f(const std::string& name, float x, float y, float z);
        void SetVector3f(const std::string& name, const glm::vec3& value);
        void SetVector4f(const std::string& name, float x, float y, float z, float w);
        void SetVector4f(const std::string& name, const glm::vec4& value);
        void SetMatrix4(const std::string& name, const glm::mat4& matrix);
        void SetUniformHandle(const std::string& name, GLuint64 handle);
        bool HasUniform(const std::string& name);

        // a global variable across all GLShaders
        static float iTime;

        // uniforms
        std::unordered_map<std::string, GLuint> Uniforms;

        // UBO for projection and view matrices, binding point 0
        static GLuint uboMatrices;
        static GLuint uboMatricesBindingPoint;
        static void SetupUBO();
        static void SetProjectionUBO(const glm::mat4& projection);
        static void SetViewUBO(const glm::mat4& view);
        static void SetProjectionViewUBO(const glm::mat4& projectionView);
        static void SetViewAndProjectionView(const glm::mat4& view, const glm::mat4& projectionView);
        static void SetCameraUBO(const struct CameraUniforms& uniformData);
        static void CleanupUBO();
        void BindUBO(const std::string& blockName, unsigned int bindingPoint);

        static void SetShader(std::shared_ptr<GLShader> shader) { activeShader = shader->Use(); }
        static void SetShader(GLShader& shader) { activeShader = shader.Use(); }
        static GLShader& GetActiveShader() { return activeShader; }

        static GLuint GetInstanceSSBO() { return instanceSSBO; }
        static void SetupInstanceSSBO();
        static GLuint GetAnimationSSBO() { return animationSSBO; }
        static void SetupAnimationSSBO();
        static GLuint GetTextureSSBO() { return textureSSBO; }
        static void SetupTextureSSBO();

    private:
        // checks if compilation or linking failed and if so, print the error logs
        bool checkCompileErrors(unsigned int object, std::string type);
        bool loadShaderFromFile(const std::string& shaderFile);

        static GLShader activeShader;
        static GLuint instanceSSBO;
        static GLuint animationSSBO;
        static GLuint textureSSBO;
    };

}