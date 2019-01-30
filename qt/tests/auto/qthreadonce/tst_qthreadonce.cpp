/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/


#include <QtTest/QtTest>

#include <qcoreapplication.h>
#include <qmutex.h>
#include <qthread.h>
#include <qwaitcondition.h>
#include "qthreadonce.h"

//TESTED_CLASS=
//TESTED_FILES=corelib/thread/qthreadonce.h corelib/thread/qthreadonce.cpp

class tst_QThreadOnce : public QObject
{
    Q_OBJECT

private slots:
    void sameThread();
    void sameThread_data();
    void multipleThreads();

    void nesting();
    void reentering();
    void exception();
};

class SingletonObject: public QObject
{
    Q_OBJECT
public:
    static int runCount;
    SingletonObject() { val = 42; ++runCount; }
    ~SingletonObject() { }

    QBasicAtomicInt val;
};

class IncrementThread: public QThread
{
public:
    static QBasicAtomicInt runCount;
    static QSingleton<SingletonObject> singleton;
    QSemaphore &sem1, &sem2;
    int &var;

    IncrementThread(QSemaphore *psem1, QSemaphore *psem2, int *pvar, QObject *parent)
        : QThread(parent), sem1(*psem1), sem2(*psem2), var(*pvar)
        { start(); }

    ~IncrementThread() { wait(); }

protected:
    void run()
        {
            sem2.release();
            sem1.acquire();             // synchronize

            Q_ONCE {
                ++var;
            }
            runCount.ref();
            singleton->val.ref();
        }
};
int SingletonObject::runCount = 0;
QBasicAtomicInt IncrementThread::runCount = Q_BASIC_ATOMIC_INITIALIZER(0);
QSingleton<SingletonObject> IncrementThread::singleton;

void tst_QThreadOnce::sameThread_data()
{
    SingletonObject::runCount = 0;
    QTest::addColumn<int>("expectedValue");

    QTest::newRow("first") << 42;
    QTest::newRow("second") << 43;
}

void tst_QThreadOnce::sameThread()
{
    static int controlVariable = 0;
    Q_ONCE {
        QCOMPARE(controlVariable, 0);
        ++controlVariable;
    }
    QCOMPARE(controlVariable, 1);

    static QSingleton<SingletonObject> s;
    QTEST((int)s->val, "expectedValue");
    s->val.ref();

    QCOMPARE(SingletonObject::runCount, 1);
}

void tst_QThreadOnce::multipleThreads()
{
#if defined(Q_OS_WINCE) || defined(Q_OS_VXWORKS) || defined(Q_OS_SYMBIAN)
    const int NumberOfThreads = 20;
#else
    const int NumberOfThreads = 100;
#endif
    int controlVariable = 0;
    QSemaphore sem1, sem2(NumberOfThreads);

    QObject *parent = new QObject;
    for (int i = 0; i < NumberOfThreads; ++i)
        new IncrementThread(&sem1, &sem2, &controlVariable, parent);

    QCOMPARE(controlVariable, 0); // nothing must have set them yet
    SingletonObject::runCount = 0;
    IncrementThread::runCount = 0;

    // wait for all of them to be ready
    sem2.acquire(NumberOfThreads);
    // unleash the threads
    sem1.release(NumberOfThreads);

    // wait for all of them to terminate:
    delete parent;

    QCOMPARE(controlVariable, 1);
    QCOMPARE((int)IncrementThread::runCount, NumberOfThreads);
    QCOMPARE(SingletonObject::runCount, 1);
}

void tst_QThreadOnce::nesting()
{
    int variable = 0;
    Q_ONCE {
        Q_ONCE {
            ++variable;
        }
    }

    QVERIFY(variable == 1);
}

static void reentrant(int control, int &counter)
{
    Q_ONCE {
        if (counter)
            reentrant(--control, counter);
        ++counter;
    }
    static QSingleton<SingletonObject> s;
    s->val.ref();
}

void tst_QThreadOnce::reentering()
{
    const int WantedRecursions = 5;
    int count = 0;
    SingletonObject::runCount = 0;
    reentrant(WantedRecursions, count);

    // reentrancy is undefined behavior:
    QVERIFY(count == 1 || count == WantedRecursions);
    QCOMPARE(SingletonObject::runCount, 1);
}

#if !defined(QT_NO_EXCEPTIONS)
static void exception_helper(int &val)
{
    Q_ONCE {
        if (val++ == 0) throw 0;
    }
}
#endif

void tst_QThreadOnce::exception()
{
#if defined(QT_NO_EXCEPTIONS)
    QSKIP("Compiled without exceptions, skipping test", SkipAll);
#else
    int count = 0;

    try {
        exception_helper(count);
    } catch (...) {
        // nothing
    }
    QCOMPARE(count, 1);

    try {
        exception_helper(count);
    } catch (...) {
        QVERIFY2(false, "Exception shouldn't have been thrown...");
    }
    QCOMPARE(count, 2);
#endif
}

QTEST_MAIN(tst_QThreadOnce)
#include "tst_qthreadonce.moc"
