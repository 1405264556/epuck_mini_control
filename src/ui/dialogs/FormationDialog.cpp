#include "FormationDialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QLabel>

FormationDialog::FormationDialog(int robotCount, QWidget *parent)
    : QDialog(parent), m_robotCount(robotCount)
{
    setWindowTitle("编队控制");
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(QString("为 %1 台机器人配置编队").arg(robotCount)));

    auto *form = new QFormLayout;
    m_shapeCombo = new QComboBox;
    m_shapeCombo->addItem("横列", static_cast<int>(FormationShape::Line));
    m_shapeCombo->addItem("圆形", static_cast<int>(FormationShape::Circle));
    m_shapeCombo->addItem("V字形", static_cast<int>(FormationShape::VShape));
    m_shapeCombo->addItem("楔形", static_cast<int>(FormationShape::Wedge));
    m_shapeCombo->addItem("纵队", static_cast<int>(FormationShape::Column));
    m_shapeCombo->addItem("菱形", static_cast<int>(FormationShape::Diamond));
    form->addRow("队形：", m_shapeCombo);

    m_spacingSpinner = new QDoubleSpinBox; m_spacingSpinner->setRange(5, 100); m_spacingSpinner->setValue(15);
    form->addRow("间距 (厘米)：", m_spacingSpinner);

    m_headingSpinner = new QDoubleSpinBox; m_headingSpinner->setRange(-180, 180); m_headingSpinner->setValue(0);
    form->addRow("朝向 (度)：", m_headingSpinner);
    layout->addLayout(form);

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(btns);
}

FormationShape FormationDialog::selectedShape() const {
    return static_cast<FormationShape>(m_shapeCombo->currentData().toInt());
}
QVector<Vec2> FormationDialog::formationPositions() const { return {}; }
double FormationDialog::spacing() const { return m_spacingSpinner->value(); }
double FormationDialog::heading() const { return m_headingSpinner->value(); }
