#include "SceneMap.h"

#include <QPainter>

SceneMap::SceneMap(QWidget* parent /*= nullptr*/) :
	QWidget(parent)
{
	m_map.load(":/images/map_yinli.png");	
	m_targetImage = new QPixmap(m_map.width(), m_map.height());
}

SceneMap::~SceneMap()
{
	delete m_targetImage;
}

void SceneMap::SetCarPos(int x, int y)
{
	auto targetHeight = m_targetImage->height();
	m_carX = std::clamp(x, 0, m_targetImage->width());
	m_carY = std::clamp(y, 0, targetHeight);
	m_carY = targetHeight - m_carY;
	update();
}

void SceneMap::paintEvent(QPaintEvent* event)
{
	QPainter painterTarget(m_targetImage);
	painterTarget.drawPixmap(0, 0, m_map);

	//绘制车元素
	painterTarget.setPen(QPen(Qt::red));
	painterTarget.setBrush(QBrush(Qt::red));
	painterTarget.drawEllipse(QPoint(m_carX, m_carY), m_carIconWidth , 
		m_carIconHeight);

	//获取当前控件大小
	//获取m_targetImage大小
	//根据控件宽高比率和m_targetImage宽高比率调整最终图片绘制位置
	auto sizeControl = this->size();
	auto sizeTarget = m_targetImage->size();
	auto ratioControl = sizeControl.width() / sizeControl.height();
	auto ratioTarget = sizeTarget.width() / sizeTarget.height();
	QPainter painter(this);
	if (ratioControl > ratioTarget)
	{
		int drawHeight = sizeControl.height();
		int drawWidth = drawHeight * ratioTarget;
		int drawX = (sizeControl.width() - drawWidth) / 2;
		int drawY = 0;
		painter.drawPixmap(drawX, drawY, drawWidth, drawHeight, *m_targetImage);
	}
	else
	{
		int drawWidth = sizeControl.width();
		int drawHeight = drawWidth / ratioTarget;
		int drawX = 0;
		int drawY = (sizeControl.height() - drawHeight) / 2;
		painter.drawPixmap(drawX, drawY, drawWidth, drawHeight, *m_targetImage);
	}
}
