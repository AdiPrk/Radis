#include <PCH/pch.h>
#include "GLFrameBuffer.h"


namespace Dog {

	GLFrameBuffer::GLFrameBuffer(const FrameBufferSpecification& spec)
		: specification(spec)
	{
		Create();

		eventSceneResize = SUBSCRIBE_EVENT(Event::SceneResize, OnSceneResize);
	}

	GLFrameBuffer::~GLFrameBuffer()
	{
		Clear();
	}

	void GLFrameBuffer::Bind()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glViewport(0, 0, specification.width, specification.height);
	}

	void GLFrameBuffer::Unbind()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void GLFrameBuffer::OnSceneResize(const Event::SceneResize& event)
	{
		if (event.width <= 0 || event.height <= 0)
			return;

		specification.width = event.width;
		specification.height = event.height;

		//Shader::SetResolutionUBO(glm::vec2(event.width, event.height));

		//DOG_INFO("GLFrameBuffer::OnSceneResize({}, {})", specification.width, specification.height);

		Create();
	}

	void GLFrameBuffer::Create()
	{
		Clear(); // remove any old data

		glCreateFramebuffers(1, &fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);

		for (auto& ca : specification.attachments) {
			switch (ca)
			{
			case FBAttachment::None:
				break;
			case FBAttachment::Depth24Stencil8:
				AddDepthAttachment(ca);
				break;
            case FBAttachment::RGBA:
            case FBAttachment::RGBA8:
            case FBAttachment::RGBA8_SRGB:
            case FBAttachment::RGBA16F:
            case FBAttachment::RGBA32F:
                AddColorAttachment(ca);
				break;
			default:
			{
                DOG_CRITICAL("Unsupported FBAttachment format!: {}", (int)ca);
				break;
			}
			}
		}

		glCreateTextures(GL_TEXTURE_2D, 1, &depthAttachment);
		glBindTexture(GL_TEXTURE_2D, depthAttachment);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, specification.width, specification.height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depthAttachment, 0);

		DOG_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Framebuffer is incomplete!");

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void GLFrameBuffer::Clear()
	{
		if (fbo)
		{
			glDeleteFramebuffers(1, &fbo);
			glDeleteTextures(1, &depthAttachment);
			for (auto& ca : colorAttachments) glDeleteTextures(1, &ca);
			fbo = 0;
			depthAttachment = 0;
			colorAttachments.clear();
		}
	}

	void GLFrameBuffer::AddColorAttachment(const FBAttachment& format)
	{
		unsigned nColorAttachments = (unsigned)colorAttachments.size();
		DOG_ASSERT(nColorAttachments < 4, "Too many color attachments!");
		colorAttachments.push_back(0);

		GLint glFormat = static_cast<GLint>(format);

		glCreateTextures(GL_TEXTURE_2D, 1, &colorAttachments.back());
		glBindTexture(GL_TEXTURE_2D, colorAttachments.back());
		glTexImage2D(GL_TEXTURE_2D, 0, glFormat, specification.width, specification.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + nColorAttachments, GL_TEXTURE_2D, colorAttachments.back(), 0);
	}

	void GLFrameBuffer::AddDepthAttachment(const FBAttachment& format)
	{
		DOG_ASSERT(depthAttachment == 0, "Depth attachment already exists!");

		GLint glFormat = static_cast<GLint>(format);

		glCreateTextures(GL_TEXTURE_2D, 1, &depthAttachment);
		glBindTexture(GL_TEXTURE_2D, depthAttachment);
		glTexImage2D(GL_TEXTURE_2D, 0, glFormat, specification.width, specification.height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthAttachment, 0);
	}

} // namespace Dog
