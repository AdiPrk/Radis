#include <PCH/pch.h>
#include "GLShader.h"
#include <fstream>
#include <sstream>

#include "Graphics/Vulkan/Uniform/ShaderTypes.h"

namespace Dog {

    float GLShader::iTime = 0.0f;
    GLuint GLShader::uboMatrices = 0;
    GLuint GLShader::uboMatricesBindingPoint = 0;
    GLuint GLShader::instanceVBO = 0;

    int GLShader::CurrentID = 0;
    GLShader GLShader::activeShader = GLShader();

    bool GLShader::load(const std::string& path)
    {
        // check if those files exist
        std::ifstream sFile(path + ".glsl");
        if (!sFile.good()) {
            DOG_CRITICAL("GLShader files not found: {0}", (path + ".glsl"));

            return false;
        }

        if (!loadShaderFromFile(path)) {
            return false;
        }

        // shader.SetUniformsFromCode(); // - an optimization which is really not worth completing for now.

        BindUBO("Matricies", GLShader::uboMatricesBindingPoint);

        return true;
    }

    bool GLShader::load(const std::string& vertPath, const std::string& fragPath)
    {
        // --- 1. Verify both files exist ---
        std::ifstream vertFile(vertPath);
        std::ifstream fragFile(fragPath);
        if (!vertFile.good() || !fragFile.good())
        {
            DOG_CRITICAL("GLShader files not found:\n  Vert: {0}\n  Frag: {1}", vertPath, fragPath);
            return false;
        }

        // --- 2. Read vertex shader ---
        std::stringstream vertStream;
        vertStream << vertFile.rdbuf();
        std::string vertexCode = vertStream.str();

        // --- 3. Read fragment shader ---
        std::stringstream fragStream;
        fragStream << fragFile.rdbuf();
        std::string fragmentCode = fragStream.str();

        vertFile.close();
        fragFile.close();

        const char* vShaderCode = vertexCode.c_str();
        const char* fShaderCode = fragmentCode.c_str();

        // --- 4. Compile and link ---
        if (!Compile(vShaderCode, fShaderCode))
        {
            DOG_CRITICAL("Failed to compile shader:\n  Vert: {0}\n  Frag: {1}", vertPath, fragPath);
            return false;
        }

        // --- 5. Bind standard UBOs ---
        BindUBO("Matricies", GLShader::uboMatricesBindingPoint);

        return true;
    }

    bool GLShader::loadShaderFromFile(const std::string& shaderFile)
    {
        // 1. retrieve the vertex/fragment source code from filePath
        std::string vertexCode;
        std::string fragmentCode;
        try
        {
            // open files
            std::ifstream shaderStream(shaderFile + ".glsl");

            std::stringstream vShaderStream, fShaderStream;

            // read till it finds a "#shadertype" line
            std::string line;
            std::streampos oldPos = shaderStream.tellg();

            while (true) {
                oldPos = shaderStream.tellg();
                if (!std::getline(shaderStream, line)) break;

                if (line.find("#shadertype") != std::string::npos) {
                    if (line.find("vertex") != std::string::npos) {
                        oldPos = shaderStream.tellg();
                        while (std::getline(shaderStream, line)) {
                            if (line.find("#shadertype") != std::string::npos) {
                                // go back a line
                                shaderStream.seekg(oldPos);
                                break;
                            }
                            else {
                                oldPos = shaderStream.tellg();
                            }
                            vShaderStream << line << '\n';
                        }
                    }
                    else if (line.find("fragment") != std::string::npos) {
                        while (std::getline(shaderStream, line)) {
                            fShaderStream << line << '\n';
                        }
                    }
                }
            }


            // close file handlers
            shaderStream.close();
            // convert stream into string
            vertexCode = vShaderStream.str();
            fragmentCode = fShaderStream.str();
        }
        catch (std::exception e)
        {
            DOG_CRITICAL("Failed to read shader files");
            return false;
        }
        const char* vShaderCode = vertexCode.c_str();
        const char* fShaderCode = fragmentCode.c_str();
        // 2. now create shader object from source code
        if (!Compile(vShaderCode, fShaderCode)) {
            return false;
        }

        return true;
    }

    GLShader::~GLShader()
    {
        if (this->ID != UINT32_MAX) glDeleteProgram(ID);
    }

    GLShader& GLShader::Use()
    {
        if (this->ID != CurrentID) {
            glUseProgram(this->ID);
            CurrentID = this->ID;
        }

        return *this;
    }

    bool GLShader::Compile(const char* vertexSource, const char* fragmentSource)
    {
        unsigned int sVertex, sFragment;
        bool retVal = true;

        // vertex GLShader 
        sVertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(sVertex, 1, &vertexSource, NULL);
        glCompileShader(sVertex);
        if (!checkCompileErrors(sVertex, "VERTEX")) retVal = false;

        // fragment GLShader
        sFragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(sFragment, 1, &fragmentSource, NULL);
        glCompileShader(sFragment);
        if (!checkCompileErrors(sFragment, "FRAGMENT")) retVal = false;

        // shader program
        this->ID = glCreateProgram();
        glAttachShader(this->ID, sVertex);
        glAttachShader(this->ID, sFragment);
        glLinkProgram(this->ID);
        if (!checkCompileErrors(this->ID, "PROGRAM")) retVal = false;

        // delete the shaders as they're linked into our program now and no longer necessary
        glDeleteShader(sVertex);
        glDeleteShader(sFragment);

        return retVal;
    }

    void GLShader::SetUniformsFromCode()
    {
        GLint numUniforms = 0;
        glGetProgramiv(ID, GL_ACTIVE_UNIFORMS, &numUniforms);

        GLchar uniformName[256]; // Adjust size as needed
        for (GLint i = 0; i < numUniforms; ++i) {
            GLint arraySize = 0;
            GLenum type = 0;
            GLsizei actualLength = 0;
            glGetActiveUniform(ID, i, sizeof(uniformName), &actualLength, &arraySize, &type, uniformName);

            GLint location = glGetUniformLocation(ID, uniformName);
            if (location != -1) { // -1 indicates that the uniform is not active
                std::string name(uniformName, actualLength); // Construct a string with the actual name length
                Uniforms[name] = location;
            }
        }
    }

    void GLShader::SetFloat(const std::string& name, float value)
    {
        this->Use();
        glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
    }
    void GLShader::SetUnsigned(const std::string& name, unsigned int value)
    {
        this->Use();
        glUniform1ui(glGetUniformLocation(ID, name.c_str()), value);
    }
    void GLShader::SetInteger(const std::string& name, int value)
    {
        this->Use();
        glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
    }
    void GLShader::SetVector2f(const std::string& name, float x, float y)
    {
        this->Use();
        glUniform2f(glGetUniformLocation(ID, name.c_str()), x, y);
    }
    void GLShader::SetVector2f(const std::string& name, const glm::vec2& value)
    {
        this->Use();
        glUniform2f(glGetUniformLocation(ID, name.c_str()), value.x, value.y);
    }
    void GLShader::SetVector3f(const std::string& name, float x, float y, float z)
    {
        this->Use();
        glUniform3f(glGetUniformLocation(ID, name.c_str()), x, y, z);
    }
    void GLShader::SetVector3f(const std::string& name, const glm::vec3& value)
    {
        this->Use();
        glUniform3f(glGetUniformLocation(ID, name.c_str()), value.x, value.y, value.z);
    }
    void GLShader::SetVector4f(const std::string& name, float x, float y, float z, float w)
    {
        this->Use();
        glUniform4f(glGetUniformLocation(ID, name.c_str()), x, y, z, w);
    }
    void GLShader::SetVector4f(const std::string& name, const glm::vec4& value)
    {
        this->Use();
        glUniform4f(glGetUniformLocation(ID, name.c_str()), value.x, value.y, value.z, value.w);
    }
    void GLShader::SetMatrix4(const std::string& name, const glm::mat4& matrix)
    {
        this->Use();
        glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, false, glm::value_ptr(matrix));
    }

    void GLShader::SetUniformHandle(const std::string& name, GLuint64 handle) {
        glUniformHandleui64ARB(glGetUniformLocation(ID, name.c_str()), handle);
    }

    bool GLShader::HasUniform(const std::string& name)
    {
        return Uniforms.find(name) != Uniforms.end();
    }

    void GLShader::SetupInstanceVBO() 
    {
        if (instanceVBO != 0) return;

        glGenBuffers(1, &instanceVBO);
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
        uint32_t maxInstances = SimpleInstanceUniforms::MAX_INSTANCES;
        glBufferData(GL_ARRAY_BUFFER, maxInstances * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);
    }

    bool GLShader::checkCompileErrors(unsigned int object, std::string type)
    {
        int success;
        char infoLog[1024];
        if (type != "PROGRAM")
        {
            glGetShaderiv(object, GL_COMPILE_STATUS, &success);
            if (!success)
            {
                glGetShaderInfoLog(object, 1024, NULL, infoLog);
                std::cout << "| ERROR::SHADER: Compile-time error: Type: " << type << "\n"
                    << infoLog << "\n -- --------------------------------------------------- -- "
                    << std::endl;

                DOG_CRITICAL("GLShader Compile-time error: Type: {0}", type);
                DOG_CRITICAL("{0}", infoLog);
                return false;
            }
        }
        else
        {
            glGetProgramiv(object, GL_LINK_STATUS, &success);
            if (!success)
            {
                glGetProgramInfoLog(object, 1024, NULL, infoLog);
                std::cout << "| ERROR::GLShader: Link-time error: Type: " << type << "\n"
                    << infoLog << "\n -- --------------------------------------------------- -- "
                    << std::endl;

                DOG_CRITICAL("GLShader Link-time error: Type: {0}", type);
                DOG_CRITICAL("{0}", infoLog);
                return false;
            }
        }

        return true;
    }

    void GLShader::SetupUBO()
    {
        glGenBuffers(1, &uboMatrices);
        glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
        glBufferData(GL_UNIFORM_BUFFER, 3 * sizeof(glm::mat4), NULL, GL_STATIC_DRAW);
        glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboMatrices, 0, 3 * sizeof(glm::mat4));
    }

    void GLShader::SetProjectionUBO(const glm::mat4& projection)
    {
        glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
        glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(projection));
    }

    void GLShader::SetViewUBO(const glm::mat4& view)
    {
        glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
        glBufferSubData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(view));
    }

    void GLShader::SetProjectionViewUBO(const glm::mat4& projectionView)
    {
        glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(projectionView));
    }

    void GLShader::SetViewAndProjectionView(const glm::mat4& view, const glm::mat4& projectionView)
    {
        glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(projectionView));
        glBufferSubData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(view));
    }

    void GLShader::CleanupUBO()
    {
        glDeleteBuffers(1, &uboMatrices);
        uboMatrices = 0;

        glDeleteBuffers(1, &instanceVBO);
        instanceVBO = 0;

        CurrentID = 0;
    }

    void GLShader::BindUBO(const std::string& blockName, unsigned int bindingPoint) {
        unsigned int blockIndex = glGetUniformBlockIndex(ID, blockName.c_str());
        if (blockIndex != GL_INVALID_INDEX) {
            glUniformBlockBinding(ID, blockIndex, bindingPoint);
        }
    }

}