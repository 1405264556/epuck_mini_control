#pragma once
#include <QLabel>
#include <QImage>
struct SensorData;
class CameraViewWidget : public QLabel {
    Q_OBJECT
public:
    explicit CameraViewWidget(QWidget *p = nullptr);
    void updateData(const SensorData &d);
protected:
    void resizeEvent(QResizeEvent *event) override;
private:
    QImage m_frame;
    void refreshFrame();
};
