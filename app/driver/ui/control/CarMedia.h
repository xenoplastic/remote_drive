#pragma once

#include "MediaElement.h"
#include "glm/ext/vector_double3.hpp"

#include <QOpenGLBuffer>
#include <QVector>

class CarMedia : public MediaElement
{
	Q_OBJECT
public:
	CarMedia(QWidget* parent = nullptr);
	~CarMedia();

	//设置方向盘角度(-1~+1),正值向右，负值向左
	void SetSteeringWheel(float angle);

protected:
	void AddModel(int vertexAttrLoc, int colorAttrLoc) override;
	void PaintModel() override;

protected:
	QOpenGLBuffer m_qVBO;
	QVector<glm::vec3> m_leftPath;
	QVector<glm::vec3> m_rightPath;
	float m_angle = 0.f;
};

