#ifndef RENDERWIDGET_HPP
#define RENDERWIDGET_HPP

#include <GL/glew.h>
#include <QtCore/qtimer.h>
#include <QtWidgets/qopenglwidget.h>

#include "simulation_ei.hpp"

class renderWidget : public QOpenGLWidget {
  Q_OBJECT

private:
  camera observer;
  std::chrono::high_resolution_clock::time_point initTime;
  std::chrono::high_resolution_clock::time_point currTime;

  QSurfaceFormat format;
  QOpenGLContext* glMainContext;
  QTimer* timer;

public:
  renderWidget(QWidget* parent = Q_NULLPTR);
  virtual ~renderWidget();

protected:
  void initializeGL() override;
  void resizeGL(int w, int h) override;
  void paintGL() override;

public slots:
  void updateObjects();
};

#endif