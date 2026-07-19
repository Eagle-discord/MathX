#include "CopyableLabel.h"
#include <QMessageBox>

void CopyableLabel::showDetailPopup() {
    QMessageBox::information(this, "Formula", m_detail);
}