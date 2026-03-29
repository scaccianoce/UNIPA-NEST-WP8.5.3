
#include <QString>
#include <QRegularExpression>
#include <QMap>
#include <QVector>
#include <QDebug>
#include <QList>

int main() {
    QString content = R"(
Schedule:Compact,
    On,                      !- Name
    Fraction,                !- Schedule Type Limits Name
    Through: 12/31,          !- Field 1
    For: AllDays,            !- Field 2
    Until: 24:00, 1.0;       !- Field 3
    
Schedule:Compact,
    Heating Setpoint,        !- Name
    Temperature,             !- Schedule Type Limits Name
    Through: 04/15,          !- Field 1
    For: Weekdays,           !- Field 2
    Until: 06:00, 15.0,      !- Field 3
    Until: 22:00, 20.0,      !- Field 4
    Until: 24:00, 15.0,      !- Field 5
    For: Weekends,           !- Field 6
    Until: 24:00, 15.0,      !- Field 7
    Through: 10/15,          !- Field 8
    For: AllDays,            !- Field 9
    Until: 24:00, 15.0,      !- Field 10
    Through: 12/31,          !- Field 11
    For: Weekdays,           !- Field 12
    Until: 06:00, 15.0,      !- Field 13
    Until: 22:00, 20.0,      !- Field 14
    Until: 24:00, 15.0,      !- Field 15
    For: Weekends,           !- Field 16
    Until: 24:00, 15.0;      !- Field 17
)";

    int daysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}; 

    QRegularExpression scheduleRegex("Schedule:Compact\\s*,\\s*([^,;!]+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression throughRegex("Through:\\s*(\\d+)/(\\d+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression forRegex("For:\\s*([^,;!]+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression untilRegex("Until:\\s*(\\d+):(\\d+)\\s*,\\s*([\\d\\.\\-]+)", QRegularExpression::CaseInsensitiveOption);

    auto it = scheduleRegex.globalMatch(content);
    while (it.hasNext()) {
        auto match = it.next();
        QString scheduleName = match.captured(1).trimmed();
        qDebug() << "Processing Schedule:" << scheduleName;
        int startPos = match.capturedStart();
        int endPos = content.indexOf(';', startPos);
        if (endPos == -1) continue;
        QString scheduleBlock = content.mid(startPos, endPos - startPos + 1);
        
        // Rimuove commenti ! prima di splittare
        QString blockNoComments;
        QStringList linesRaw = scheduleBlock.split('\n');
        for(QString l : linesRaw) {
            int commentIdx = l.indexOf('!');
            if(commentIdx != -1) l = l.left(commentIdx);
            blockNoComments += l + " ";
        }

        QStringList lines = blockNoComments.split(QRegularExpression("[\\n\\r,;]+"), Qt::SkipEmptyParts);
        
        struct Period {
            int endDay;
            QMap<QString, QList<QPair<int, double>>> dayProfiles;
        };
        QList<Period> periods;
        Period* currentPeriod = nullptr;
        QStringList currentDayTypes;
        
        for (QString line : lines) {
            line = line.trimmed();
            if (line.isEmpty()) continue;

            auto tMatch = throughRegex.match(line);
            if (tMatch.hasMatch()) {
                int month = tMatch.captured(1).toInt();
                int day = tMatch.captured(2).toInt();
                int daysUntilMonth = 0;
                for(int m=1; m < month; ++m) daysUntilMonth += daysInMonth[m];
                Period p;
                p.endDay = daysUntilMonth + day;
                periods.append(p);
                currentPeriod = &periods.last();
                currentDayTypes.clear();
                qDebug() << "  Through:" << month << "/" << day << " (Day" << p.endDay << ")";
                continue;
            }

            auto fMatch = forRegex.match(line);
            if (fMatch.hasMatch() && currentPeriod) {
                currentDayTypes = fMatch.captured(1).split(' ', Qt::SkipEmptyParts);
                for(int k=0; k<currentDayTypes.size(); ++k) currentDayTypes[k] = currentDayTypes[k].toLower();
                qDebug() << "    For:" << currentDayTypes;
                continue;
            }

            auto uMatch = untilRegex.match(line);
            if (uMatch.hasMatch() && currentPeriod && !currentDayTypes.isEmpty()) {
                int h = uMatch.captured(1).toInt();
                double v = uMatch.captured(3).toDouble();
                for (const QString& dt : currentDayTypes) {
                    currentPeriod->dayProfiles[dt].append({h, v});
                }
                qDebug() << "      Until:" << h << " Value:" << v;
                continue;
            }
        }
    }

    return 0;
}
