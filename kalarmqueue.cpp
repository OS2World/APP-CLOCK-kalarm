/****************************************************************************
**
** KAlarmQueue, an alarm queue class
**
** Copyright (C) 2015 by KO Myung-Hun
** All rights reserved.
** Contact: KO Myung-Hun (komh@chollian.net)
**
** This file is part of K Alarm.
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

#include "kalarmqueue.h"

#ifdef CONFIG_QT5
#include <QSoundEffect>
#endif

KAlarmQueue::KAlarmQueue(QObject *parent) : QObject(parent)
{
    connect(&_timer, SIGNAL(timeout()), this, SLOT(timerTimeout()));
    _timer.start(1000); // 1 second timer
}

KAlarmQueue::~KAlarmQueue()
{

}

void KAlarmQueue::add(const KAlarmItemWidget *w)
{
    _alarmMap.insert(w, findNextAlarm(w, QDateTime(QDate::currentDate(),
                                                   w->startTime()), true));
}

void KAlarmQueue::remove(const KAlarmItemWidget *w)
{
    _alarmMap.remove(w);
}

void KAlarmQueue::modify(const KAlarmItemWidget *w)
{
    _alarmMap.insert(w, findNextAlarm(w, QDateTime(QDate::currentDate(),
                                                   w->startTime()), true));
}

QDateTime KAlarmQueue::findNextAlarm(const KAlarmItemWidget *w,
                                     const QDateTime &dt, bool inclusive)
{
    QDateTime current(QDateTime::currentDateTime());

    // Clear seconds and milli-seconds parts
    current.setTime(QTime(current.time().hour(), current.time().minute()));

    QDateTime nextAlarm(dt);

    if (w->alarmType() == KAlarmItemWidget::IntervalAlarm)
    {
        if (inclusive && nextAlarm >= current)
            return nextAlarm;

        while (nextAlarm <= current)
        {
            if (inclusive && nextAlarm == current)
                break;

            nextAlarm = nextAlarm.addSecs(w->intervalTime().hour() * 3600 +
                                          w->intervalTime().minute() * 60);
        }
    }
    else if (w->alarmType() == KAlarmItemWidget::WeeklyAlarm)
    {
        if (inclusive
                && w->isWeekDayEnabled(
                    w->numToWeekDay(nextAlarm.date().dayOfWeek()))
                && nextAlarm >= current)
            return nextAlarm;

        for (int days = 1; days <= 7; ++days)
        {
            int dayOfWeek = nextAlarm.addDays(days).date().dayOfWeek();

            if (w->isWeekDayEnabled(w->numToWeekDay(dayOfWeek)))
            {
                nextAlarm = nextAlarm.addDays(days);
                break;
            }
        }
    }

    return nextAlarm;
}

void KAlarmQueue::alarm(const KAlarmItemWidget *w, const QDateTime &dt)
{
#ifdef CONFIG_QT5
    QSoundEffect *sound = new QSoundEffect;
    if (w->playSound())
    {
        sound->setSource(QUrl::fromLocalFile(w->soundFile()));
        sound->setLoopCount(QSoundEffect::Infinite);
        sound->setVolume(1.0f);
        sound->play();
    }
#else
    QSound *sound = new QSound(w->soundFile());
    if (w->playSound())
    {
        sound->setLoops(-1);
        sound->play();
    }
#endif

    if (w->execProgram())
    {
        QProcess::startDetached(w->execProgramName() + " " +
                                w->execProgramParams());
    }

    if (!w->showAlarmWindow())
    {
        delete sound;

        return;
    }

    QString text;
    text.append("<p align=center>");

    text.append("<h1>");
    text.append(dt.toString("HH:mm"));
    text.append("</h1>");

    text.append("<h3>");
    text.append(w->name());
    text.append("</h3>");

    text.append("</p>");

    QMessageBox *msgBox = new QMessageBox;
    msgBox->setModal(false);
    msgBox->setAttribute(Qt::WA_DeleteOnClose);
    msgBox->setText(text);

    connect(msgBox, SIGNAL(destroyed()), sound, SLOT(deleteLater()));

    // Resize a message box, minimum width of 320
    QSpacerItem* hspacer = new QSpacerItem(320, 0,
                                           QSizePolicy::Minimum,
                                           QSizePolicy::Expanding);

    QGridLayout* layout = qobject_cast<QGridLayout *>(msgBox->layout());
    layout->addItem(hspacer, layout->rowCount(), 0, 1, layout->columnCount());

    msgBox->show();
    // Activate a message box.
    // If not activated, change the color of  a task bar entry.
    msgBox->activateWindow();
    // Ensure that a message box is stacked on top.
    msgBox->raise();
}

void KAlarmQueue::timerTimeout()
{
    QDateTime currentDateTime(QDateTime::currentDateTime());

    QMap<const KAlarmItemWidget *, QDateTime> bellMap;
    QMapIterator<const KAlarmItemWidget *, QDateTime> it(_alarmMap);

    while (it.hasNext())
    {
        it.next();

        const KAlarmItemWidget *w(it.key());
        QDateTime dt(it.value());

        if (dt.date() == currentDateTime.date()
                && dt.time().hour() == currentDateTime.time().hour()
                && dt.time().minute() == currentDateTime.time().minute())
        {
            bellMap.insert(w, dt);
        }
    }

    it = QMapIterator<const KAlarmItemWidget *, QDateTime>(bellMap);
    while (it.hasNext())
    {
        it.next();

        const KAlarmItemWidget *w(it.key());
        QDateTime dt(it.value());

        // Alarm if enabled
        if (w->isAlarmEnabled())
            alarm(w, currentDateTime);

        // Update alarm
        if (w->alarmType() == KAlarmItemWidget::SingleShotAlarm)
        {
            // Remove single-shot alarm to prevent from alarming repeately
            // until a minute is changed.
            remove(w);

            // Disable alarm if single-shot alarm
            // FIXEME: implement this without const_cast !!!
            const_cast<KAlarmItemWidget *>(w)->setAlarmEnabled(false);
        }
        else
            _alarmMap.insert(w, findNextAlarm(w, dt));
    }
}

