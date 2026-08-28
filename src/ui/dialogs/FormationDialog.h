#pragma once
#include <QDialog>
#include "core/Types.h"
class QComboBox;
class QDoubleSpinBox;
class RobotManager;

class FormationDialog : public QDialog {
    Q_OBJECT
public:
    explicit FormationDialog(int robotCount, QWidget *parent = nullptr);
    FormationShape selectedShape() const;
    QVector<Vec2> formationPositions() const;
    double spacing() const; double heading() const;
private:
    QComboBox *m_shapeCombo;
    QDoubleSpinBox *m_spacingSpinner, *m_headingSpinner;
    int m_robotCount;
};
