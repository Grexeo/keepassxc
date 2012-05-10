/*
 *  Copyright (C) 2010 Felix Geyer <debfx@fobos.de>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "TestKeePass1Reader.h"

#include <QtCore/QFile>
#include <QtTest/QTest>

#include "config-keepassx-tests.h"
#include "tests.h"
#include "core/Database.h"
#include "core/Entry.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "crypto/Crypto.h"
#include "format/KeePass1Reader.h"

void TestKeePass1Reader::initTestCase()
{
    Crypto::init();

    QString filename = QString(KEEPASSX_TEST_DATA_DIR).append("/basic.kdb");

    KeePass1Reader reader;
    m_db = reader.readDatabase(filename, "masterpw", QByteArray());
    QVERIFY(m_db);
    QVERIFY(!reader.hasError());
}

void TestKeePass1Reader::testBasic()
{
    QCOMPARE(m_db->rootGroup()->children().size(), 2);

    Group* group1 = m_db->rootGroup()->children().at(0);
    QCOMPARE(group1->name(), QString("Internet"));
    QCOMPARE(group1->children().size(), 2);
    QCOMPARE(group1->entries().size(), 2);
    QCOMPARE(group1->iconNumber(), 1);

    Entry* entry11 = group1->entries().at(0);
    QCOMPARE(entry11->title(), QString("Test entry"));
    QCOMPARE(entry11->iconNumber(), 1);
    QCOMPARE(entry11->username(), QString("I"));
    QCOMPARE(entry11->url(), QString("http://example.com/"));
    QCOMPARE(entry11->password(), QString("secretpassword"));
    QCOMPARE(entry11->notes(), QString("Lorem ipsum\ndolor sit amet"));
    QVERIFY(entry11->timeInfo().expires());
    QCOMPARE(entry11->timeInfo().expiryTime(), genDT(2012, 5, 9, 10, 32));
    QCOMPARE(entry11->attachments()->keys().size(), 1);
    QCOMPARE(entry11->attachments()->keys().first(), QString("attachment.txt"));
    QCOMPARE(entry11->attachments()->value("attachment.txt"), QByteArray("hello world\n"));

    Entry* entry12 = group1->entries().at(1);
    QCOMPARE(entry12->title(), QString(""));
    QCOMPARE(entry12->iconNumber(), 0);
    QCOMPARE(entry12->username(), QString(""));
    QCOMPARE(entry12->url(), QString(""));
    QCOMPARE(entry12->password(), QString(""));
    QCOMPARE(entry12->notes(), QString(""));
    QVERIFY(!entry12->timeInfo().expires());
    QCOMPARE(entry12->attachments()->keys().size(), 0);

    Group* group11 = group1->children().at(0);
    QCOMPARE(group11->name(), QString("Subgroup 1"));
    QCOMPARE(group11->children().size(), 1);

    Group* group111 = group11->children().at(0);
    QCOMPARE(group111->name(), QString("Unexpanded"));
    QCOMPARE(group111->children().size(), 1);

    Group* group1111 = group111->children().at(0);
    QCOMPARE(group1111->name(), QString("abc"));
    QCOMPARE(group1111->children().size(), 0);

    Group* group12 = group1->children().at(1);
    QCOMPARE(group12->name(), QString("Subgroup 2"));
    QCOMPARE(group12->children().size(), 0);

    Group* group2 = m_db->rootGroup()->children().at(1);
    QCOMPARE(group2->name(), QString("eMail"));
    QCOMPARE(group2->entries().size(), 1);
    QCOMPARE(group2->iconNumber(), 19);
}

void TestKeePass1Reader::testCustomIcons()
{
    QCOMPARE(m_db->metadata()->customIcons().size(), 1);

    Entry* entry = m_db->rootGroup()->children().at(1)->entries().first();

    QCOMPARE(entry->icon().width(), 16);
    QCOMPARE(entry->icon().height(), 16);

    for (int x = 0; x < 16; x++) {
        for (int y = 0; y < 16; y++) {
            QRgb rgb = entry->icon().pixel(x, y);
            QCOMPARE(qRed(rgb), 8);
            QCOMPARE(qGreen(rgb), 160);
            QCOMPARE(qBlue(rgb), 60);
        }
    }
}

void TestKeePass1Reader::testGroupExpanded()
{
    QCOMPARE(m_db->rootGroup()->children().at(0)->isExpanded(), true);
    QCOMPARE(m_db->rootGroup()->children().at(0)->children().at(0)->isExpanded(), true);
    QCOMPARE(m_db->rootGroup()->children().at(0)->children().at(0)->children().at(0)->isExpanded(),
             false);
}

void TestKeePass1Reader::testFileKey()
{
    QFETCH(QString, type);

    QString name = QString("FileKey").append(type);

    KeePass1Reader reader;

    QString dbFilename = QString("%1/%2.kdb").arg(QString(KEEPASSX_TEST_DATA_DIR), name);
    QString keyFilename = QString("%1/%2.key").arg(QString(KEEPASSX_TEST_DATA_DIR), name);

    QFile file(keyFilename);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QByteArray keyData = KeePass1Reader::readKeyfile(&file);
    QVERIFY(!keyData.isEmpty());

    Database* db = reader.readDatabase(dbFilename, QString(), keyData);
    QVERIFY(db);
    QVERIFY(!reader.hasError());
    QCOMPARE(db->rootGroup()->children().size(), 1);
    QCOMPARE(db->rootGroup()->children().at(0)->name(), name);

    delete db;
}

void TestKeePass1Reader::testFileKey_data()
{
    QTest::addColumn<QString>("type");
    QTest::newRow("Binary") << QString("Binary");
    QTest::newRow("Hex") << QString("Hex");
    QTest::newRow("Hashed") << QString("Hashed");
}

void TestKeePass1Reader::testCompositeKey()
{
    QString name = "CompositeKey";

    KeePass1Reader reader;

    QString dbFilename = QString("%1/%2.kdb").arg(QString(KEEPASSX_TEST_DATA_DIR), name);
    QString keyFilename = QString("%1/FileKeyHex.key").arg(QString(KEEPASSX_TEST_DATA_DIR));

    QFile file(keyFilename);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QByteArray keyData = KeePass1Reader::readKeyfile(&file);
    QVERIFY(!keyData.isEmpty());

    Database* db = reader.readDatabase(dbFilename, "mypassword", keyData);
    QVERIFY(db);
    QVERIFY(!reader.hasError());
    QCOMPARE(db->rootGroup()->children().size(), 1);
    QCOMPARE(db->rootGroup()->children().at(0)->name(), name);

    delete db;
}

void TestKeePass1Reader::testTwofish()
{
    QString name = "Twofish";

    KeePass1Reader reader;

    QString dbFilename = QString("%1/%2.kdb").arg(QString(KEEPASSX_TEST_DATA_DIR), name);

    Database* db = reader.readDatabase(dbFilename, "masterpw", QByteArray());
    QVERIFY(db);
    QVERIFY(!reader.hasError());
    QCOMPARE(db->rootGroup()->children().size(), 1);
    QCOMPARE(db->rootGroup()->children().at(0)->name(), name);

    delete db;
}

void TestKeePass1Reader::cleanupTestCase()
{
    delete m_db;
}

QDateTime TestKeePass1Reader::genDT(int year, int month, int day, int hour, int min)
{
    QDate date(year, month, day);
    QTime time(hour, min, 0);
    return QDateTime(date, time, Qt::UTC);
}

KEEPASSX_QTEST_CORE_MAIN(TestKeePass1Reader)