#include "OpenGLView.h"
#include "common.h"

#include "glm/ext/matrix_clip_space.hpp" // glm::perspective

#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QDebug>
#include <QCoreApplication>

OpenGLView::OpenGLView(QWidget* parent) :
	QOpenGLWidget(parent)
{
	QSurfaceFormat format;
	format.setSamples(8);
	setFormat(format);
}

OpenGLView::~OpenGLView()
{
	makeCurrent();
	m_backgroundVBO.destroy();
	delete m_textureY;
	delete m_textureU;
	delete m_textureV;
	delete m_shaderProgram;
	doneCurrent();

	if (m_yBuf)
		free(m_yBuf);
	if (m_uBuf)
		free(m_uBuf);
	if (m_vBuf)
		free(m_vBuf);
}

void OpenGLView::InitCamera(const CameraProperty& cameraParam, const glm::vec3& camWorldPoint)
{
	glm::mat4 matCamTrans = glm::translate(glm::mat4(1.0f),
		-camWorldPoint);
	m_matView = glm::mat4(1.0f);
	m_matView = glm::rotate(m_matView, glm::radians(cameraParam.pitch), 
		glm::vec3(1.0f, 0.0f, 0.0f));
	m_matView = glm::rotate(m_matView, glm::radians(cameraParam.yaw), 
		glm::vec3(0.0f, 1.0f, 0.0f));
	m_matView = glm::rotate(m_matView, glm::radians(cameraParam.roll),
		glm::vec3(0.0f, 0.0f, 1.0f));
	m_matView *= matCamTrans;

	auto aspect = (float)this->width() / this->height();
	m_matProjection = glm::perspective(glm::radians(cameraParam.hFov), aspect, 
		cameraParam.zNear, cameraParam.zFar);
}

QRect OpenGLView::GetVideoRegion() const
{
	return m_videoRect;
}

void OpenGLView::SetBackgroundMode(OpenGLViewMode sbm)
{
	m_backMode = sbm;
	AdjustDraw(m_videoWidth, m_videoHeight);
	
	update();
	static_cast<QWidget*>(parent())->update();
}

OpenGLViewMode OpenGLView::GetBackgroundMode()
{
	return m_backMode;
}

void OpenGLView::ShowModel(bool visible)
{
	m_showModel = visible ? 1.f : 0.f;
	update();
}

void OpenGLView::ResetRender(bool enable/* = true*/)
{
	if (m_yBuf)
	{
		free(m_yBuf);
		m_yBuf = nullptr;
	}
		
	if (m_uBuf)
	{
		free(m_uBuf);
		m_uBuf = nullptr;
	}
		
	if (m_vBuf)
	{
		free(m_vBuf);
		m_vBuf = nullptr;
	}

	m_enableRender = enable;

	update();
	static_cast<QWidget*>(parent())->update();
}

void OpenGLView::LoadYUV(void* yBuf, void* uBuf, void* vBuf,
	uint width, uint height, bool needCopy /*= true*/)
{
	auto yBufSize = (size_t)width * height;
	auto uBufSize = (size_t)width * height / 4;
	auto vBufSize = (size_t)width * height / 4;
	if (needCopy)
	{
		auto AllocBufferFun = [&](void** buf, size_t& currentSize, size_t newSize)
		{
			if (*buf == nullptr)
			{
				currentSize = newSize;
				*buf = malloc(newSize);
			}
			else if (currentSize != newSize)
			{
				auto newBuf = realloc(*buf, newSize);
				if (!newBuf)
				{
					free(*buf);
					*buf = nullptr;
					currentSize = 0;
				}
				else
				{
					*buf = newBuf;
					currentSize = newSize;
				}
			}
		};

		AllocBufferFun(&m_yBuf, m_yBufSize, yBufSize);
		AllocBufferFun(&m_uBuf, m_uBufSize, uBufSize);
		AllocBufferFun(&m_vBuf, m_vBufSize, vBufSize);

		if (!m_yBuf || !m_uBuf || !m_vBuf) 
		{
			return;
		}

		memcpy_s(m_yBuf, m_yBufSize, yBuf, yBufSize);
		memcpy_s(m_uBuf, m_uBufSize, uBuf, uBufSize);
		memcpy_s(m_vBuf, m_vBufSize, vBuf, vBufSize);
	}
	else
	{
		m_yBuf = yBuf;
		m_uBuf = uBuf;
		m_vBuf = vBuf;
		m_yBufSize = yBufSize;
		m_uBufSize = uBufSize;
		m_vBufSize = vBufSize;
	}

	if (this->m_videoWidth != width || this->m_videoHeight != height)
	{
		AdjustDraw(width, height);
		this->m_videoWidth = width;
		this->m_videoHeight = height;
	}

	update();
	static_cast<QWidget*>(parent())->update();
}

void OpenGLView::initializeGL()
{
	initializeOpenGLFunctions();
	glCullFace(GL_FRONT_AND_BACK);

	GLfloat vertices[]{
		//顶点坐标
		-1.0f,-1.0f,-1.f,
		-1.0f,+1.0f,-1.f,
		+1.0f,+1.0f,-1.f,
		+1.0f,-1.0f,-1.f,
		//纹理坐标
		0.0f,1.0f,
		0.0f,0.0f,
		1.0f,0.0f,
		1.0f,1.0f,
	};
	m_backgroundVBO.create();
	m_backgroundVBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
	m_backgroundVBO.bind();
	m_backgroundVBO.allocate(vertices, sizeof(vertices));

	QString exeDir = QCoreApplication::applicationDirPath();
	QOpenGLShader vertexShader(QOpenGLShader::Vertex, this);
	QOpenGLShader fragmentShader(QOpenGLShader::Fragment, this);
	vertexShader.compileSourceFile(exeDir + "/shader.vs");
	fragmentShader.compileSourceFile(exeDir + "/shader.fs");

	m_shaderProgram = new QOpenGLShaderProgram(this);
	m_shaderProgram->addShader(&vertexShader);
	m_shaderProgram->addShader(&fragmentShader);
	m_shaderProgram->link();

	auto vertexIn = m_shaderProgram->attributeLocation("vertexIn");
	auto textureIn = m_shaderProgram->attributeLocation("textureIn");
	m_shaderProgram->setAttributeBuffer(vertexIn, GL_FLOAT, 0, 3, 3 * sizeof(GLfloat));
	m_shaderProgram->setAttributeBuffer(textureIn, GL_FLOAT,
		12 * sizeof(GLfloat), 2, 2 * sizeof(GLfloat));
	m_shaderProgram->enableAttributeArray(vertexIn);
	m_shaderProgram->enableAttributeArray(textureIn);

	m_textureUniformY = m_shaderProgram->uniformLocation("tex_y");
	m_textureUniformU = m_shaderProgram->uniformLocation("tex_u");
	m_textureUniformV = m_shaderProgram->uniformLocation("tex_v");
	
	m_textureY = new QOpenGLTexture(QOpenGLTexture::Target2D);
	m_textureU = new QOpenGLTexture(QOpenGLTexture::Target2D);
	m_textureV = new QOpenGLTexture(QOpenGLTexture::Target2D);

	m_textureY->create();
	m_textureU->create();
	m_textureV->create();
	m_idY = m_textureY->textureId();
	m_idU = m_textureU->textureId();
	m_idV = m_textureV->textureId();
	m_backgroundVBO.release();

	//初始化标记相关
	auto vertexAttrLoc = m_shaderProgram->attributeLocation("vertexSign");
	auto colorAttrLoc = m_shaderProgram->attributeLocation("colorSign");
	m_VAO.create();
	m_VAO.bind();
	AddModel(vertexAttrLoc, colorAttrLoc);
	m_VAO.release();

	glClearColor(0.0,0.0, 0.0, 0.0);
}

void OpenGLView::paintGL()
{
	glClear(GL_COLOR_BUFFER_BIT);

	if (!m_yBuf || !m_uBuf || !m_vBuf || !m_enableRender)
		return;
		
	m_shaderProgram->bind();

	auto indexModel = m_shaderProgram->uniformLocation("isModel");
	glUniform1f(indexModel, 0.f);

	auto glTexParmFunc = [this]{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	};

	//激活纹理单元GL_TEXTURE0,系统里面的
	glActiveTexture(GL_TEXTURE0); 
	//绑定y分量纹理对象id到激活的纹理单元
	glBindTexture(GL_TEXTURE_2D, m_idY); 
	//使用内存中的数据创建真正的y分量纹理数据
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_videoWidth, m_videoHeight, 0, 
		GL_RED, GL_UNSIGNED_BYTE, m_yBuf);
	//https://blog.csdn.net/xipiaoyouzi/article/details/53584798 纹理参数解析
	glTexParmFunc();

	//激活纹理单元GL_TEXTURE1
	glActiveTexture(GL_TEXTURE1); 
	glBindTexture(GL_TEXTURE_2D, m_idU);
	//使用内存中的数据创建真正的u分量纹理数据
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_videoWidth >> 1, m_videoHeight >> 1, 0, 
		GL_RED, GL_UNSIGNED_BYTE, m_uBuf);
	glTexParmFunc();

	//激活纹理单元GL_TEXTURE2
	glActiveTexture(GL_TEXTURE2); 
	glBindTexture(GL_TEXTURE_2D, m_idV);
	//使用内存中的数据创建真正的v分量纹理数据
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_videoWidth >> 1, m_videoHeight >> 1, 0, 
		GL_RED, GL_UNSIGNED_BYTE, m_vBuf);
	glTexParmFunc();
	//指定y纹理要使用新值
	glUniform1i(m_textureUniformY, 0);
	//指定u纹理要使用新值
	glUniform1i(m_textureUniformU, 1);
	//指定v纹理要使用新值
	glUniform1i(m_textureUniformV, 2);
	
	//使用顶点数组方式绘制图形
	m_backgroundVBO.bind();
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	m_backgroundVBO.release();

	//////////////////////////////////////////////////////////////////////////
	//画模型
	if (m_showModel)
	{
		glUniform1f(indexModel, 1.f);
		glEnable(GL_DEPTH_TEST);
		if (m_backMode == OpenGLViewMode::SBM_FULL)
		{
			glEnable(GL_SCISSOR_TEST);
			glScissor(m_videoRect.x(), m_videoRect.y(), m_videoRect.width(),
				m_videoRect.height());
		}

		//根据背景模式,调整顶点坐标
		auto indexWidthPercent = m_shaderProgram->uniformLocation("widthPercent");
		auto indexHeightPercent = m_shaderProgram->uniformLocation("heightPercent");
		if (m_backMode == OpenGLViewMode::SBM_FULL)
		{
			auto widthPercent = (float)m_videoRect.width() / this->width();
			auto heightPercent = (float)m_videoRect.height() / this->height();
			glUniform1f(indexWidthPercent, widthPercent);
			glUniform1f(indexHeightPercent, heightPercent);
		}
		else
		{
			auto widthPercent = (float)this->m_videoWidth / m_capVideoRect.width();
			auto heightPercent = (float)this->m_videoHeight / m_capVideoRect.height();
			glUniform1f(indexWidthPercent, widthPercent);
			glUniform1f(indexHeightPercent, heightPercent);
		}
		
		m_VAO.bind();
		PaintModel();
		m_VAO.release();
		
		glDisable(GL_DEPTH_TEST);
		if (m_backMode == OpenGLViewMode::SBM_FULL)
			glDisable(GL_SCISSOR_TEST);
	}
	
	m_shaderProgram->release();
}

void OpenGLView::resizeGL(int w, int h)
{
	if (this->m_videoWidth != 0 && this->m_videoHeight != 0) 
	{
		AdjustDraw(this->m_videoWidth, this->m_videoHeight);
	}
}

void OpenGLView::AddModel(int vertexAttrLoc, int colorAttrLoc)
{

}

void OpenGLView::PaintModel()
{

}

void OpenGLView::AdjustDraw(uint videoWidth, uint videoHeight)
{
	bool ret = false;
	auto rect = this->geometry();
	float controlWidth = this->width();
	float controlHeight = this->height();
	auto videoRatio = (float)videoWidth / videoHeight;
	auto controlRatio = (float)controlWidth / controlHeight;
	m_videoRect.setRect(0, 0, controlWidth, controlHeight);
	m_capVideoRect.setRect(0, 0, videoWidth, videoHeight);
	if (videoRatio > controlRatio)
	{	
		ret = m_backgroundVBO.bind();
		if (m_backMode == OpenGLViewMode::SBM_FULL)
		{
			auto drawVideoHeight = controlWidth / videoRatio;
			float verYPos = drawVideoHeight / controlHeight;
			GLfloat vertices[]{
				//顶点坐标
				-1.0f,-verYPos,-1.f,
				-1.0f,verYPos,-1.f,
				+1.0f,verYPos,-1.f,
				+1.0f,-verYPos,-1.f,
				//纹理坐标
				0.0f,1.0f,
				0.0f,0.0f,
				1.0f,0.0f,
				1.0f,1.0f,
			};
			m_backgroundVBO.allocate(vertices, sizeof(vertices));
			m_videoRect.setRect(0, (controlHeight - drawVideoHeight) / 2.f,
				controlWidth, drawVideoHeight);
			m_capVideoRect.setRect(0, 0,videoWidth, videoHeight);
		}
		else
		{
			auto mapVideoWidth = videoHeight * controlRatio;
			float texX =  ((float)(videoWidth - mapVideoWidth) / 2.f) / videoWidth;
			GLfloat vertices[]{
				//顶点坐标
				-1.0f,-1,-1.f,
				-1.0f,1,-1.f,
				+1.0f,1,-1.f,
				+1.0f,-1,-1.f,
				//纹理坐标
				texX,1.0f,
				texX,0.0f,
				1.0f - texX,0.0f,
				1.0f - texX,1.0f,
			};
			m_backgroundVBO.allocate(vertices, sizeof(vertices));
			auto newControlWidth = controlHeight * videoRatio;
			m_videoRect.setRect(-(newControlWidth - controlWidth) / 2.f, 0, 
				newControlWidth, 
				controlHeight);
			m_capVideoRect.setRect((videoWidth - mapVideoWidth) / 2.f, 0, 
				mapVideoWidth, videoHeight);
		}			
		m_backgroundVBO.release();	
		emit videoRectChange(m_videoRect);
	}
	else if (videoRatio < controlRatio)
	{
		ret = m_backgroundVBO.bind();
		if (m_backMode == OpenGLViewMode::SBM_FULL)
		{
			auto drawVideoWidth = controlHeight * videoRatio;
			float verXPos = drawVideoWidth / controlWidth;
			GLfloat vertices[]{
				//顶点坐标
				-verXPos,-1.0f,-1.f,
				-verXPos,+1.0f,-1.f,
				verXPos,+1.0f,-1.f,
				verXPos,-1.0f,-1.f,
				//纹理坐标
				0.0f,1.0f,
				0.0f,0.0f,
				1.0f,0.0f,
				1.0f,1.0f,
			};
			m_backgroundVBO.allocate(vertices, sizeof(vertices));
			m_videoRect.setRect((controlWidth - drawVideoWidth) / 2.f, 0,
				drawVideoWidth, controlHeight);
			m_capVideoRect.setRect(0, 0, videoWidth, videoHeight);
		}
		else
		{
			auto mapVideoHeight = videoWidth / controlRatio;
			float texY = ((float)(videoHeight - mapVideoHeight) / 2.f) / videoHeight;
			GLfloat vertices[]{
				//顶点坐标
				-1,-1.0f,-1.f,
				-1,+1.0f,-1.f,
				1,+1.0f,-1.f,
				1,-1.0f,-1.f,
				//纹理坐标
				0.0f,1 - texY,
				0.0f,texY,
				1.0f,texY,
				1.0f,1 - texY,
			};
			m_backgroundVBO.allocate(vertices, sizeof(vertices));
			auto newControlHeight = controlWidth / videoRatio;
			m_videoRect.setRect(0, -(newControlHeight - controlHeight) / 2.f, controlWidth, 
				newControlHeight);
			m_capVideoRect.setRect(0, (videoHeight - mapVideoHeight) / 2.f, 
				videoWidth, mapVideoHeight);
		}
		m_backgroundVBO.release();
		emit videoRectChange(m_videoRect);
	}
}

