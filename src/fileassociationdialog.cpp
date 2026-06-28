#include "fileassociationdialog.h"

#include "fileassociationservice.h"
#include "imageformats.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>

FileAssociationDialog::FileAssociationDialog(QWidget *parent)
    : QDialog(parent)
{
    const QList<ImageFormatDescriptor> formats = ImageFormats::supportedFormatDescriptors();

    setWindowTitle(tr("关联支持的图片格式"));
    resize(420, 520);

    auto *layout = new QVBoxLayout(this);
    auto *selectAllCheckBox = new QCheckBox(tr("全选"), this);
    selectAllCheckBox->setTristate(true);

    auto *table = new QTableWidget(formats.size(), 2, this);
    table->setHorizontalHeaderLabels({tr("图片格式"), tr("关联")});
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setShowGrid(false);
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    m_checkboxes.reserve(formats.size());
    m_extensions.reserve(formats.size());
    for (int row = 0; row < formats.size(); ++row) {
        const ImageFormatDescriptor &format = formats.at(row);
        m_extensions << format.extension;

        auto *nameItem = new QTableWidgetItem(
            QStringLiteral("%1 (.%2)").arg(format.displayName, format.extension));
        nameItem->setFlags(Qt::ItemIsEnabled);
        table->setItem(row, 0, nameItem);

        auto *checkBox = new QCheckBox(this);
        checkBox->setChecked(FileAssociationService::isAssociated(format.extension));
        m_checkboxes << checkBox;

        auto *checkCell = new QWidget(this);
        auto *checkLayout = new QHBoxLayout(checkCell);
        checkLayout->setContentsMargins(0, 0, 0, 0);
        checkLayout->setAlignment(Qt::AlignCenter);
        checkLayout->addWidget(checkBox);
        table->setCellWidget(row, 1, checkCell);
    }

    auto updateSelectAllState = [this, selectAllCheckBox] {
        int checkedCount = 0;
        for (const QCheckBox *checkBox : m_checkboxes) {
            if (checkBox->isChecked()) {
                ++checkedCount;
            }
        }

        QSignalBlocker blocker(selectAllCheckBox);
        if (checkedCount == 0) {
            selectAllCheckBox->setCheckState(Qt::Unchecked);
        } else if (checkedCount == m_checkboxes.size()) {
            selectAllCheckBox->setCheckState(Qt::Checked);
        } else {
            selectAllCheckBox->setCheckState(Qt::PartiallyChecked);
        }
    };
    updateSelectAllState();

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    for (QCheckBox *checkBox : m_checkboxes) {
        connect(checkBox, &QCheckBox::checkStateChanged, this, updateSelectAllState);
    }
    connect(selectAllCheckBox,
            &QCheckBox::checkStateChanged,
            this,
            [this, selectAllCheckBox](Qt::CheckState state) {
                if (state == Qt::PartiallyChecked) {
                    return;
                }

                const bool checked = state == Qt::Checked;
                for (QCheckBox *checkBox : m_checkboxes) {
                    QSignalBlocker blocker(checkBox);
                    checkBox->setChecked(checked);
                }
                QSignalBlocker blocker(selectAllCheckBox);
                selectAllCheckBox->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
            });
#else
    for (QCheckBox *checkBox : m_checkboxes) {
        connect(checkBox, &QCheckBox::stateChanged, this, updateSelectAllState);
    }
    connect(selectAllCheckBox, &QCheckBox::stateChanged, this, [this, selectAllCheckBox](int state) {
        if (state == Qt::PartiallyChecked) {
            return;
        }

        const bool checked = state == Qt::Checked;
        for (QCheckBox *checkBox : m_checkboxes) {
            QSignalBlocker blocker(checkBox);
            checkBox->setChecked(checked);
        }
        QSignalBlocker blocker(selectAllCheckBox);
        selectAllCheckBox->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    });
#endif

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("确定"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("取消"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    layout->addWidget(selectAllCheckBox);
    layout->addWidget(table);
    layout->addWidget(buttons);
}

QStringList FileAssociationDialog::selectedExtensions() const
{
    QStringList extensions;
    for (int index = 0; index < m_checkboxes.size(); ++index) {
        if (m_checkboxes.at(index)->isChecked()) {
            extensions << m_extensions.at(index);
        }
    }
    return extensions;
}
