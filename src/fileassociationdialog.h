#ifndef FILEASSOCIATIONDIALOG_H
#define FILEASSOCIATIONDIALOG_H

#include <QDialog>
#include <QList>
#include <QStringList>

class QCheckBox;

class FileAssociationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FileAssociationDialog(QWidget *parent = nullptr);

    QStringList selectedExtensions() const;

private:
    QList<QCheckBox *> m_checkboxes;
    QStringList m_extensions;
};

#endif // FILEASSOCIATIONDIALOG_H
