#pragma once

#include <QOpenGLWidget>
#include <QOpenGLExtraFunctions>
#include <QRect>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>

#include "glm/ext/matrix_transform.hpp"

QT_FORWARD_DECLARE_CLASS(QOpenGLShader)
QT_FORWARD_DECLARE_CLASS(QOpenGLShaderProgram)
QT_FORWARD_DECLARE_CLASS(QOpenGLTexture)

//glm默认为右手坐标系

enum class OpenGLViewMode
{
	//保持背景长宽比，显示完整背景,背景和控件比例不一致时，出现黑边
	SBM_FULL,
	//保持背景长宽比，铺满整个控件,因此会裁剪背景
	SBM_COVER,
};

//相机参数，角度相关统一度表示
struct CameraProperty 
{
	float yaw;
	float pitch;
	float roll;
	float hFov;
	float zNear = 0.1f;
	float zFar = 5000.f;
};

class OpenGLView : public QOpenGLWidget, protected QOpenGLExtraFunctions
{
	Q_OBJECT
public:
	OpenGLView(QWidget* parent = nullptr);
	~OpenGLView();

	void InitCamera(
		const CameraProperty& cameraParam,
		const glm::vec3& camWorldPoint
	);

	QRect GetVideoRegion() const;
	void SetBackgroundMode(OpenGLViewMode sbm);
	OpenGLViewMode GetBackgroundMode();

	void ShowModel(bool visible);

	virtual void ResetRender(bool start = true);
	
public slots:
	void LoadYUV(void* yBuf, void* uBuf, void* vBuf,
		uint width, uint height, bool needCopy = true);

signals:
	void videoRectChange(const QRect& videoRect);

protected:
	void initializeGL() Q_DECL_OVERRIDE;
	void paintGL() Q_DECL_OVERRIDE;
	void resizeGL(int w, int h) Q_DECL_OVERRIDE;

	virtual void AddModel(int vertexAttrLoc, int colorAttrLoc);
	virtual void PaintModel();

	//调整视频渲染长宽比
	void AdjustDraw(uint videoWidth, uint videoHeight);

protected:
	QOpenGLBuffer m_backgroundVBO;

	//opengl中y、u、v分量位置
	GLuint m_textureUniformY;
	GLuint m_textureUniformU;
	GLuint m_textureUniformV;
	QOpenGLTexture* m_textureY = nullptr;
	QOpenGLTexture* m_textureU = nullptr;
	QOpenGLTexture* m_textureV = nullptr;
	//自己创建的纹理对象ID，创建错误返回0
	GLuint m_idY = 0, m_idU = 0, m_idV = 0;
	//被渲染的视频长宽
	uint m_videoWidth = 0, m_videoHeight = 0;
	void* m_yBuf = nullptr, * m_uBuf = nullptr, * m_vBuf = nullptr;
	size_t m_yBufSize = 0, m_uBufSize = 0, m_vBufSize = 0;
	QOpenGLShaderProgram* m_shaderProgram = nullptr;
	//裁剪之前的视频相对于控件的区域,非视频渲染区域，
	QRect m_videoRect;
	//采集的视频区域(相对于视频)
	QRect m_capVideoRect;
	OpenGLViewMode m_backMode = OpenGLViewMode::SBM_COVER;
	std::atomic_bool m_enableRender = false;

	//标记相关
	QOpenGLVertexArrayObject m_VAO;
	float m_showModel = 0.f;
	glm::mat4 m_matView;
	glm::mat4 m_matProjection;
};