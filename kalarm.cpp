/****************************************************************************
**
** KAlarm, main class of K Alarm
**
** Copyright (C) 2015 by KO Myung-Hun
** All rights reserved.
** Contact: KO Myung-Hun (komh@chollian.net)
**
** This file is part of K Alarm
**
** $BEGIN_LICENSE$
**
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** $END_LICENSE$
**
****************************************************************************/

#include "kalarm.h"
#include "ui_kalarm.h"

#include <QtCore>
#ifdef CONFIG_QT5
#include <QtWidgets>
#else
#include <QtGui>
#endif

#include "kalarmconfigdialog.h"
#include "kalarmitemwidget.h"

KAlarm::KAlarm(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::KAlarm)
{
    ui->setupUi(this);

    _fileMenu = menuBar()->addMenu(tr("&File"));
    _fileMenu->addAction(tr("&New alarm..."), this, SLOT(addItem()),
                         QKeySequence::New);
    _fileMenu->addSeparator();
    _fileMenu->addAction(tr("E&xit"), qApp, SLOT(quit()),
                         QKeySequence(tr("Ctrl+Q")));

    _viewMenu = menuBar()->addMenu(tr("&View"));
    _showKAlarmAction = _viewMenu->addAction(tr("&Show K Alarm at startup"),
                                             this,
                                             SLOT(showKAlarmTriggered(bool)));
    _showKAlarmAction->setCheckable(true);

    _helpMenu = menuBar()->addMenu(tr("&Help"));
    _helpMenu->addAction(tr("&About %1...").arg(title()), this, SLOT(about()));
    _helpMenu->addAction(tr("About &Qt..."), this, SLOT(aboutQt()));

    QPushButton *addButton = new QPushButton(tr("&Add"));
    QPushButton *modifyButton = new  QPushButton(tr("&Modify"));
    QPushButton *deleteButton = new QPushButton(tr("&Delete"));

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(addButton);
    buttonLayout->addWidget(modifyButton);
    buttonLayout->addWidget(deleteButton);

    _listWidget = new QListWidget;

    QVBoxLayout *vlayout = new QVBoxLayout;
    vlayout->addWidget(_listWidget);
    vlayout->addLayout(buttonLayout);

    centralWidget()->setLayout(vlayout);

    // hide a toolbar and a status bar
    ui->mainToolBar->hide();
    statusBar()->hide();

    _trayIconMenu = new QMenu;
    _trayIconMenu->addAction(tr("&Open %1...").arg(title()),
                             this, SLOT(openKAlarm()));
    _trayIconMenu->addMenu(_helpMenu);
    _trayIconMenu->addSeparator();
    _trayIconMenu->addAction(tr("E&xit"), qApp, SLOT(quit()));

    _trayIcon = new QSystemTrayIcon;
    _trayIcon->setToolTip(title());
    _trayIcon->setContextMenu(_trayIconMenu);
    _trayIcon->setIcon(QIcon(":/icons/candy_clock_16.png"));

    connect(addButton, SIGNAL(clicked()), this, SLOT(addItem()));
    connect(modifyButton, SIGNAL(clicked()), this, SLOT(modifyItem()));
    connect(deleteButton, SIGNAL(clicked()), this, SLOT(deleteItem()));
    connect(_listWidget, SIGNAL(doubleClicked(QModelIndex)),
            this, SLOT(modifyItem(QModelIndex)));
    connect(_trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));

    _trayIcon->show();

    loadAlarmItems();

    if (_showKAlarmAction->isChecked())
        show();
    else
    {
        // Prevent from qutting K Alarm when K Alarm is hidden and an alarm
        // window is closed.
        QApplication::setQuitOnLastWindowClosed(false);

        if (QSystemTrayIcon::isSystemTrayAvailable())
            hide();
        else
            showMinimized();
    }
}

KAlarm::~KAlarm()
{
    delete _trayIcon;

    saveAlarmItems();

    delete ui;
}

bool KAlarm::event(QEvent *e)
{
    if (e->type() == QEvent::KeyPress)
    {
        QKeyEvent *key = reinterpret_cast<QKeyEvent *>(e);

        if (!(key->modifiers() & (Qt::ControlModifier | Qt::AltModifier)))
        {
            switch (key->key())
            {
                case Qt::Key_Insert:
                    addItem();
                    e->accept();
                    return true;

                case Qt::Key_Delete:
                    deleteItem();
                    e->accept();
                    return true;
            }
        }
    }

    return QMainWindow::event(e);
}

void KAlarm::closeEvent(QCloseEvent *e)
{
    if (QSystemTrayIcon::isSystemTrayAvailable())
    {
        // Prevent K Alarm from quitting when an alarm window opened after
        // a main window was hidden, is closed.
        QApplication::setQuitOnLastWindowClosed(false);

        hide();
        e->ignore();

        return;
    }

    // If all visible window are closed, then quit K Alarm.
    QApplication::setQuitOnLastWindowClosed(true);

    QMainWindow::closeEvent(e);
}

static void configDialogToItemWidget(const KAlarmConfigDialog &configDialog,
                                     KAlarmItemWidget *itemWidget)
{
    itemWidget->setName(configDialog.name());
    itemWidget->setStartTime(QTime(configDialog.startTime().hour(),
                                   configDialog.startTime().minute()));
    itemWidget->setIntervalTime(QTime(configDialog.intervalTime().hour(),
                                      configDialog.intervalTime().minute()));

    itemWidget->setWeekDayEnabled(KAlarmItemWidget::Monday,
                                  configDialog.isMondayChecked());

    itemWidget->setWeekDayEnabled(KAlarmItemWidget::Tuesday,
                                  configDialog.isTuesdayChecked());

    itemWidget->setWeekDayEnabled(KAlarmItemWidget::Wednesday,
                                  configDialog.isWednesdayChecked());

    itemWidget->setWeekDayEnabled(KAlarmItemWidget::Thursday,
                                  configDialog.isThursdayChecked());

    itemWidget->setWeekDayEnabled(KAlarmItemWidget::Friday,
                                  configDialog.isFridayChecked());

    itemWidget->setWeekDayEnabled(KAlarmItemWidget::Saturday,
                                  configDialog.isSaturdayChecked());

    itemWidget->setWeekDayEnabled(KAlarmItemWidget::Sunday,
                                  configDialog.isSundayChecked());

    KAlarmItemWidget::KAlarmType alarmType;

    if (configDialog.isUseIntervalChecked())
        alarmType = KAlarmItemWidget::IntervalAlarm;
    else if (itemWidget->weekDaysToString().isEmpty())
        alarmType = KAlarmItemWidget::SingleShotAlarm;
    else
        alarmType = KAlarmItemWidget::WeeklyAlarm;

    itemWidget->setAlarmType(alarmType);

    itemWidget->setShowAlarmWindow(configDialog.isShowAlarmWindowChecked());
    itemWidget->setPlaySound(configDialog.isPlaySoundChecked());
    itemWidget->setSoundFile(configDialog.soundFile());

    itemWidget->setExecProgram(configDialog.isExecProgramChecked());
    itemWidget->setExecProgramName(configDialog.execProgramName());
    itemWidget->setExecProgramParams(configDialog.execProgramParams());
}

void KAlarm::addItem()
{
    KAlarmConfigDialog configDialog(this);

    if (configDialog.exec() == QDialog::Accepted)
    {
        KAlarmItemWidget *itemWidget = new KAlarmItemWidget;

        configDialogToItemWidget(configDialog, itemWidget);

        itemWidget->setAlarmEnabled(true);

        QListWidgetItem *item = new QListWidgetItem;
        item->setSizeHint(QSize(itemWidget->sizeHint()));

        _listWidget->addItem(item);
        _listWidget->setItemWidget(item, itemWidget);

        _alarmQueue.add(itemWidget);

        connect(itemWidget, SIGNAL(alarmEnabledToggled(bool)),
                this, SLOT(itemWidgetAlarmEnabledToggled(bool)));

        saveAlarmItems();
    }
}

void KAlarm::modifyItem(const QModelIndex &index)
{
    KAlarmConfigDialog configDialog(this);

    QListWidgetItem *item;

    if (index.isValid())
        item = _listWidget->item(index.row());
    else
        item = _listWidget->currentItem();

    if (!item)
        return;

    KAlarmItemWidget *itemWidget =
            qobject_cast<KAlarmItemWidget *>(_listWidget->itemWidget(item));
    configDialog.setName(itemWidget->name());
    configDialog.setStartTime(itemWidget->startTime());
    configDialog.setUseIntervalChecked(itemWidget->alarmType()
                                        ==  KAlarmItemWidget::IntervalAlarm);
    configDialog.setIntervalTime(itemWidget->intervalTime());
    configDialog.setMondayChecked(
                itemWidget->isWeekDayEnabled(KAlarmItemWidget::Monday));
    configDialog.setTuesdayChecked(
                itemWidget->isWeekDayEnabled(KAlarmItemWidget::Tuesday));
    configDialog.setWednesdayChecked(
                itemWidget->isWeekDayEnabled(KAlarmItemWidget::Wednesday));
    configDialog.setThursdayChecked(
                itemWidget->isWeekDayEnabled(KAlarmItemWidget::Thursday));
    configDialog.setFridayChecked(
                itemWidget->isWeekDayEnabled(KAlarmItemWidget::Friday));
    configDialog.setSaturdayChecked(
                itemWidget->isWeekDayEnabled(KAlarmItemWidget::Saturday));
    configDialog.setSundayChecked(
                itemWidget->isWeekDayEnabled(KAlarmItemWidget::Sunday));
    configDialog.setShowAlarmWindowChecked(itemWidget->showAlarmWindow());
    configDialog.setPlaySoundChecked(itemWidget->playSound());
    configDialog.setSoundFile(itemWidget->soundFile());
    configDialog.setExecProgramChecked(itemWidget->execProgram());
    configDialog.setExecProgramName(itemWidget->execProgramName());
    configDialog.setExecProgramParams(itemWidget->execProgramParams());

    if (configDialog.exec() == QDialog::Accepted)
    {
        configDialogToItemWidget(configDialog, itemWidget);

        _alarmQueue.modify(itemWidget);

        saveAlarmItems();
    }
}

void KAlarm::deleteItem()
{
    QListWidgetItem *item = _listWidget->currentItem();
    if (item)
    {
        KAlarmItemWidget *w = qobject_cast<KAlarmItemWidget *>
                                (_listWidget->itemWidget(item));

        // Remove a item widget from alarm queue
        _alarmQueue.remove(w);

        // Dissociate a item widget from a list widget item
        _listWidget->removeItemWidget(item);

        // Disconnect signal
        w->disconnect();

        // Destroy a item widget
        delete w;

        // Destroy a list widget item
        delete item;

        saveAlarmItems();
    }
}

void KAlarm::itemWidgetAlarmEnabledToggled(bool enabled)
{
    if (sender())
    {
        // Update alarm if signalled and enabled
        if (enabled)
            _alarmQueue.modify(qobject_cast<KAlarmItemWidget *>(sender()));

        saveAlarmItems();
    }
}

void KAlarm::showKAlarmTriggered(bool checked) const
{
    QSettings settings;

    settings.setValue("ShowKAlarm", checked);
}

void KAlarm::saveAlarmItems() const
{
    QSettings settings;

    settings.setValue("MainWindowGeometry", saveGeometry());

    int count = _listWidget->count();
    settings.setValue("AlarmCount", count);

    for (int i = 0; i < count; ++i)
    {
        qobject_cast<KAlarmItemWidget *>
                (_listWidget->itemWidget(_listWidget->item(i)))->saveAlarm(i);
    }
}

void KAlarm::loadAlarmItems()
{
    QSettings settings;

    restoreGeometry(settings.value("MainWindowGeometry").toByteArray());

    _showKAlarmAction->setChecked(settings.value("ShowKAlarm", true).toBool());

    int count = settings.value("AlarmCount").toInt();
    for (int i = 0; i < count; ++i)
    {
        KAlarmItemWidget *itemWidget = new KAlarmItemWidget;

        itemWidget->loadAlarm(i);

        QListWidgetItem *item = new QListWidgetItem;
        item->setSizeHint(QSize(itemWidget->sizeHint()));

        _listWidget->addItem(item);
        _listWidget->setItemWidget(item, itemWidget);

        _alarmQueue.add(itemWidget);

        connect(itemWidget, SIGNAL(alarmEnabledToggled(bool)),
                this, SLOT(itemWidgetAlarmEnabledToggled(bool)));
    }
}

void KAlarm::about()
{
    QMessageBox::about( this, tr("About %1").arg(title()), tr(
"<h2>%1 %2</h2>"
"<p>Copyright &copy; 2015 by %3 "
"<a href=mailto:komh@chollian.net>&lt;komh@chollian.net&gt;</a>"
"<p>%1 is a program to alarm one time, at regular intervals or weekly."
"<p>If you want to promote to develop this program, then donate at the below "
"web page, please."
"<p align=center><a href=http://www.ecomstation.co.kr/komh/donate.html>"
"http://www.ecomstation.co.kr/komh/donate.html</a>"
"<p>This program comes with ABSOLUTELY NO WARRANTY. This is free software, "
"and you are welcome to redistribute it under certain conditions. See "
"<a href=http://www.gnu.org/licenses/gpl.html>the GPL v3 license</a> "
"for details."
                            ).arg(title()).arg(version()).arg(organization()));
}

void KAlarm::aboutQt()
{
    QMessageBox::aboutQt(this);
}

void KAlarm::openKAlarm()
{
    show();
    activateWindow();
    raise();
}

void KAlarm::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick)
        openKAlarm();
}
