#pragma once

#include <QPoint>
#include <QCursor>
#include <QString>

class BaseMeshViewer;
class QMouseEvent;
class QWheelEvent;
class QKeyEvent;
class AbstractMeshEventHandle
{
public:
	enum ProcessorType{
		ProcessorTypeGeneral = 0,
		ProcessorTypeClothSelect,
		ProcessorTypeClothTranslate,
		ProcessorTypeClothRotate,
		ProcessorTypeEnd, // the end, no processor for this
	};
public:
	AbstractMeshEventHandle(BaseMeshViewer* v);
	~AbstractMeshEventHandle();
	virtual ProcessorType type() { return ProcessorTypeGeneral; }
	static AbstractMeshEventHandle* create(ProcessorType type, BaseMeshViewer* v);
	QCursor& cursor(){ return m_cursor; }
	const QCursor& cursor()const{ return m_cursor; }
	QString iconFile()const;
	QString inactiveIconFile()const;
	QString toolTips()const;

	virtual void mousePressEvent(QMouseEvent *);
	virtual void mouseReleaseEvent(QMouseEvent *);
	virtual void mouseDoubleClickEvent(QMouseEvent *);
	virtual void mouseMoveEvent(QMouseEvent*);
	virtual void wheelEvent(QWheelEvent*);
	virtual void keyPressEvent(QKeyEvent*);
	virtual void keyReleaseEvent(QKeyEvent*);

	void pickMesh(QPoint p);
	bool getPickedMeshFrameInfo(ldp::Double3& o, ldp::Double3& u, ldp::Double3& v, int& id_l)const;
protected:
	BaseMeshViewer* m_viewer;
	int m_lastHighlightShapeId;
	int m_currentSelectedId;
	QPoint m_mouse_press_pt;
	QCursor m_cursor;
	QString m_iconFile;
	QString m_inactiveIconFile;
	QString m_toolTips;
	ldp::Double3 m_picked_screenPos;
};