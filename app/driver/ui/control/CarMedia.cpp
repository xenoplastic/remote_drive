#include "CarMedia.h"
#include "glm/ext/matrix_transform.hpp" // glm::translate, glm::rotate, glm::scale

#include <QOpenGLShaderProgram>

CarMedia::CarMedia(QWidget* parent/* = nullptr*/) : 
	MediaElement(parent)
{
	CameraProperty cameraParam;
	cameraParam.hFov = 45.f;
	cameraParam.pitch = 5.f;
	cameraParam.yaw = 0.f;
	cameraParam.roll = 0.f;
	this->InitCamera(cameraParam, { 0, 3.f, 0 });
	
	const float initZ = -3.f;
	const float zOffset = 0.15f;
	const int count = 10;
	const float x = -2.5f;
	//左边提示
	float z = initZ;
	for (int i = 0; i < count; ++i)
	{
		m_leftPath.push_back({ x, 2.f, z });
		z -= zOffset;
	}	
	//右边提示
	z = initZ;
	for (int i = 0; i < count; ++i)
	{
		m_rightPath.push_back({ -x, 2.f, z });
		z -= zOffset;
	}
}

CarMedia::~CarMedia()
{

}

void CarMedia::SetSteeringWheel(float angle)
{
	m_angle = std::clamp(angle, -1.f, 1.f);
}

void CarMedia::AddModel(int vertexAttrLoc, int colorAttrLoc)
{
	//立方体,默认顶点逆时针为正面
	float alpha = 0.7f;
	float cube[] = {
		//底面
		-0.5f, 0.f, -0.1f, 0.3f, 0.4f, 0.3f, alpha,
		0.5f, 0.f, -0.1f, 0.3f, 0.1f, 0.4f, alpha,
		0.5f, 0.f, 0.1f, 0.1f, 0.9f, 0.2f, alpha,
		-0.5f, 0.f, 0.1f,  0.3f, 0.3f, 0.8f, alpha,

		//顶面		
		-0.5f, 0.05f, 0.1f, 0.1f, 1.f, 1.f, alpha,
		0.5f, 0.05f, 0.1f, 0.3f, 1.f, 0.5f, alpha,
		0.5f, 0.05f, -0.1f, 1.f, 1.f, 0.1f, alpha,
		-0.5f, 0.05f, -0.1f, 1.f, 1.f, 0.2f, alpha,
		
		//前面
		-0.5f, 0.05f, 0.1f, 1.f, 0.f, 1.f, alpha,
		-0.5f, 0.f, 0.1f, 1.f, 0.f, 0.f, alpha,
		0.5f, 0.f, 0.1f, 1.f, 0.5f, 0.f, alpha,
		0.5f, 0.05f, 0.1f, 1.0f, 0.4f, 0.3f, alpha,

		//后面
		-0.5f, 0.05f, -0.1f, 0.56f, 0.45f, 0.44f, alpha,
		0.5f, 0.05f, -0.1f, 0.23f, 0.87f, 0.89f, alpha,
		0.5f, 0.f, -0.1f, 0.12f, 0.22f, 0.55f, alpha,
		-0.5f, 0.f, -0.1f, 0.12f, 0.32f, 0.55f, alpha,

		//左面
		-0.5f, 0.05f, -0.1f, 0.56f, 0.45f, 0.44f, alpha,
		-0.5f, 0.f, -0.1f, 0.12f, 0.22f, 0.55f, alpha,
		-0.5f, 0.f, 0.1f, 0.91f, 0.32f, 0.71f, alpha,
		-0.5f, 0.05f, 0.1f, 0.3f, 0.4f, 0.3f, alpha,

		//右面
		0.5f, 0.05f, 0.1f, 0.3f, 0.4f, 0.3f, alpha,
		0.5f, 0.f, 0.1f, 0.3f, 0.4f, 0.3f, alpha,
		0.5f, 0.f, -0.1f, 0.3f, 0.4f, 0.3f, alpha,
		0.5f, 0.05f, -0.1f, 0.3f, 0.4f, 0.3f, alpha,
	};

	m_qVBO.create();
	m_qVBO.bind();
	m_qVBO.allocate(cube, sizeof(cube));
	glVertexAttribPointer(vertexAttrLoc, 3, GL_FLOAT, GL_FALSE,
		7 * sizeof(float), (void*)0);
	glVertexAttribPointer(colorAttrLoc, 4, GL_FLOAT, GL_FALSE,
		7 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(vertexAttrLoc);
	glEnableVertexAttribArray(colorAttrLoc);
	m_qVBO.release();
}

void CarMedia::PaintModel()
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);	

	m_qVBO.bind();

	float xOffset = 0.f;
	float step = 0.f;
	const float stepOffset = 0.06f * m_angle;
	auto transFun = [&](const glm::vec3& model) {
		glm::mat4 matModel(1);
		matModel = glm::translate(matModel, glm::vec3(model.x + xOffset, model.y, model.z));
		//matModel = glm::rotate(matModel, glm::radians(-step * 50),
		//	glm::vec3(0.0f, 1.0f, 0.0f));
		matModel = glm::scale(matModel, glm::vec3(0.8f, 0.3f, 0.5f));

		auto matMVP = m_matProjection * m_matView * matModel;
		glUniformMatrix4fv(m_shaderProgram->uniformLocation("matMVP"),
			1, GL_FALSE, &matMVP[0][0]);
		glDrawArrays(GL_QUADS, 0, m_qVBO.size() / sizeof(float));

		step += stepOffset;
		xOffset += step;
	};

	for (auto& model : m_leftPath)
	{
		transFun(model);
	}

	xOffset = 0.f;
	step = 0.f;
	for (auto& model : m_rightPath)
	{
		transFun(model);
	}

	m_qVBO.release();

	glDisable(GL_BLEND);
}
