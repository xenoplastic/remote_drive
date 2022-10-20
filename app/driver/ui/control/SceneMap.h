#pragma once

#include <QWidget>
#include <QPixmap>

class SceneMap : public QWidget
{
	Q_OBJECT
public:
	SceneMap(QWidget* parent = nullptr);
	~SceneMap();

	void SetCarPos(int x, int y);

protected:
	void paintEvent(QPaintEvent* event) override;

private:
	QPixmap m_map;
	QPixmap* m_targetImage;
	int m_carX = 0;
	int m_carY = 0;
	const int m_carIconWidth = 20;
	const int m_carIconHeight = 20;
};